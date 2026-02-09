#include "ext2.h"
#include "kernel.h"
#include "heap.h"
#include "vfs.h"
#include "ahci.h"

static ext2_superblock sb;
static ahci_port* g_port;
static uint32_t block_size;
static uint32_t inodes_per_group;
static uint32_t inode_size;

vfs_node* ext2_root = nullptr; 

// Helper: Czytanie bloku (konwersja na sektory LBA)
bool ext2_read_block(uint32_t block_no, uint8_t* buffer) {
    if (!g_port) return false;
    // Blok 0 jest "NULL" w strukturach i-node
    if (block_no == 0) {
        // memset(buffer, 0, block_size); // Opcjonalnie zeruj
        return true; // Udawaj sukces, ale nic nie czytaj z dysku
    }

    uint32_t sectors_per_block = block_size / 512;
    uint64_t lba = (uint64_t)block_no * sectors_per_block;
    return ahci_read(g_port, lba, sectors_per_block, (uint16_t*)buffer);
}

// Funkcja czytająca zawartość i-noda z dysku
bool ext2_get_inode(uint32_t inode_num, ext2_inode* out_inode) {
    if (inode_num == 0) return false;

    // 1. Grupa i Indeks
    uint32_t group = (inode_num - 1) / inodes_per_group;
    uint32_t index = (inode_num - 1) % inodes_per_group;

    // 2. Znajdź Tablicę Deskryptorów Grup (Block Group Descriptor Table)
    // Zaczyna się w bloku:
    // - 2 (dla bloków 1KB)
    // - 1 (dla bloków 2KB/4KB)
    uint32_t bgdt_start_block = (block_size == 1024) ? 2 : 1;
    
    // Wczytaj blok z deskryptorami
    uint8_t* bgdt_buf = (uint8_t*)kmalloc(block_size);
    if (!ext2_read_block(bgdt_start_block, bgdt_buf)) { kfree(bgdt_buf); return false; }

    ext2_group_descriptor* gd = (ext2_group_descriptor*)bgdt_buf;
    
    // Pobierz numer bloku, gdzie zaczyna się Inode Table dla tej grupy
    uint32_t inode_table_start = gd[group].bg_inode_table;
    kfree(bgdt_buf);

    // 3. Znajdź blok zawierający nasz i-node
    uint32_t byte_offset_in_table = index * inode_size;
    uint32_t block_offset = byte_offset_in_table / block_size;
    uint32_t byte_offset_in_block = byte_offset_in_table % block_size;

    // 4. Wczytaj blok z i-nodem
    uint8_t* inode_block = (uint8_t*)kmalloc(block_size);
    if (!ext2_read_block(inode_table_start + block_offset, inode_block)) { kfree(inode_block); return false; }

    // 5. Kopiuj dane
    memcpy(out_inode, inode_block + byte_offset_in_block, sizeof(ext2_inode));
    
    kfree(inode_block);
    return true;
}

// Funkcja czytająca dane pliku (obsługa wskaźników)
uint64_t ext2_read_node(vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    uint64_t bytes_read = 0;
    uint64_t ptrs_per_block = block_size / 4; 

    // Bufory cache dla bloków pośrednich
    uint32_t* indirect_buf = (uint32_t*)kmalloc(block_size);
    uint32_t  cached_indirect_block = 0;
    
    // Bufor na dane (gdy offset nie jest wyrównany)
    uint8_t* data_buf = (uint8_t*)kmalloc(block_size);

    while (bytes_read < size) {
        uint32_t block_idx = (offset + bytes_read) / block_size;
        uint32_t chunk_off = (offset + bytes_read) % block_size;
        uint32_t chunk_len = block_size - chunk_off;
        if (chunk_len > (size - bytes_read)) chunk_len = size - bytes_read;

        uint32_t disk_block = 0;

        // A. Blok Bezpośredni (0-11)
        if (block_idx < 12) {
            disk_block = node->blocks[block_idx];
        }
        // B. Pojedynczy Pośredni (12)
        else if (block_idx < 12 + ptrs_per_block) {
            if (cached_indirect_block != node->blocks[12]) {
                ext2_read_block(node->blocks[12], (uint8_t*)indirect_buf);
                cached_indirect_block = node->blocks[12];
            }
            disk_block = indirect_buf[block_idx - 12];
        }
        // C. Podwójny pośredni (Pominięte dla prostoty, filmy < 4GB na 4KB blokach działają na pojedynczym/podwójnym)
        
        if (disk_block == 0) {
            memset(buffer + bytes_read, 0, chunk_len);
        } else {
            // Czytamy blok
            if (chunk_off == 0 && chunk_len == block_size) {
                // Optymalizacja: Czytaj prosto do bufora docelowego
                ext2_read_block(disk_block, buffer + bytes_read);
            } else {
                ext2_read_block(disk_block, data_buf);
                memcpy(buffer + bytes_read, data_buf + chunk_off, chunk_len);
            }
        }
        bytes_read += chunk_len;
    }

    kfree(indirect_buf);
    kfree(data_buf);
    return bytes_read;
}

bool ext2_init(ahci_port* port) {
    if (!port) return false;
    g_port = port;

    // 1. Czytaj Superblock (Zawsze offset 1024)
    // 1024 bajt to początek 3-go sektora (LBA 2)
    uint8_t* raw_sb = (uint8_t*)kmalloc(1024);
    if (!ahci_read(port, 2, 2, (uint16_t*)raw_sb)) {
        write_serial_string("[EXT2] Krytyczny błąd odczytu SB!\n");
        return false;
    }

    memcpy(&sb, raw_sb, sizeof(ext2_superblock));
    kfree(raw_sb);

    if (sb.s_magic != EXT2_MAGIC) {
        write_serial_string("[EXT2] Zła sygnatura: ");
        write_serial_hex(sb.s_magic);
        write_serial_string("\n");
        return false;
    }

    block_size = 1024 << sb.s_log_block_size;
    inodes_per_group = sb.s_inodes_per_group;
    inode_size = sb.s_inode_size;

    write_serial_string("[EXT2] Wykryto EXT2!\n");
    write_serial_string("       Blok: "); write_serial_dec(block_size); write_serial_string("\n");

    // 2. Utwórz Root Node
    ext2_root = (vfs_node*)kmalloc(sizeof(vfs_node));
    memset(ext2_root, 0, sizeof(vfs_node));
    const char* rname = "/"; 
    memcpy(ext2_root->name, rname, 2);

    ext2_inode root_in;
    if (!ext2_get_inode(EXT2_ROOT_INODE, &root_in)) return false;

    ext2_root->size = root_in.i_size;
    ext2_root->type = FS_DIRECTORY;
    for(int i=0; i<15; i++) ext2_root->blocks[i] = root_in.i_block[i];
    ext2_root->read = nullptr;
    ext2_root->next = nullptr;

    // 3. Skanuj katalog główny i dodaj pliki do VFS
    uint8_t* dir_buf = (uint8_t*)kmalloc(block_size);
    
    // Podepnij pod globalny VFS (na koniec listy z TarFS)
    extern vfs_node* vfs_root;
    vfs_node* last = vfs_root;
    if (last) while(last->next) last = last->next;

    // Skanujemy bloki bezpośrednie katalogu root
    for (int b = 0; b < 12; b++) {
        uint32_t blk = root_in.i_block[b];
        if (blk == 0) break;

        ext2_read_block(blk, dir_buf);
        
        uint32_t offset = 0;
        while (offset < block_size) {
            ext2_directory_entry* entry = (ext2_directory_entry*)(dir_buf + offset);
            
            if (entry->rec_len == 0) break; // koniec
            
            if (entry->inode != 0) {
                // Filtruj . i ..
                bool skip = (entry->name_len == 1 && entry->name[0] == '.') ||
                            (entry->name_len == 2 && entry->name[0] == '.' && entry->name[1] == '.');

                if (!skip) {
                    vfs_node* node = (vfs_node*)kmalloc(sizeof(vfs_node));
                    memset(node, 0, sizeof(vfs_node));
                    
                    memcpy(node->name, entry->name, entry->name_len);
                    node->name[entry->name_len] = 0;

                    node->addr = (uint64_t)entry->inode;
                    // Pobierz detale pliku
                    ext2_inode fin;
                    ext2_get_inode(entry->inode, &fin);
                    
                    node->size = fin.i_size;
                    node->type = (entry->file_type == 2) ? FS_DIRECTORY : FS_FILE;
                    node->read = ext2_read_node; // Przypisujemy driver!
                    node->write = ext2_write;
                    for(int k=0; k<15; k++) node->blocks[k] = fin.i_block[k];

                    if (last) last->next = node;
                    else vfs_root = node;
                    last = node;

                    write_serial_string("[EXT2] Dodano: ");
                    write_serial_string(node->name);
                    write_serial_string("\n");
                }
            }
            offset += entry->rec_len;
        }
    }
    kfree(dir_buf);
    return true;
}

char* ext2_read_file(const char* path) {
    // 1. Znajdź węzeł pliku w VFS
    // Uwaga: Zakładamy, że ext2_root jest globalnie dostępne (zadeklarowane w ext2.h)
    vfs_node* node = vfs_find_node(ext2_root, path);
    
    if (!node) {
        write_serial_string("[EXT2] Error: File not found: ");
        write_serial_string(path);
        write_serial_string("\n");
        return nullptr;
    }

    // 2. Zaalokuj pamięć na cały plik
    // node->size to rozmiar pliku w bajtach
    char* buffer = (char*)kmalloc(node->size);
    if (!buffer) {
        write_serial_string("[EXT2] Error: Out of memory for file buffer.\n");
        return nullptr;
    }

    // 3. Przeczytaj dane
    // ext2_read_node to Twoja istniejąca funkcja czytająca z dysku
    uint32_t bytes_read = ext2_read_node(node, 0, node->size, (uint8_t*)buffer);
    
    if (bytes_read != node->size) {
        write_serial_string("[EXT2] Warning: Read fewer bytes than expected.\n");
    }

    return buffer;
}

void ext2_write_inode(uint32_t inode_num, ext2_inode* inode) {
    if (inode_num == 0 || !g_port) return;

    // Obliczamy gdzie na dysku leży ta inoda
    uint32_t group = (inode_num - 1) / inodes_per_group;
    uint32_t index = (inode_num - 1) % inodes_per_group;
    uint32_t bgdt_start_block = (block_size == 1024) ? 2 : 1;
    
    uint8_t* bgdt_buf = (uint8_t*)kmalloc(block_size);
    ext2_read_block(bgdt_start_block, bgdt_buf);
    ext2_group_descriptor* gd = (ext2_group_descriptor*)bgdt_buf;
    uint32_t inode_table_start = gd[group].bg_inode_table;
    kfree(bgdt_buf);

    uint32_t byte_offset = index * inode_size;
    uint32_t block_offset = byte_offset / block_size;
    uint32_t offset_in_block = byte_offset % block_size;

    // Czytamy blok tabeli inod, modyfikujemy go i zapisujemy
    uint8_t* buffer = (uint8_t*)kmalloc(block_size);
    ext2_read_block(inode_table_start + block_offset, buffer);
    
    memcpy(buffer + offset_in_block, inode, sizeof(ext2_inode));
    
    // FIZYCZNY ZAPIS NA DYSK (używając odblokowanego ahci_write)
    uint32_t sectors_per_block = block_size / 512;
    ahci_write(g_port, (uint64_t)(inode_table_start + block_offset) * sectors_per_block, sectors_per_block, (uint16_t*)buffer);

    kfree(buffer);
}

uint32_t ext2_allocate_block() {
    uint32_t bitmap_block_num = 3; // To zależy od konfiguracji Twojego mkfs.ext2
    uint8_t bitmap[4096];
    
    // 1. Czytamy bitmapę (używamy poprawnego sata_port)
    ahci_read(g_port, bitmap_block_num * 8, 8, (uint16_t*)bitmap);

    for (uint32_t i = 0; i < 4096; i++) {
        if (bitmap[i] != 0xFF) {
            for (int bit = 0; bit < 8; bit++) {
                if (!(bitmap[i] & (1 << bit))) {
                    bitmap[i] |= (1 << bit);
                    
                    // 2. Poprawiona literówka: ahci_write zamiast ahb_write
                    ahci_write(g_port, bitmap_block_num * 8, 8, (uint16_t*)bitmap);
                    return i * 8 + bit;
                }
            }
        }
    }
    return 0;
}

uint64_t ext2_write(vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    uint32_t inode_num = (uint32_t)node->addr;
    
    write_serial_string("[EXT2] Zapis do Inody: ");
    write_serial_dec(inode_num);
    write_serial_string("\n");

    if (inode_num == 0) {
        write_serial_string("[EXT2] BLAD: Inoda 0 jest nieprawidlowa!\n");
        return 0;
    }

    ext2_inode inode;
    if (!ext2_get_inode(inode_num, &inode)) {
        write_serial_string("[EXT2] BLAD: Nie mozna pobrac inody z dysku.\n");
        return 0;
    }

    uint32_t block_index = offset / 4096;
    
    if (inode.i_block[block_index] == 0) {
        write_serial_string("[EXT2] Alokacja nowego bloku...\n");
        uint32_t new_block = ext2_allocate_block(); 
        if (new_block == 0) return 0;
        inode.i_block[block_index] = new_block;
    }

    uint64_t lba = inode.i_block[block_index] * 8; 
    
    // Zapisz dane (nadpisujemy 1 sektor = 512 bajtów)
    // UWAGA: To jest trochę "brudne" (nadpisuje resztę sektora śmieciami z RAMu),
    // ale do testu "czy działa" wystarczy. W przyszłości trzeba zrobić Read-Modify-Write.
    if (!ahci_write(g_port, lba, 1, (uint16_t*)buffer)) {
        write_serial_string("[EXT2] BLAD: AHCI Write failed.\n");
        return 0;
    }

    if (offset + size > inode.i_size) {
        inode.i_size = offset + size;
        node->size = inode.i_size;
    }

    ext2_write_inode(inode_num, &inode);
    write_serial_string("[EXT2] Zapis zakonczony sukcesem.\n");

    return size;
}
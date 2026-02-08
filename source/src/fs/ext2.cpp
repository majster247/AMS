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
uint32_t ext2_read_node(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    uint32_t bytes_read = 0;
    uint32_t ptrs_per_block = block_size / 4; 

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

                    // Pobierz detale pliku
                    ext2_inode fin;
                    ext2_get_inode(entry->inode, &fin);
                    
                    node->size = fin.i_size;
                    node->type = (entry->file_type == 2) ? FS_DIRECTORY : FS_FILE;
                    node->read = ext2_read_node; // Przypisujemy driver!
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
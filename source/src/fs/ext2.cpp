#include "ext2.h"
#include "kernel.h"
#include "heap.h"
#include "kernel.h"
#include "vfs.h"

static ext2_superblock sb;
static ahci_port* g_port;
static uint32_t block_size;

static uint8_t* temp_block_buffer = nullptr;

bool ext2_read_block(uint32_t block_no, uint8_t* buffer) {
    if (!g_port) return false;
    
    // Rozmiar bloku EXT2 może być np. 1024, a sektora SATA 512.
    // Musimy obliczyć ile sektorów przeczytać.
    uint32_t sectors_per_block = block_size / 512;
    uint64_t lba = (uint64_t)block_no * sectors_per_block;
    
    return ahci_read(g_port, lba, sectors_per_block, (uint16_t*)buffer);
}

uint32_t ext2_read_node(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    uint32_t bytes_read = 0;
    // Używamy bufora tymczasowego na jeden blok
    static uint8_t debug_buf[1024]; 
    uint8_t* block_buf = debug_buf;

    while (bytes_read < size) {
        uint32_t current_pos = offset + bytes_read;
        uint32_t block_idx = current_pos / block_size;
        uint32_t pos_in_block = current_pos % block_size;

        if (block_idx >= 12) {
            // Tutaj kiedyś dodasz "Indirect Blocks", na razie starczy 12KB
            break; 
        }

        uint32_t disk_block = 0;

        if (block_idx < 12) {
            disk_block = node->blocks[block_idx];
        } else if (block_idx < 12 + (block_size / 4)) {
            // 1. Odczytaj blok pośredni (wskaźnik jest w blocks[12])
            uint32_t indirect_block_ptr = node->blocks[12];
            uint32_t* indirect_buf = (uint32_t*)kmalloc(block_size);

            ext2_read_block(indirect_block_ptr, (uint8_t*)indirect_buf);

            // 2. Wyciągnij numer bloku z tablicy
            uint32_t index_in_indirect = block_idx - 12;
            disk_block = indirect_buf[index_in_indirect];

            // kfree(indirect_buf); // Jeśli masz kfree
        } else {
            break; // Tu kiedyś wejdzie Double Indirect (blocks[13])
        }

                // 1. Odczytaj blok z dysku
                if (!ext2_read_block(disk_block, block_buf)) {
                    break; 
                }
            
                // 2. Skopiuj dane do bufora użytkownika
                uint32_t to_copy = block_size - pos_in_block;
                if (to_copy > size - bytes_read) {
                    to_copy = size - bytes_read;
                }
            
                memcpy(buffer + bytes_read, block_buf + pos_in_block, to_copy);
                bytes_read += to_copy;
            }

    // kfree(block_buf); // Jeśli masz kfree, to zwolnij!
    return bytes_read;
}

// Funkcja odczytująca konkretny i-node z dysku
bool ext2_read_inode(uint32_t inode_no, ext2_inode* inode_out) {
    uint8_t* local_buffer = (uint8_t*)kmalloc(4096); // Bufor do odczytu bloków

    // 1. Znajdź grupę, w której jest i-node
    uint32_t group = (inode_no - 1) / sb.s_inodes_per_group;
    uint32_t index = (inode_no - 1) % sb.s_inodes_per_group;

    // 2. Przeczytaj Deskryptor Grupy, żeby wiedzieć gdzie jest tabela i-nodów
    // Deskryptory zaczynają się w bloku po Superbloku (zazwyczaj blok 2 dla 1KB)
    ext2_group_descriptor gd;
    uint32_t gd_block = (block_size == 1024) ? 2 : 1;
    ext2_read_block(gd_block, local_buffer);
    
    // Pobieramy deskryptor odpowiedniej grupy
    ext2_group_descriptor* gds = (ext2_group_descriptor*)local_buffer;
    gd = gds[group];

    // 3. Oblicz pozycję i-noda w tabeli
    uint32_t inode_table_block = gd.bg_inode_table;
    uint32_t offset_in_table = index * sb.s_inode_size;
    uint32_t block_offset = offset_in_table / block_size;
    uint32_t final_offset = offset_in_table % block_size;

    // 4. Przeczytaj blok z i-nodem
    ext2_read_block(inode_table_block + block_offset, local_buffer);
    memcpy(inode_out, local_buffer + final_offset, sizeof(ext2_inode));
    
    return true;
}

bool ext2_init(ahci_port* port) {
    g_port = port;

    if (temp_block_buffer == nullptr) {
        temp_block_buffer = (uint8_t*)kmalloc(4096); // Bezpieczne 4KB
    }
    
    // Czytamy superblok do statycznego bufora
    if (!ahci_read(port, 2, 2, (uint16_t*)temp_block_buffer)) {
        write_serial_string("[EXT2] Blad odczytu!\n");
        return false;
    }
    
    memcpy(&sb, temp_block_buffer, sizeof(ext2_superblock));

    if (sb.s_magic != EXT2_MAGIC) {
        write_serial_string("[EXT2] Brak sygnatury na dysku.\n");
        return false;
    }

    block_size = 1024 << sb.s_log_block_size;
    
    write_serial_string("[EXT2] Superblok odczytany pomyślnie!\n");
    write_serial_string("[EXT2] Liczba i-nodów: ");
    write_serial_hex(sb.s_inodes_count);
    write_serial_string("\n[EXT2] Liczba bloków: ");
    write_serial_hex(sb.s_blocks_count);
    write_serial_string("\n[EXT2] Rozmiar bloku: ");
    write_serial_hex(block_size);
    write_serial_string("\n");
    // Tworzymy wirtualny plik reprezentujący root EXT2 w VFS
    vfs_node* ext2_file = (vfs_node*)kmalloc(sizeof(vfs_node));
    const char* test_name = "ext2_root";

    
    // Kopiowanie nazwy
    int i = 0;
    while(test_name[i]) { ext2_file->name[i] = test_name[i]; i++; }
    ext2_file->name[i] = '\0';

    ext2_file->type = FS_FILE;
    ext2_file->size = 32; 
    // Zakładamy, że pierwszy blok danych to ten po superbloku
    ext2_file->addr = sb.s_first_data_block + 1; 
    ext2_file->read = ext2_read_node;
    
    // Wstrzyknięcie do VFS
    ext2_file->next = vfs_root;
    vfs_root = ext2_file;

    write_serial_string("[EXT2] System zamontowany pomyślnie!\n");
    
    write_serial_string("[EXT2] Skanowanie Root Directory...\n");

    ext2_inode root_inode;
    if (!ext2_read_inode(2, &root_inode)) return false;

    // Czytamy pierwszy blok danych katalogu root
    ext2_read_block(root_inode.i_block[0], temp_block_buffer);
    uint32_t current_pos = 0;

    while (current_pos < root_inode.i_size) {
        ext2_directory_entry* entry = (ext2_directory_entry*)(temp_block_buffer + current_pos);
        
        if (entry->rec_len == 0) {
            write_serial_string("[EXT2] BLAD: rec_len == 0, wychodze.\n");
            break; 
        }

        if (entry->inode != 0) {
            char namebuf[256];
            uint8_t len = entry->name_len;
            memcpy(namebuf, entry->name, len);
            namebuf[len] = '\0';

            write_serial_string("[EXT2] Wpis: ");
            write_serial_string(namebuf);
            write_serial_string(" (Inode: ");
            write_serial_hex(entry->inode);
            write_serial_string(")\n");

            // Dodajemy do VFS wszystko co nie jest kropką
            if (namebuf[0] != '.') {
                vfs_node* node = (vfs_node*)kmalloc(sizeof(vfs_node));
                memset(node, 0, sizeof(vfs_node));
                memcpy(node->name, namebuf, len + 1);

                ext2_inode file_inode;
                ext2_read_inode(entry->inode, &file_inode);
                write_serial_string("DEBUG: Inode ");
                write_serial_hex(entry->inode);
                write_serial_string(" size: ");
                write_serial_hex(file_inode.i_size); // Sprawdźmy czy tu jest 5000 (0x1388) czy 1
                write_serial_string("\n");

                // Typy: 1 = Regular file, 2 = Directory
                node->type = (entry->file_type == 2) ? FS_DIRECTORY : FS_FILE;
                node->size = file_inode.i_size;
                for(int b=0; b<15; b++) {node->blocks[b] = file_inode.i_block[b];}
                node->read = ext2_read_node;
                
                node->next = vfs_root;
                vfs_root = node;
            }
        }
        current_pos += entry->rec_len;
    }

    return true;

}


#include "vfs.h"
#include "tar.h"
#include "kernel.h"

// Musisz mieć funkcję kmalloc zadeklarowaną gdzieś
extern "C" void* kmalloc(size_t size);

vfs_node* vfs_root = nullptr;
extern uint64_t initrd_addr;

// Funkcja odczytu specyficzna dla plików w RAM
uint32_t tar_read_internal(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    uint8_t* src = (uint8_t*)(node->addr + offset);
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = src[i];
    }
    return size;
}

void vfs_init() {
    if (initrd_addr == 0) return;

    uint64_t current_addr = initrd_addr;
    vfs_node* last_node = nullptr;

    while (true) {
        tar_header* header = (tar_header*)current_addr;
        
        // Jeśli nazwa jest pusta, to koniec archiwum
        if (header->filename[0] == '\0') break;

        uint64_t size = get_tar_size(header->size);
        
        // Tworzymy nowy węzeł VFS dla każdego pliku w TAR
        vfs_node* node = (vfs_node*)kmalloc(sizeof(vfs_node));
        
        // Kopiujemy nazwę
        for(int i=0; i<100; i++) node->name[i] = header->filename[i];
        
        node->type = FS_FILE;
        node->size = size;
        node->addr = current_addr + 512; // Dane są po nagłówku
        node->read = tar_read_internal;
        node->next = nullptr;

        // Budujemy listę powiązaną plików
        if (vfs_root == nullptr) {
            vfs_root = node;
        } else {
            last_node->next = node;
        }
        last_node = node;

        // Skaczemy do następnego nagłówka (wyrównanie do 512 bajtów)
        current_addr += ((size + 511) / 512 + 1) * 512;
    }
    write_serial_string("[VFS] System plikow zainicjalizowany.\n");
}

vfs_node* vfs_find(const char* name) {
    vfs_node* curr = vfs_root;
    while (curr) {
        // Proste porównanie stringów (możesz użyć swojej funkcji strcmp)
        bool match = true;
        for (int i = 0; name[i] != '\0' || curr->name[i] != '\0'; i++) {
            if (name[i] != curr->name[i]) {
                match = false;
                break;
            }
        }
        if (match) return curr;
        curr = curr->next;
    }
    return nullptr;
}
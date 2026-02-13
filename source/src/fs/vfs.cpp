#include "vfs.h"
#include "tar.h"
#include "kernel.h"

// Musisz mieć funkcję kmalloc zadeklarowaną gdzieś
extern "C" void* kmalloc(size_t size);
extern "C" int strcmp(const char* s1, const char* s2);

vfs_node* vfs_root = nullptr;
extern uint64_t initrd_addr;

uint64_t tar_read(vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (offset >= node->size) return 0;
    if (offset + size > node->size) size = node->size - offset;

    uint8_t* src = (uint8_t*)(node->addr + offset);
    for (uint64_t i = 0; i < size; i++) {
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
        if (header->filename[0] == '\0') break;

        write_serial_string("[VFS] Znaleziono w TAR: ");
        write_serial_string(header->filename);
        write_serial_string("\n");

        uint64_t size = get_tar_size(header->size);
        
        vfs_node* node = (vfs_node*)kmalloc(sizeof(vfs_node));
        for(int i=0; i<100; i++) {
            node->name[i] = header->filename[i];
            if (header->filename[i] == '\0') break; // Skończ wcześniej, jeśli jest null
        }
        node->name[99] = '\0'; // Bezpiecznik
        
        node->type = FS_FILE;
        node->size = size;
        node->addr = current_addr + 512;
        node->read = tar_read;
        node->write = nullptr;
        node->next = nullptr;

        if (vfs_root == nullptr) vfs_root = node;
        else last_node->next = node;
        
        last_node = node;
        current_addr += ((size + 511) / 512 + 1) * 512;
    }
    write_serial_string("[VFS] System plikow zainicjalizowany.\n");
}

vfs_node* vfs_find(const char* name) {
    if (!name || name[0] == '\0') return nullptr;

    // 1. Znajdź "basename" (wszystko po ostatnim ukośniku)
    const char* filename = name;
    for (int i = 0; name[i] != '\0'; i++) {
        if (name[i] == '/') {
            filename = &name[i + 1];
        }
    }

    // Jeśli po ostatnim '/' nic nie ma (np. ścieżka to "/"), zwróć błąd
    if (filename[0] == '\0') return nullptr;

    write_serial_string("[VFS] Szukam surowej nazwy: ");
    write_serial_string(filename);
    write_serial_string("\n");

    vfs_node* curr = vfs_root;
    while (curr) {
        write_serial_string("  Sprawdzam: ");
        write_serial_string(curr->name);
        write_serial_string("\n");
        if (strcmp(curr->name, filename) == 0) {
            write_serial_string("[VFS] Znaleziono plik: ");
            write_serial_string(curr->name);
            write_serial_string("\n");
            return curr;
        }
        curr = curr->next;
    }
    return nullptr;
}

uint64_t vfs_read(vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}

vfs_node* vfs_find_node(vfs_node* root, const char* name) {
    return vfs_find(name); // Przekierowanie na naszą nową funkcję
}

uint64_t vfs_write(vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (node && node->write)
        return node->write(node, offset, size, buffer);
    return 0;
}
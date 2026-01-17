#pragma once
#include <stdint.h>
#include <stddef.h>

#define FS_FILE      0x01
#define FS_DIRECTORY 0x02

struct vfs_node {
    char name[128];
    uint32_t type;
    uint32_t size;
    uint64_t addr;      // Fizyczny adres danych w pamięci (dla TarFS)
    
    // Wskaźniki na funkcje - polimorfizm w C
    uint32_t (*read)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
    struct vfs_node* next; // Lista plików w katalogu
};

extern vfs_node* vfs_root;

// Funkcje systemowe do użycia w Shellu
void vfs_init();
vfs_node* vfs_find(const char* name);
uint32_t vfs_read(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
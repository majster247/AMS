#pragma once
#include <stdint.h>
#include <stddef.h>

#define FS_FILE      0x01
#define FS_DIRECTORY 0x02

enum FS_SOURCE { FS_TAR, FS_EXT2 };

struct vfs_node {
    char name[128];
    uint32_t type;
    uint32_t size;
    uint64_t addr;      // Dla TAR: adres w RAM, Dla EXT2: numer Inody
    uint64_t length;    // Długość pliku (dla katalogów może być 0)
    FS_SOURCE source;   // Skąd pochodzi plik?
    
    uint32_t (*read)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
    struct vfs_node* next;
    uint32_t blocks[15]; // Dla EXT2: tablica bloków pliku
};

extern vfs_node* vfs_root;

// Funkcje systemowe do użycia w Shellu
void vfs_init();
vfs_node* vfs_find(const char* name);
uint32_t vfs_read(vfs_node* node, uint32_t offset, uint32_t size, uint8_t* buffer);
vfs_node* vfs_find_node(vfs_node* start, const char* name);
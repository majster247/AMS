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
    uint32_t current_pos;
    uint64_t addr;      // Inode number
    uint64_t length;    
    FS_SOURCE source;   
    
    // --- ZMIANA NA uint64_t ---
    uint64_t (*read)(struct vfs_node*, uint64_t, uint64_t, uint8_t*);
    uint64_t (*write)(struct vfs_node*, uint64_t, uint64_t, uint8_t*);
    // --------------------------
    
    struct vfs_node* next;
    uint32_t blocks[15]; 
};

extern vfs_node* vfs_root;

void vfs_init();
vfs_node* vfs_find(const char* name);
vfs_node* vfs_find_node(vfs_node* start, const char* name);

// Te też muszą być uint64_t
uint64_t vfs_read(vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer);
uint64_t vfs_write(vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer);
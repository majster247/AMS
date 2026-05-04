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
    uint32_t max_size; // Dla plików w initrd, aby wiedzieć ile można bezpiecznie czytać
    //is_directory jest teraz boolem, ale zostawiamy type jako uint32_t, bo może nam się przydać do oznaczania różnych typów w przyszłości (np. linki symboliczne)
    bool is_directory;
    uint32_t current_pos;
    uint64_t addr;      // Inode number
    uint64_t length;    
    FS_SOURCE source;   
    
    // --- ZMIANA NA uint64_t ---
    uint64_t (*read)(struct vfs_node*, uint64_t, uint64_t, uint8_t*);
    uint64_t (*write)(struct vfs_node*, uint64_t, uint64_t, uint8_t*);
    // --------------------------
    
    struct vfs_node* next;
    //tar_data będzie używane tylko dla plików z initrd, które chcemy móc modyfikować (np. do zapisu skompilowanych plików przez tcc)
    uint8_t* tar_data;
    uint32_t blocks[15]; 
};

extern vfs_node* vfs_root;

void vfs_init();
vfs_node* vfs_find(const char* name);
vfs_node* vfs_find_node(vfs_node* start, const char* name);
/** Remove a dynamically added flat-VFS file node (matches vfs_find naming rules). Returns true if removed. */
bool vfs_remove_file(const char* name);

// Te też muszą być uint64_t
size_t vfs_read(vfs_node* node, uint64_t offset, size_t size, uint8_t* buffer);
uint64_t vfs_write(vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer);
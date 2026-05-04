#include "vfs.h"
#include "tar.h"
#include "ext2.h"
#include "kernel.h"

// Musisz mieć funkcję kmalloc zadeklarowaną gdzieś
extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);
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


//funkcja pomocnicza k_strstr do sprawdzania prefiksów w nazwach plików
static char* k_strstr(const char* haystack, const char* needle) {
    if (!haystack || !needle) return nullptr;
    size_t needle_len = 0;
    while (needle[needle_len]) needle_len++;

    for (size_t i = 0; haystack[i]; i++) {
        size_t j = 0;
        while (j < needle_len && haystack[i + j] == needle[j]) j++;
        if (j == needle_len) return (char*)(haystack + i);
    }
    return nullptr;
}


vfs_node* vfs_find(const char* name) {
    if (!name || name[0] == '\0') return nullptr;

    // 1. Pomiń WSZYSTKIE wiodące slashe (naprawia problem z //tccdefs.h)
    const char* clean_name = name;
    while (*clean_name == '/') {
        clean_name++;
    }

    // 1b. Pomiń powtarzające się prefiksy "./"
    while (clean_name[0] == '.' && clean_name[1] == '/') {
        clean_name += 2;
    }

    // 1c. Zachowujemy pełną ścieżkę jako pierwszy wybór (Linux-like).
    // Flat basename używamy tylko jako fallback kompatybilności.
    const char* original_clean_name = clean_name;

    // 2. Jeśli nazwa była samym "/" i teraz jest pusta, zwróć root (lub błąd jeśli nie obsługujesz)
    if (*clean_name == '\0') return nullptr;

    write_serial_string("[VFS] vfs_find called with: \"");
    write_serial_string(name);
    write_serial_string("\"\n");

    // 3. Mapowanie nagłówków TCC
    // Używamy clean_name, żeby nie martwić się o slashe na początku
    if (k_strstr(clean_name, "usr/lib/tcc/include/") || 
        k_strstr(clean_name, "usr/local/lib/tcc/include/")) {
        
        // Wyciągamy samą nazwę pliku po ścieżce tcc (np. stdio.h)
        const char* hdr_last_slash = clean_name;
        const char* p = clean_name;
        while(*p) {
            if (*p == '/') hdr_last_slash = p + 1;
            p++;
        }
        clean_name = hdr_last_slash;
        write_serial_string("[VFS] Redirecting TCC include to: ");
        write_serial_string(clean_name);
        write_serial_string("\n");
    }

    write_serial_string("[VFS] Szukam pelnej nazwy: ");
    write_serial_string(original_clean_name);
    write_serial_string("\n");

    vfs_node* curr = vfs_root;
    while (curr) {
        if (strcmp(curr->name, original_clean_name) == 0) {
            write_serial_string("[VFS] Znaleziono plik: ");
            write_serial_string(curr->name);
            write_serial_string("\n");
            return curr;
        }
        curr = curr->next;
    }

    // Fallback: flat basename dla istniejących artefaktów AMS.
    const char* last_slash = original_clean_name;
    for (const char* p = original_clean_name; *p; ++p) {
        if (*p == '/') last_slash = p + 1;
    }
    const char* fallback_name = (*last_slash) ? last_slash : original_clean_name;
    if (strcmp(fallback_name, original_clean_name) != 0) {
        write_serial_string("[VFS] Fallback basename: ");
        write_serial_string(fallback_name);
        write_serial_string("\n");

        curr = vfs_root;
        while (curr) {
            if (strcmp(curr->name, fallback_name) == 0) {
                write_serial_string("[VFS] Znaleziono plik (fallback): ");
                write_serial_string(curr->name);
                write_serial_string("\n");
                return curr;
            }
            curr = curr->next;
        }
    }

    write_serial_string("[VFS] NIE ZNALEZIONO: ");
    write_serial_string(original_clean_name);
    write_serial_string("\n");
    return nullptr;
}

size_t vfs_read(vfs_node* node, uint64_t offset, size_t size, uint8_t* buffer) {
    // Walidacja buffer (musi być user space!)
    if (node->read) {
        // Odczyt z EXT2
        return node->read(node, offset, size, buffer);
    }
    
    if (node->read) {
        write_serial_string("[VFS] Reading from TAR at offset ");
        write_serial_dec(offset);
        write_serial_string(", size ");
        write_serial_dec(size);
        write_serial_string("\n");
        
        size_t to_copy = size;
        if (offset >= node->size) return 0;
        if (offset + size > node->size) {
            to_copy = node->size - offset;
        }
        
        // ✅ Kopiuj z kernel space (tar_data) do user space (buffer)
        //memcpy(buffer, node->tar_data + offset, to_copy);
        
        write_serial_string("[VFS] Copied ");
        write_serial_dec(to_copy);
        write_serial_string(" bytes to user buffer\n");
        
        return to_copy;
    }
    
    return 0;
}

vfs_node* vfs_find_node(vfs_node* root, const char* name) {
    return vfs_find(name); // Przekierowanie na naszą nową funkcję
}

bool vfs_remove_file(const char* name) {
    vfs_node* target = vfs_find(name);
    if (!target || target->is_directory) return false;

    vfs_node* prev = nullptr;
    vfs_node* curr = vfs_root;
    while (curr) {
        if (curr == target) {
            if (prev) prev->next = curr->next;
            else vfs_root = curr->next;
            if (curr->tar_data) kfree(curr->tar_data);
            kfree(curr);
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

uint64_t vfs_write(vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer) {
    if (node && node->write)
        return node->write(node, offset, size, buffer);
    return 0;
}
#pragma once
#include <stdint.h>

struct tar_header {
    char filename[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];     // Rozmiar w formacie ósemkowym (tekst)
    char mtime[12];
    char chksum[8];
    char typeflag;     // '0' = plik, '5' = katalog
    char linkname[100];
    char magic[6];     // "ustar"
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
} __attribute__((packed));

inline uint64_t get_tar_size(const char* in) {
    uint64_t size = 0;
    for (int i = 0; i < 11; i++) {
        if (in[i] < '0' || in[i] > '7') break;
        size = size * 8 + (in[i] - '0');
    }
    return size;
}

char* tar_read_file(uint64_t addr, const char* target_filename);
void list_files(uint64_t addr);
#pragma once
#include <stdint.h>

#define EXT2_MAGIC 0xEF53

// Typy plików w Inodzie
#define EXT2_S_IFREG  0x8000
#define EXT2_S_IFDIR  0x4000

struct ext2_superblock {
    uint32_t s_inodes_count;      // Całkowita liczba i-węzłów
    uint32_t s_blocks_count;      // Całkowita liczba bloków
    uint32_t s_r_blocks_count;    // Bloki zarezerwowane dla roota
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;  // Zazwyczaj 0 (dla bloków > 1KB) lub 1
    uint32_t s_log_block_size;    // Rozmiar bloku = 1024 << s_log_block_size
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;             // Musi być 0xEF53
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    // Pola dla wersji dynamicznej (rev >= 1)
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;
} __attribute__((packed));

struct ext2_group_descriptor {
    uint32_t bg_block_bitmap;      // Blok bitmapy bloków
    uint32_t bg_inode_bitmap;      // Blok bitmapy i-węzłów
    uint32_t bg_inode_table;       // Początek tabeli i-węzłów
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed));

struct ext2_inode {
    uint16_t i_mode;        // Uprawnienia i typ pliku
    uint16_t i_uid;
    uint32_t i_size;        // Rozmiar w bajtach
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;      // Liczba sektorów (512B) zajętych
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];   // Wskaźniki na bloki danych
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint32_t i_osd2[3];
} __attribute__((packed));
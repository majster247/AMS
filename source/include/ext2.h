/**
 * @file ext2.h
 * @author Majster
 * @brief Obsługa systemu plików Second Extended Filesystem (EXT2).
 */

#pragma once
#include <stdint.h>
#include "ahci.h"

#define EXT2_MAGIC 0xEF53    /**< Sygnatura systemu plików EXT2 */
#define EXT2_ROOT_INODE 2    /**< Numer i-node'a katalogu głównego */

/** @brief Superblock - Główne metadane systemu plików */
struct ext2_superblock {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;   /**< Rozmiar bloku = 1024 << s_log_block_size */
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;            /**< Musi wynosić 0xEF53 */
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    uint32_t s_first_ino;
    uint16_t s_inode_size;
} __attribute__((packed));

/** @brief Deskryptor grupy bloków (Block Group Descriptor) */
struct ext2_group_descriptor {
    uint32_t bg_block_bitmap;
    uint32_t bg_inode_bitmap;
    uint32_t bg_inode_table;    /**< Adres bazy tablicy i-node'ów w tej grupie */
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;
    uint16_t bg_pad;
    uint32_t bg_reserved[3];
} __attribute__((packed));

/** @brief Struktura i-node reprezentująca plik lub katalog */
struct ext2_inode {
    uint16_t i_mode;        /**< Typ pliku (katalog/plik) i uprawnienia */
    uint16_t i_uid;
    uint32_t i_size;        /**< Rozmiar pliku w bajtach */
    uint32_t i_atime;
    uint32_t i_ctime;
    uint32_t i_mtime;
    uint32_t i_dtime;
    uint16_t i_gid;
    uint16_t i_links_count;
    uint32_t i_blocks;      /**< Liczba sektorów dyskowych 512B zajmowanych przez plik */
    uint32_t i_flags;
    uint32_t i_osd1;
    uint32_t i_block[15];   /**< Wskaźniki do bloków danych (0-11: bezpośrednie, 12: ind. 1 stopnia...) */
    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint32_t i_osd2[3];
} __attribute__((packed));

/** @brief Wpis w katalogu (Directory Entry) */
struct ext2_directory_entry {
    uint32_t inode;         /**< Numer i-node dla danego wpisu */
    uint16_t rec_len;       /**< Długość całkowita rekordu */
    uint8_t  name_len;      /**< Długość nazwy pliku */
    uint8_t  file_type;     /**< Typ wpisu (1=plik, 2=katalog) */
    char     name[];        /**< Nazwa pliku o zmiennej długości */
} __attribute__((packed));

/** @brief Korzeń systemu plików EXT2 wewnątrz VFS */
extern struct vfs_node* ext2_root;

/** @brief Inicjalizuje EXT2 na danym porcie AHCI i montuje w VFS */
bool ext2_init(ahci_port* port);
/** @brief Czyta dane z i-node'a do bufora (obsługuje offset i rozmiar) */
uint64_t ext2_read_node(struct vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer);
/** @brief Czyta cały plik pod wskazaną ścieżką do bufora w RAM (zwraca wskaźnik) */
char* ext2_read_file(const char* path);
/** @brief Zapisuje dane do i-node'a (obsługuje offset i rozmiar) */
uint64_t ext2_write(struct vfs_node* node, uint64_t offset, uint64_t size, uint8_t* buffer);

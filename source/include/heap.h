/**
 * @file heap.h
 * @author Majster
 * @brief Dynamiczna alokacja pamięci jądra (Kernel Heap).
 */

#pragma once
#include <stddef.h>
#include <stdint.h>

extern "C" {
    /** @brief Alokuje blok pamięci jądra o rozmiarze size */
    void* kmalloc(size_t size);
    /** @brief Zwalnia blok pamięci wskazywany przez ptr */
    void kfree(void* ptr);
    /** @brief Standardowy alias malloc (używany przez mlibc/TCC) */
    void* malloc(size_t size);
    /** @brief Standardowy alias free */
    void free(void* ptr);
}

/** === C++ Operators Overloading === */
void* operator new(size_t size);
void operator delete(void* ptr);
void* operator new[](size_t size);
void operator delete[](void* ptr);

void operator delete(void* ptr, size_t size);
void operator delete[](void* ptr, size_t size);
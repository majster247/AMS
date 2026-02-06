#pragma once
#include <stddef.h>
#include <stdint.h>
extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);
extern "C" void* malloc(size_t size);
extern "C" void free(void* ptr);

void* operator new(size_t size);
void operator delete(void* ptr);
void* operator new[](size_t size);
void operator delete[](void* ptr);

void operator delete(void* ptr, size_t size);
void operator delete[](void* ptr, size_t size);
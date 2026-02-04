#pragma once
#include <stddef.h>
#include <stdint.h>
extern "C" void* kmalloc(size_t size);
extern "C" void kfree(void* ptr);
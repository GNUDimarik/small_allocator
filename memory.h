#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

int mem_initialize(void *base, size_t size);
void mem_unuinitialize();
void *mem_malloc(size_t size);
void *mem_calloc(size_t num, size_t size);
void *mem_realloc(void *p, size_t new_sz);
void mem_free(void *ptr);
[[maybe_unused]] void dump_mem();
[[maybe_unused]] void dump_bins();
[[maybe_unused]] bool mem_check_block(void *p);
[[maybe_unused]] bool mem_check(bool verbose = false);

#endif //MEMORY_H

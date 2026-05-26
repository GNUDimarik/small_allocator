#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

int mem_initialize(void *__base, size_t __size);
void mem_unuinitialize();
void *mem_malloc(size_t _size);
void *mem_calloc(size_t snum, size_t size);
void *mem_realloc(void *p, size_t new_sz);
void mem_free(void *ptr);
void mem_dump();

void dump_free_mem();
void dump_bins();

#endif //MEMORY_H

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <string.h>
#include "memory.h"

#define LOG_TAG "main"
#include "logging.h"

#define PAGE_SIZE 4096
#define HEAP_SIZE (4096 + 1000000 + 2048)
#define ITEMS_NUMBER 1000

static char *char_alloc(size_t size) __attribute__((unused));

static char *char_alloc(size_t size)
{
    auto p = mem_malloc(size);
    //std::cout << __func__ << ": p = " << std::hex << p << std::endl;
    return reinterpret_cast<char *>( p);
}

int main()
{
    std::unique_ptr<char[]> heap(new char[HEAP_SIZE]);
    std::vector<char *> vptr;
    vptr.reserve(ITEMS_NUMBER);
    auto p = heap.get();
    *p = 'a';
    mem_initialize(heap.get(), HEAP_SIZE);
    dump_mem();
    dump_bins();

    for (int i = 0; i < ITEMS_NUMBER; ++i) {
        vptr[i] = char_alloc(i);
    }

    for (int i = 0; i < ITEMS_NUMBER; ++i) {
        if ((i % 2) == 0) {
            mem_free(vptr[i]);
        }
    }

    dump_mem();
    dump_bins();
    return 0;
}

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <string.h>
#include "memory.h"

#define LOG_TAG "main"
#include "logging.h"

#define PAGE_SIZE 4096
#define HEAP_SIZE (PAGE_SIZE * 1)
#define ITEMS_NUMBER 19

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
    mem_initialize(heap.get(), HEAP_SIZE);
    dump_mem();
    auto a = char_alloc(12);
    auto b = char_alloc(12);
    auto c = char_alloc(12);
    mem_free(a);
    dump_mem();
    mem_free(b);
    dump_mem();
    mem_free(c);
    dump_mem();
    return 0;
}

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <string.h>
#include "memory.h"

#define LOG_TAG "main"
#include "logging.h"

#define PAGE_SIZE 4096
#define HEAP_SIZE PAGE_SIZE//(4096 + 1000000 + 2048)
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
#if 1
    for (int iter = 0; iter < 1000; ++iter) {
        void* p1 = mem_malloc(rand() % 1024 + 1);
        if (p1) {
            void* p2 = mem_realloc(p1, rand() % 2048 + 1);
            if (p2) {
                void* p3 = nullptr;//mem_calloc(rand() % 100 + 1, 8);
                if (p3) {
                    mem_free(p3);
                }
                mem_free(p2);
            } else {
                mem_free(p1);
            }
        }
    }
#endif
    dump_mem();
    return 0;
}

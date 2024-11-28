#include <iostream>
#include <memory>
#include <vector>
#include "allocator.h"

using namespace std;

#define HEAP_SIZE 4096
#define COUNT 3
#define PTR_SIZE 10

uint8_t* memory_alloc(size_t size) {
    return reinterpret_cast<uint8_t*> (utils::alloc_malloc::mem_malloc(size));
}

char* char_alloc(size_t size) {
    return reinterpret_cast<char*> (utils::alloc_malloc::mem_malloc(size));
}

char* char_realloc(char* p, size_t size) {
    return reinterpret_cast<char*> (utils::alloc_malloc::mem_realloc(p, size));
}

void memory_free(void *p) {
    utils::alloc_malloc::mem_free(p);
}

int main()
{
    std::unique_ptr<uint8_t[]> heap(new uint8_t[HEAP_SIZE]);
    utils::alloc_malloc::mem_init(heap.get(), HEAP_SIZE);
    std::vector<uint8_t*> pointers;
    pointers.reserve(COUNT);

    utils::alloc_malloc::__dump();

    for (int i = 0; i < COUNT; i++) {
        pointers[i] = memory_alloc(i);
    }

    utils::alloc_malloc::__dump();

    for (int i = 0; i < COUNT; i++) {
        utils::alloc_malloc::mem_free(pointers[i]);
    }

    utils::alloc_malloc::__dump();

    memory_alloc(50000);
    return 0;
}

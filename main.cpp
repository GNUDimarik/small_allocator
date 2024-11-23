#include <iostream>
#include <memory>
#include <vector>
#include "allocator.h"

using namespace std;

#define HEAP_SIZE 4096LU * 4
#define COUNT 20
#define PTR_SIZE 30

uint8_t* memory_alloc(size_t size) {
    return reinterpret_cast<uint8_t*> (utils::alloc_malloc::mem_malloc(size));
}

int main()
{
    std::unique_ptr<uint8_t[]> heap(new uint8_t[HEAP_SIZE]);
    utils::alloc_malloc::mem_init(heap.get(), HEAP_SIZE);
    std::vector<uint8_t*> pointers;
    pointers.reserve(COUNT);

    for (int i = 0; i < COUNT; i++) {
        pointers[i] = memory_alloc(i);
    }

    utils::alloc_malloc::__dump();

    for (int i = 0; i < COUNT; i++) {
        utils::alloc_malloc::mem_free(pointers[i]);
    }

    utils::alloc_malloc::__dump();

    return 0;
}

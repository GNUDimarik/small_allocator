#include <iostream>
#include <memory>
#include <vector>
#include "_MemoryBlock.h"
#include "_Allocator.h"

using namespace std;

#define HEAP_SIZE 4096
#define COUNT 300
#define PTR_SIZE 10

std::unique_ptr<mem::utils::_Allocator> gAlloc;

uint8_t *memory_alloc(size_t size)
{
    return reinterpret_cast<uint8_t *> (gAlloc->malloc(size));
}

char *char_alloc(size_t size)
{
    return reinterpret_cast<char *> (gAlloc->malloc(size));
}

char *char_realloc(char *p, size_t size)
{
    return reinterpret_cast<char *> (gAlloc->realloc(p, size));
}

void memory_free(void *p)
{
    gAlloc->free(p);
}

int main()
{

    std::unique_ptr<uint8_t[]> heap(new uint8_t[HEAP_SIZE]);
    gAlloc = std::make_unique<mem::utils::_Allocator>(heap.get(), HEAP_SIZE);
    std::vector<uint8_t *> pointers;
    pointers.reserve(COUNT);

    gAlloc->dump();

    for (int i = 0; i < COUNT; i++) {
        if (i % 2) {
            pointers[i] = memory_alloc(i);
        }
    }

    gAlloc->dump();

    for (int i = 0; i < COUNT; i++) {
        gAlloc->free(pointers[i]);
    }

    memory_alloc(16);

    gAlloc->dump();
    return 0;
}
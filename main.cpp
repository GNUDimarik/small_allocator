#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <string.h>
#include "memory.h"

#define LOG_TAG "main"
#include "logging.h"

#define HEAP_SIZE 4096
#define ITEMS_NUMBER 19

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
    mem_dump();

    for (int i = 1; i < ITEMS_NUMBER; ++i) {
        vptr[i] = char_alloc(20);
        std::string str = "str zalupa" + std::to_string(i);
        auto ptr = vptr[i];

        if (ptr) {
            strcpy(ptr, str.c_str());
        }
    }

    mem_dump();

    /*for (int i = 1; i < ITEMS_NUMBER; ++i) {
        //if (i % 2) {
        std::cout << "vptr[" << i << "] " << vptr[i] << std::endl;
        mem_free(vptr[i]);
        // }
    }*/

    /*for (int i = 1; i < ITEMS_NUMBER; ++i) {
        vptr[i] = char_alloc(i * 20);
        std::string str = "str zalupa" + std::to_string(i);
        auto ptr = vptr[i];

        if (ptr) {
            strcpy(ptr, str.c_str());
        }
    }*/

    mem_free(vptr[10]);
    auto str = char_alloc(20);
    mem_dump();
    strcpy(str, "Jopka kota");
    std::cout << "before realloc: str " << str << std::endl;
    auto str1 = static_cast<char *>(mem_realloc(str, 600));
    std::cout << "after realloc: str1 " << std::endl;
    ALOGD("after realloc str %p str1 %p", str, str1);

    //char_alloc(500);

    mem_dump();

    dump_bins();

    return 0;
}

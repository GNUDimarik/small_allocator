#include <gtest/gtest.h>
#include <memory>
#include <string.h>
#include <string>
#include <string.h>
#include <vector>

#define ALLOCATOR_IN_TEST 1
#include "../allocator.h"

#define HEAP_SIZE 1024
#define CALLOC_TEST_ARRAY_LEN 100
#define CALLOC_TEST_ARG_SZ 1
#define MEM_ALLOC_ARRAY_LEN 10
#define TEST_BLK_SIZE_MIN 15
#define TEST_BLK_SIZE_MAX 30

std::unique_ptr<char[]> sHeap(new char[HEAP_SIZE]);

char* memory_alloc(size_t size) {
    return reinterpret_cast<char*> (utils::alloc_malloc::mem_malloc(size));
}

char* memory_calloc(size_t size, size_t count) {
    return reinterpret_cast<char*> (utils::alloc_malloc::mem_calloc(size, count));
}

char* memory_realloc(void* ptr, size_t new_size) {
    return reinterpret_cast<char*> (utils::alloc_malloc::mem_realloc(ptr, new_size));
}

constexpr auto memory_free = utils::alloc_malloc::mem_free;

using utils::alloc_malloc::_MemoryBlock_t;

TEST(AllocatorTest, merge_test) {
    std::vector<char*> pointers;
    pointers.reserve(MEM_ALLOC_ARRAY_LEN);

    char* p = nullptr;
    _MemoryBlock_t block =  _MemoryBlock_t::__from_user_space_memory(p);
    size_t expected_block_size = _MemoryBlock_t::_S_total_overhead_size;
    size_t prev_blk_size = _MemoryBlock_t::_S_total_overhead_size;

    for (int i = 1; i < 8; i++) {
        p = memory_alloc(i);
        pointers[i] = p;
        block =  _MemoryBlock_t::__from_user_space_memory(p);
        ASSERT_EQ(block.__is_allocated(), true);
        ASSERT_EQ(block.__is_free(), false);
        ASSERT_EQ(block.__size(), expected_block_size);
        ASSERT_EQ(block.__size_with_overhead(), _MemoryBlock_t::_S_total_overhead_size + block.__size());
        ASSERT_EQ(*block._M_prev_blk_size, prev_blk_size);
    }

    auto first =  _MemoryBlock_t::__from_user_space_memory(pointers[1]);

    for (int i = 1; i < 8; i++) {
        block =  _MemoryBlock_t::__from_user_space_memory(pointers[i]);
        expected_block_size = block.__size();

        auto __n = block.__next_implicit_block();

        if (!__n.__is_null() && __n.__is_free()) {
            expected_block_size += __n.__size_with_overhead();
        }

        memory_free(pointers[i]);
        ASSERT_EQ(block.__is_allocated(), false);
        ASSERT_EQ(block.__is_free(), true);
        ASSERT_EQ(block.__size(), expected_block_size);
        ASSERT_EQ(block.__size_with_overhead(), _MemoryBlock_t::_S_total_overhead_size + block.__size());
        ASSERT_EQ(*block._M_prev_blk_size, prev_blk_size);

        if (i > 1) {
            prev_blk_size += block.__size_with_overhead();
        }
    }

    ASSERT_EQ(first.__is_allocated(), false);
    ASSERT_EQ(first.__is_free(), true);
    ASSERT_EQ(first.__size(), HEAP_SIZE - (_MemoryBlock_t::_S_total_overhead_size * 5));
    ASSERT_EQ(first.__size_with_overhead(), HEAP_SIZE - (_MemoryBlock_t::_S_total_overhead_size * 4));
    ASSERT_EQ(*first._M_prev_blk_size, _MemoryBlock_t::_S_total_overhead_size);
}

TEST(AllocatorTest, mem_malloc_test) {
    char* p = memory_alloc(0);
    ASSERT_EQ(p, nullptr);

    _MemoryBlock_t block =  _MemoryBlock_t::__from_user_space_memory(p);
    ASSERT_EQ(block.__is_null(), true);

    std::vector<char*> pointers;
    pointers.reserve(MEM_ALLOC_ARRAY_LEN);

    for (int i = 1; i < 8; i++) {
        p = memory_alloc(i);
        pointers[i] = p;
        block =  _MemoryBlock_t::__from_user_space_memory(p);
        ASSERT_EQ(block.__is_allocated(), true);
        ASSERT_EQ(block.__is_free(), false);
        ASSERT_EQ(*block._M_prev_blk_size, _MemoryBlock_t::_S_total_overhead_size);
    }

    for  (int i = 1; i < 8; i++) {
        // Not more 8 characters including ending \0
        std::string str = "it nr=" + std::to_string(i);
        strcpy(pointers[i], str.c_str());
        ASSERT_EQ(strcmp(pointers[i], str.c_str()), 0);
    }

    for (int i = 1; i < 8; i++) {
        p = pointers[i];
        block =  _MemoryBlock_t::__from_user_space_memory(p);
        memory_free(p);
        ASSERT_EQ(block.__is_allocated(), false);
        ASSERT_EQ(block.__is_free(), true);
    }
}

TEST(AllocatorTest, mem_free_test) {
    char *ptr = memory_calloc(CALLOC_TEST_ARRAY_LEN, CALLOC_TEST_ARG_SZ);
    char array[CALLOC_TEST_ARRAY_LEN];
    memset(array, 0, CALLOC_TEST_ARRAY_LEN);
    EXPECT_EQ(memcmp(ptr, array, CALLOC_TEST_ARRAY_LEN), 0);

    _MemoryBlock_t block =  _MemoryBlock_t::__from_user_space_memory(ptr);
    ASSERT_EQ(block.__is_allocated(), true);

    memory_free(ptr);
    ASSERT_EQ(block.__is_free(), true);
}

TEST(AllocatorTest, mem_realloc_test) {

    char* p0 = memory_alloc(100);
    char* p1 = memory_alloc(200);
    char* p2 = memory_alloc(100);
    const char* str = "Hello world!";

    memory_free(p2);
    strcpy(p1, str);
    char* old_ptr = p1;
    p1 = memory_realloc(p1, 220);
    ASSERT_EQ(p1, old_ptr);
    ASSERT_STREQ(p1, str);

    old_ptr = p0;
    strcpy(p0, str);
    p0 = memory_realloc(p0, 300);
    ASSERT_NE(old_ptr, p0);
    ASSERT_TRUE(p0 > old_ptr);
    ASSERT_STREQ(p0, str);

    old_ptr = p0;
    p0 = memory_realloc(p0, 100);
    ASSERT_EQ(old_ptr, p0);
    ASSERT_STREQ(p0, str);

    memory_free(p0);
    memory_free(p1);
}

TEST(AllocatorTest, mem_calloc_test) {
    char *ptr = memory_calloc(CALLOC_TEST_ARRAY_LEN, CALLOC_TEST_ARG_SZ);
    char array[CALLOC_TEST_ARRAY_LEN];
    memset(array, 0, CALLOC_TEST_ARRAY_LEN);
    EXPECT_EQ(memcmp(ptr, array, CALLOC_TEST_ARRAY_LEN), 0);

    memset(array, 1, CALLOC_TEST_ARRAY_LEN);
    EXPECT_NE(memcmp(ptr, array, CALLOC_TEST_ARRAY_LEN), 0);
    memory_free(ptr);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    utils::alloc_malloc::mem_init(reinterpret_cast<uint8_t*>(sHeap.get()), HEAP_SIZE);

    return RUN_ALL_TESTS();
}

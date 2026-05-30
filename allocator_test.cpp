#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new> // for std::max_align_t
#include <vector>
#include "memory.h"

#define LOG_TAG "test"
#include "logging.h"

// Вспомогательная функция для проверки выравнивания указателя
static bool is_aligned_to_max_align(void *ptr)
{
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(ptr);
    return addr % alignof(sizeof(void *)) == 0;
}

// ----------------------------------------------------------------------
// Тесты для mem_malloc
// ----------------------------------------------------------------------

TEST(MallocTest, AllocZero)
{
    void *p = mem_malloc(0);
    EXPECT_EQ(p, nullptr);
    mem_free(p);
    SUCCEED();
}

TEST(MallocTest, AllocMax)
{
// Попытка выделить слишком большой блок – ожидаем NULL
    size_t huge = std::numeric_limits<size_t>::max() / 2;
    void *p = mem_malloc(huge);
    EXPECT_EQ(p, nullptr);
// free не вызываем, так как указатель нулевой
}

TEST(MallocTest, MultipleAllocationsNoOverlap)
{
    dump_mem();
    const size_t N = 100;
    std::vector<void *> ptrs;
    for (size_t i = 0; i < N; ++i) {
        void *p = mem_malloc(16);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }
    dump_mem();
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = i + 1; j < N; ++j) {
            EXPECT_NE(ptrs[i], ptrs[j]);
        }
    }
    dump_mem();
    for (void *p: ptrs) {
        mem_free(p);
    }
}

TEST(MallocTest, ReuseAfterFree)
{
// Выделяем и освобождаем память, затем снова выделяем – не должно быть ошибок
    void *p1 = mem_malloc(100);
    ASSERT_NE(p1, nullptr);
    mem_free(p1);
    void *p2 = mem_malloc(100);
    ASSERT_NE(p2, nullptr);
    mem_free(p2);
    SUCCEED();
}

// ----------------------------------------------------------------------
// Тесты для mem_calloc
// ----------------------------------------------------------------------

TEST(CallocTest, ZeroInitialization)
{
    const size_t snum = 10, size = 4; // 40 байт
    void *p = mem_calloc(snum, size);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned_to_max_align(p));
    unsigned char *bytes = static_cast<unsigned char *>(p);
    for (size_t i = 0; i < snum * size; ++i) {
        EXPECT_EQ(bytes[i], 0) << "byte at offset " << i << " is not zero";
    }
    mem_free(p);
}

TEST(CallocTest, ZeroSizeArguments)
{
// calloc(0, anything) и calloc(anything, 0) могут вернуть NULL или уникальный указатель
    void *p1 = mem_calloc(0, 10);
    void *p2 = mem_calloc(10, 0);
    mem_free(p1);
    mem_free(p2);
    SUCCEED();
}

TEST(CallocTest, OverflowDetection)
{
    dump_mem();
// Переполнение при умножении snum * size должно вернуть NULL
    size_t snum = std::numeric_limits<size_t>::max() / 2 + 1;
    size_t size = 2;
    void *p = mem_calloc(snum, size);
    EXPECT_EQ(p, nullptr);
    dump_mem();
}

TEST(CallocTest, LargeAllocation)
{
    const size_t snum = 10000, size = 1024; // ~10 MiB
    void *p = mem_calloc(snum, size);

    if (p == nullptr) {
        GTEST_SKIP() << "System cannot allocate 10 MiB, skipping";
    }

    EXPECT_TRUE(is_aligned_to_max_align(p));
    unsigned char *bytes = static_cast<unsigned char *>(p);

    for (size_t i = 0; i < 1000; ++i) {
        size_t offset = i * 1024;
        if (offset < snum * size) {
            EXPECT_EQ(bytes[offset], 0);
        }
    }
    mem_free(p);
}

// ----------------------------------------------------------------------
// Тесты для mem_realloc
// ----------------------------------------------------------------------

TEST(ReallocTest, ReallocNull)
{
// realloc(NULL, size) эквивалентен malloc(size)
    void *p = mem_realloc(nullptr, 100);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned_to_max_align(p));
    mem_free(p);
}

TEST(ReallocTest, ReallocNullZero)
{
// realloc(NULL, 0) может вернуть NULL или уникальный указатель
    void *p = mem_realloc(nullptr, 0);
    mem_free(p); // free(NULL) безопасно
    SUCCEED();
}

TEST(ReallocTest, ReallocZero)
{
// realloc(ptr, 0) должен освободить ptr и вернуть NULL
    void *p = mem_malloc(100);
    ASSERT_NE(p, nullptr);
    void *p2 = mem_realloc(p, 0);
    EXPECT_EQ(p2, nullptr);
// p больше не действителен, повторный free(p) – UB, не делаем
}

TEST(ReallocTest, PreserveDataOnGrow)
{
    const size_t old_sz = 10, new_sz = 20;
    void *p = mem_malloc(old_sz);
    ASSERT_NE(p, nullptr);
    char *cp = static_cast<char *>(p);
    for (size_t i = 0; i < old_sz; ++i) cp[i] = static_cast<char>(i);
    void *p2 = mem_realloc(p, new_sz);
    ASSERT_NE(p2, nullptr);
    EXPECT_TRUE(is_aligned_to_max_align(p2));
    cp = static_cast<char *>(p2);
    for (size_t i = 0; i < old_sz; ++i) {
        EXPECT_EQ(cp[i], static_cast<char>(i)) << "data lost at index " << i;
    }
    mem_free(p2);
}

TEST(ReallocTest, PreserveDataOnShrink)
{
    dump_mem();
    const size_t old_sz = 20, new_sz = 5;
    void *p = mem_malloc(old_sz);
    ASSERT_NE(p, nullptr);
    char *cp = static_cast<char *>(p);
    for (size_t i = 0; i < old_sz; ++i) cp[i] = static_cast<char>(i);
    void *p2 = mem_realloc(p, new_sz);
    ASSERT_NE(p2, nullptr);
    cp = static_cast<char *>(p2);
    for (size_t i = 0; i < new_sz; ++i) {
        EXPECT_EQ(cp[i], static_cast<char>(i)) << "data lost at index " << i;
    }
    mem_free(p2);
    dump_mem();
}

TEST(ReallocTest, DataPreservedOnMove)
{
// Если realloc перемещает блок, данные должны быть скопированы
    void *p = mem_malloc(8);
    ASSERT_NE(p, nullptr);
// Записываем уникальный паттерн
    uint64_t *pattern = static_cast<uint64_t *>(p);
    *pattern = 0xDEADBEEFCAFEBABEULL;
    void *p2 = mem_realloc(p, 1024 * 1024); // большой размер, почти наверняка переместит
    ASSERT_NE(p2, nullptr);
    uint64_t *new_pattern = static_cast<uint64_t *>(p2);
    EXPECT_EQ(*new_pattern, 0xDEADBEEFCAFEBABEULL);
    mem_free(p2);
}

TEST(ReallocTest, SamePointerOnShrink)
{
// Стандарт не гарантирует, но хорошие аллокаторы часто не перемещают при уменьшении
// Этот тест просто фиксирует поведение, но не требует жёстко.
    void *p = mem_malloc(100);
    ASSERT_NE(p, nullptr);
    void *p2 = mem_realloc(p, 50);
    ASSERT_NE(p2, nullptr);
// Не проверяем равенство, просто освобождаем
    mem_free(p2);
    dump_mem();
}

TEST(ReallocTest, ReallocAfterFreeIsUB)
{
// Не тестируем, так как это неопределённое поведение.
// Просто проверяем, что аллокатор не крашится при корректном использовании.
    SUCCEED();
}

// ----------------------------------------------------------------------
// Тесты для mem_free
// ----------------------------------------------------------------------

TEST(FreeTest, FreeNull)
{
// free(NULL) не должен ничего делать
    mem_free(nullptr);
    SUCCEED();
}

TEST(FreeTest, FreeValidPointer)
{
    void *p = mem_malloc(32);
    ASSERT_NE(p, nullptr);
    mem_free(p);
    SUCCEED();
}

TEST(FreeTest, DoubleFreeIsUB)
{
// Не тестируем, пропускаем
    SUCCEED();
}

// ----------------------------------------------------------------------
// Комбинированные тесты
// ----------------------------------------------------------------------

TEST(CombinedTest, MallocReallocFreeChain)
{
    void *p = mem_malloc(100);
    ASSERT_NE(p, nullptr);
    p = mem_realloc(p, 200);
    ASSERT_NE(p, nullptr);
    mem_free(p);
    SUCCEED();
}

TEST(CombinedTest, CallocThenRealloc)
{
    void *p = mem_calloc(10, 4); // 40 байт, все нули
    ASSERT_NE(p, nullptr);
    char *cp = static_cast<char *>(p);
    for (int i = 0; i < 40; ++i) cp[i] = static_cast<char>(i);
    p = mem_realloc(p, 80);
    ASSERT_NE(p, nullptr);
    cp = static_cast<char *>(p);
    for (int i = 0; i < 40; ++i) {
        EXPECT_EQ(cp[i], static_cast<char>(i));
    }
    mem_free(p);
}

TEST(CombinedTest, RepeatedAllocations)
{
// Выделяем и освобождаем много раз – проверяем стабильность и отсутствие утечек
    for (int iter = 0; iter < 1000; ++iter) {
        void *p1 = mem_malloc(rand() % 1024 + 1);
        dump_mem();
        if (p1) {
            void *p2 = mem_realloc(p1, rand() % 2048 + 1);
            dump_mem();
            if (p2) {
                void *p3 = mem_calloc(rand() % 100 + 1, 8);
                dump_mem();

                if (p3) {
                    mem_free(p3);
                }
                mem_free(p2);
            }
            else {
                mem_free(p1);
            }
        }
    }
    SUCCEED();
}

// ----------------------------------------------------------------------
// Тесты на выравнивание для всех функций
// ----------------------------------------------------------------------

TEST(AlignmentTest, MallocAlignment)
{
    for (size_t sz: {1, 2, 4, 8, 16, 32, 64, 128}) {
        void *p = mem_malloc(sz);
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(is_aligned_to_max_align(p)) << "misaligned for size " << sz;
        mem_free(p);
    }
}

TEST(AlignmentTest, CallocAlignment)
{
    for (size_t sz: {1, 2, 4, 8, 16, 32}) {
        void *p = mem_calloc(sz, 1);
        ASSERT_NE(p, nullptr);
        EXPECT_TRUE(is_aligned_to_max_align(p)) << "misaligned for size " << sz;
        mem_free(p);
    }
}

TEST(AlignmentTest, ReallocAlignment)
{
    void *p = mem_malloc(8);
    ASSERT_NE(p, nullptr);
    p = mem_realloc(p, 64);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned_to_max_align(p));
    mem_free(p);
}

// ----------------------------------------------------------------------
// main для запуска тестов
// ----------------------------------------------------------------------

#define HEAP_SIZE (4096 + 1000000 + 2048)

int main(int argc, char **argv)
{
    std::unique_ptr<char[]> heap(new char[HEAP_SIZE]);
    mem_initialize(heap.get(), HEAP_SIZE);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}//
// Created by Dmitry A on 5/28/2026.
//

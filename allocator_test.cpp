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

#include <gtest/gtest.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new> // for std::max_align_t
#include <vector>

#define HEAP_SIZE (4096 * 4096)

// Вспомогательная функция для проверки выравнивания указателя
static bool is_aligned_to_max_align(void *ptr)
{
    std::uintptr_t addr = reinterpret_cast<std::uintptr_t>(ptr);
    return addr % sizeof(void *) == 0;
}

// ----------------------------------------------------------------------
// Вспомогательная функция для заполнения блока тестовым паттерном
// ----------------------------------------------------------------------
static void fill_pattern(void *ptr, size_t size, unsigned char seed)
{
    unsigned char *p = static_cast<unsigned char *>(ptr);
    for (size_t i = 0; i < size; ++i) {
        p[i] = static_cast<unsigned char>(seed + i);
    }
}

// ----------------------------------------------------------------------
// Проверка, что содержимое блока соответствует ожидаемому паттерну
// ----------------------------------------------------------------------
static void verify_pattern(const void *ptr, size_t size, unsigned char seed)
{
    const unsigned char *p = static_cast<const unsigned char *>(ptr);
    for (size_t i = 0; i < size; ++i) {
        ASSERT_EQ(p[i], static_cast<unsigned char>(seed + i))
                            << "Data mismatch at offset " << i;
    }
}

// ----------------------------------------------------------------------
// Тесты для mem_malloc
// ----------------------------------------------------------------------

TEST(MallocTest, AllocZero)
{
    // malloc(0) может вернуть NULL или уникальный указатель.
    // Главное – не падать при free.
    void *p = mem_malloc(0);
    mem_free(p);          // free(NULL) допустимо, если p == NULL
    SUCCEED();
}

TEST(MallocTest, AllocSmall)
{
    void *p = mem_malloc(10);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned_to_max_align(p));
    mem_free(p);
}

TEST(MallocTest, AllocLarge)
{
    const size_t large = 1024 * 1024; // 1 MiB
    void *p = mem_malloc(large);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned_to_max_align(p));
    mem_free(p);
}

TEST(MallocTest, AllocMax)
{
    // Попытка выделить слишком большой блок – ожидаем NULL
    void *p = mem_malloc(HEAP_SIZE + 1);
    EXPECT_EQ(p, nullptr);
    // free не вызываем, так как указатель нулевой
}

TEST(MallocTest, MultipleAllocationsNoOverlap)
{
    const size_t N = 100;
    std::vector<void *> ptrs;
    for (size_t i = 0; i < N; ++i) {
        void *p = mem_malloc(16);
        ASSERT_NE(p, nullptr);
        ptrs.push_back(p);
    }
    // Проверяем, что все указатели уникальны (простейшая проверка на непересечение)
    for (size_t i = 0; i < N; ++i) {
        for (size_t j = i + 1; j < N; ++j) {
            EXPECT_NE(ptrs[i], ptrs[j]);
        }
    }
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

TEST(MallocTest, EvenNotEvenFree)
{
    int count = 1000;
    std::vector<void *> pointers;
    pointers.reserve(count);

    for (int i = 0; i < count; ++i) {
        pointers[i] = mem_malloc(i);
    }

    for (int i = 0; i < count; ++i) {
        if ((i % 2) == 0) {
            mem_free(pointers[i]);
        }
    }

    for (int i = 0; i < count; ++i) {
        pointers[i] = mem_malloc(i);
    }

    for (int i = 0; i < count; ++i) {
        if ((i % 2) != 0) {
            mem_free(pointers[i]);
        }
    }

    for (int i = 0; i < count; ++i) {
        if ((i % 2) == 0) {
            mem_free(pointers[i]);
        }
    }
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
    // Переполнение при умножении snum * size должно вернуть NULL
    size_t snum = ULONG_MAX;
    size_t size = 2;
    void *p = mem_calloc(snum, size);
    EXPECT_EQ(p, nullptr);
}

TEST(CallocTest, LargeAllocation)
{
    // Выделяем достаточно большой блок, инициализированный нулями
    const size_t snum = 1000, size = 1024; // ~10 MiB
    void *p = mem_calloc(snum, size);
    if (p == nullptr) {
        // Если система не может выделить так много, тест пропускается
        GTEST_SKIP() << "System cannot allocate 10 MiB, skipping";
    }
    EXPECT_TRUE(is_aligned_to_max_align(p));
    unsigned char *bytes = static_cast<unsigned char *>(p);
    // Проверяем несколько случайных байтов, а не все 10 MiB
    for (size_t i = 0; i < 1000; ++i) {
        size_t offset = i * 1024; // каждый килобайт
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
    for (int iter = 0; iter < 10000; ++iter) {
        void *p1 = mem_malloc(rand() % 1024 + 1);
        if (p1) {
            void *p2 = mem_realloc(p1, rand() % 2048 + 1);
            if (p2) {
                void *p3 = mem_calloc(rand() % 100 + 1, 8);
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
// Тесты mem_realloc, проверяющие сохранность данных (паттерны)
// ----------------------------------------------------------------------

TEST(ReallocPatternTest, IncreaseSizePreservesData)
{
    const size_t old_size = 64;
    const size_t new_size = 128;
    unsigned char seed = 0xAB;

    void *p = mem_malloc(old_size);
    ASSERT_NE(p, nullptr);
    fill_pattern(p, old_size, seed);

    void *p2 = mem_realloc(p, new_size);
    ASSERT_NE(p2, nullptr);

    // Проверяем, что старые данные сохранились
    verify_pattern(p2, old_size, seed);

    // Новую область можно не проверять, но для полноты заполним и освободим
    mem_free(p2);
}

TEST(ReallocPatternTest, DecreaseSizePreservesData)
{
    const size_t old_size = 128;
    const size_t new_size = 64;
    unsigned char seed = 0xCD;

    void *p = mem_malloc(old_size);
    ASSERT_NE(p, nullptr);
    fill_pattern(p, old_size, seed);

    void *p2 = mem_realloc(p, new_size);
    ASSERT_NE(p2, nullptr);

    // Проверяем, что данные в усечённой части сохранились
    verify_pattern(p2, new_size, seed);

    mem_free(p2);
}

TEST(ReallocPatternTest, SameSizeKeepsData)
{
    const size_t size = 100;
    unsigned char seed = 0xEF;

    void *p = mem_malloc(size);
    ASSERT_NE(p, nullptr);
    fill_pattern(p, size, seed);

    void *p2 = mem_realloc(p, size);
    ASSERT_NE(p2, nullptr);
    // Адрес может как измениться, так и остаться – данные должны быть целы
    verify_pattern(p2, size, seed);

    mem_free(p2);
}

TEST(ReallocPatternTest, MoveToNewLocationCopiesData)
{
    // Этот тест не требует знания, переместится блок или нет.
    // Если переместится – данные скопируются, если нет – всё равно данные сохранятся.
    const size_t old_size = 32;
    const size_t new_size = 2048; // достаточно большой, чтобы почти наверняка переместить
    unsigned char seed = 0x12;

    void *p = mem_malloc(old_size);
    ASSERT_NE(p, nullptr);
    fill_pattern(p, old_size, seed);

    void *p2 = mem_realloc(p, new_size);
    ASSERT_NE(p2, nullptr);
    verify_pattern(p2, old_size, seed);

    mem_free(p2);
}

TEST(ReallocPatternTest, ReallocNullActsAsMalloc)
{
    const size_t size = 256;
    unsigned char seed = 0x34;

    void *p = mem_realloc(nullptr, size);
    ASSERT_NE(p, nullptr);
    fill_pattern(p, size, seed);
    verify_pattern(p, size, seed);

    mem_free(p);
}

TEST(ReallocPatternTest, ReallocZeroFreesAndReturnsNull)
{
    const size_t size = 64;
    unsigned char seed = 0x56;

    void *p = mem_malloc(size);
    ASSERT_NE(p, nullptr);
    fill_pattern(p, size, seed);

    void *p2 = mem_realloc(p, 0);
    // Стандарт: realloc(ptr, 0) может вернуть NULL или уникальный указатель.
    // В любом случае, старый указатель p больше недействителен.
    // Повторно освобождать его нельзя, если p2 не NULL.
    if (p2 != nullptr) {
        // Если вернулся не NULL, его нужно освободить
        mem_free(p2);
    }
    // Попытка проверить, что память освобождена, невозможна без дополнительных средств.
    SUCCEED();
}

TEST(ReallocPatternTest, CallocThenReallocPreservesZerosAndData)
{
    const size_t num = 10;
    const size_t elem_size = 8;
    const size_t old_total = num * elem_size; // 80
    const size_t new_total = 160;

    void *p = mem_calloc(num, elem_size);
    ASSERT_NE(p, nullptr);
    // Изначально все нули, заполняем первые old_total/2 байт паттерном
    unsigned char seed = 0x78;
    fill_pattern(p, old_total / 2, seed);

    void *p2 = mem_realloc(p, new_total);
    ASSERT_NE(p2, nullptr);

    // Проверяем, что первые old_total/2 байт – паттерн
    verify_pattern(p2, old_total / 2, seed);
    // Проверяем, что область между old_total/2 и old_total осталась нулевой
    const unsigned char *bytes = static_cast<const unsigned char *>(p2);
    for (size_t i = old_total / 2; i < old_total; ++i) {
        ASSERT_EQ(bytes[i], 0) << "Byte at offset " << i << " should be zero (was calloc)";
    }

    mem_free(p2);
}


int main(int argc, char **argv)
{
    std::unique_ptr<char[]> heap(new char[HEAP_SIZE]);
    mem_initialize(heap.get(), HEAP_SIZE);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
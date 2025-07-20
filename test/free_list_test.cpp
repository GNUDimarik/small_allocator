#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "../allocator.h"
#include "../free_list.h"

#include <cstdlib> // For rand() and srand()
#include <ctime> // For time()
#include <iostream>
#include <random>

using namespace utils::alloc_malloc::detail;

static constexpr const int kListSize = 10;
using vector_list_t = std::vector<std::shared_ptr<_MemoryBlock_t>>;

namespace {

auto create_node(size_t size) noexcept
{
    return std::make_shared<_MemoryBlock_t>(size);
}

void make_sorted_vector(vector_list_t& blockList) noexcept
{
    blockList.clear();
    blockList.reserve(kListSize);

    for (size_t i = 0; i < kListSize; ++i) {
        blockList.emplace_back(create_node(i));
    }
}

void make_shuffled_vector(vector_list_t& blockList) noexcept
{
    make_sorted_vector(blockList);
    std::random_device rd;
    std::mt19937 gen { rd() };
    std::shuffle(std::begin(blockList), std::end(blockList), gen);
}

void print_list(_MemoryBlock_t* __head) noexcept
{
    auto __p = __head;
    int count = 0;
    int max = kListSize;

    while (__p) {
        std::cout << "LIST: Block with size " << std::dec << __p->_M_size << " prev "
                  << std::hex << __p->_M_prev << " next " << std::hex
                  << __p->_M_next << " addr " << std::hex << __head << std::endl;
        __p = __p->_M_next;

        if (count++ >= max) {
            std::cout << "beak my max" << std::endl;
            break;
        }
    }
}

} // namespace

TEST(FreeListTest, sorted_insert_test)
{
    vector_list_t blocks;
    make_shuffled_vector(blocks);

    _MemoryBlock_t* head = blocks[0].get();

    for (size_t i = 1; i < blocks.size(); i++) {
        head = __list_insert_sorted(head, blocks[i].get());
    }

    vector_list_t srotedBlocks;
    make_sorted_vector(srotedBlocks);
    size_t index = 0;
    _MemoryBlock_t* p = head;

    while (p) {
        ASSERT_EQ(p->_M_size == srotedBlocks[index++]->_M_size, true);
        p = p->_M_next;
    }
}

TEST(FreeListTest, find_test)
{
    vector_list_t blocks;
    make_shuffled_vector(blocks);
    _MemoryBlock_t* head = blocks[0].get();

    for (size_t i = 1; i < blocks.size(); i++) {
        head = __list_insert_sorted(head, blocks[i].get());
    }

    for (const auto& b : blocks) {
        ASSERT_EQ(__list_find(head, b->_M_size) != nullptr, true);
    }
}

TEST(FreeListTest, erase_test)
{
    vector_list_t blocks;
    make_shuffled_vector(blocks);
    _MemoryBlock_t* head = blocks[0].get();

    for (size_t i = 1; i < blocks.size(); i++) {
        head = __list_insert_sorted(head, blocks[i].get());
    }

    for (size_t i = 0; i < blocks.size(); ++i) {
        head = __list_erase(blocks[i].get());
        ASSERT_EQ(__list_find(head, blocks[i]->_M_size), nullptr);
    }
}

TEST(FreeListTest, find_min_test)
{
    vector_list_t blocks;
    make_shuffled_vector(blocks);
    _MemoryBlock_t* head = blocks[0].get();

    for (size_t i = 1; i < blocks.size(); i++) {
        head = __list_insert_sorted(head, blocks[i].get());
    }

    ASSERT_EQ(__list_find_min(head)->_M_size == 0, true);
}

TEST(FreeListTest, reverse_list)
{
    vector_list_t blocks;
    make_sorted_vector(blocks);
    _MemoryBlock_t* head = blocks[0].get();

    for (size_t i = 1; i < blocks.size(); i++) {
        head = __list_insert_sorted(head, blocks[i].get());
    }

    head = __list_reverse(head);

    for (size_t i = blocks.size() - 1; i >= 0 && head; --i) {
        ASSERT_EQ(head->_M_size == blocks[i]->_M_size, true);
        head = head->_M_next;
    }
}

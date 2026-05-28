#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <functional>
#include "memory.h"

// define LOG_NDEBUG 1
#define LOG_TAG "memory"
#include "logging.h"


struct MemBlock;

MemBlock *gFreeList;

void *gMemStart{};

void *gMemEnd{};

static constexpr const size_t kPointerSize = sizeof(void *);

static constexpr const size_t kHeaderSize = kPointerSize;

static constexpr const size_t kFooterSize = kPointerSize;

static constexpr const size_t kOverheadSize = kHeaderSize + kFooterSize;

static constexpr const size_t kMinBlockSize = kPointerSize + kOverheadSize;

static constexpr const size_t kBlockAllocated = 1;

static constexpr const size_t kBlockFree = 0;

static constexpr size_t kBinCount = 256;

MemBlock **gBinList;

/**
 * Sorted linear list
 */

template<typename _Node, typename _Cmp>
_Node *__free_list_insert_sorted(_Node *head,
                                 _Node *block,
                                 _Cmp cmp)
{
    if (!head) {
        head = block;
        head->next = nullptr;
        return head;
    }

    auto cursor = head;

    if (cmp(block, cursor) && block != cursor) {
        block->next = cursor;
        return block;
    }

    while (cursor->next && !cmp(block, cursor->next)) {
        cursor = cursor->next;
    }

    block->next = cursor->next;
    cursor->next = block;
    return head;
}

template<typename _Node>
_Node *free_list_erase(_Node *head, _Node *block)
{
    if (head == block) {
        return head->next;
    }

    auto cursor = head;
    auto prev = cursor;

    while (cursor && cursor != block) {
        prev = cursor;
        cursor = cursor->next;
    }

    prev->next = block->next;
    return head;
}

template<typename _Node, typename _Cmp>
_Node *__free_list_find_first_fit(_Node *head,
                                  size_t size,
                                  _Cmp cmp)
{
    auto cursor = head;

    while (cursor) {
        if (cmp(cursor, size)) {
            return cursor;
        }

        cursor = cursor->next;
    }

    return nullptr;
}

/**
 * Memory block related stuff
 */

static size_t *mem_block_size_t_ptr(void *_p)
{
    return reinterpret_cast<size_t *>(_p);
}

static char *mem_block_char_ptr(void *_p)
{
    return reinterpret_cast<char *>(_p);
}

static char *mem_block_user_ptr(void *_p)
{
    return mem_block_char_ptr(_p) + kHeaderSize;
}

static void mem_block_pack(void *_p, size_t _sz, size_t _st)
{
    *mem_block_size_t_ptr(_p) = _sz | _st;
}

static char *mem_block_header(void *_p)
{
    return mem_block_char_ptr(_p) - kHeaderSize;
}

static size_t mem_block_size(void *_p)
{
    return (*mem_block_size_t_ptr(mem_block_header(_p)) & ~0x01);
}

static char *mem_block_footer(void *_p)
{
    return mem_block_char_ptr(_p) + mem_block_size(_p) - kFooterSize;
}

static bool mem_block_is_allocated(void *p)
{
    return (*mem_block_size_t_ptr(mem_block_header(p)) & 0x01) == kBlockAllocated;
}

static bool mem_block_is_free(void *p)
{
    return !mem_block_is_allocated(p);
}

static void mem_block_put_to_header(void *_p, size_t _sz, size_t state)
{
    mem_block_pack(mem_block_header(_p), _sz, state);
}

static void mem_block_put_to_footer(void *_p, size_t _sz, size_t state)
{
    mem_block_pack(mem_block_footer(_p), _sz, state);
}

static void *mem_block_next(void *_p)
{
    return mem_block_char_ptr(mem_block_header(_p)) + mem_block_size(_p) + kOverheadSize;
}

static inline void *mem_block_init_block(void *_p, size_t _sz,
                                         size_t state)
{
    mem_block_put_to_header(_p, _sz, state);
    mem_block_put_to_footer(_p, _sz, state);
    return _p;
}

static void *mem_block_prev(void *_p)
{
    char *header = mem_block_char_ptr(mem_block_header(_p));
    return (header - mem_block_size(header));
}

static inline size_t mem_block_size_with_overhead(void *ptr)
{
    return mem_block_size(ptr) + kOverheadSize;
}

struct MemBlock
{
    size_t size;
    MemBlock *next;
    size_t footer;

    static MemBlock *createWithNullNext(void *addr)
    {
        auto block = new(mem_block_header(addr)) MemBlock;
        block->next = nullptr;
        return block;
    }

    static MemBlock *place(void *addr)
    {
        return new(mem_block_header(addr)) MemBlock;
    }

    void *toBlock()
    {
        return mem_block_char_ptr(this) + kHeaderSize;
    }
};

template<typename _Node>
_Node *free_list_insert_sorted_by_size(_Node *head,
                                       _Node *block)
{
    block->next = nullptr;

    if (!head) {
        return block;
    }

    if (head != block) {
        auto cmp = [](_Node *first, _Node *second)
        { return mem_block_size(first->toBlock()) <= mem_block_size(second->toBlock()); };
        return __free_list_insert_sorted<_Node, decltype(cmp)>(head, block, cmp);
    }

    return block;
}

template<typename _Node>
_Node *free_list_insert_sorted_by_addr(_Node *head,
                                       _Node *block)
{
    block->next = nullptr;

    if (!head) {
        return block;
    }

    if (head != block) {
        auto cmp = [](_Node *first, _Node *second)
        { return first < second; };
        return __free_list_insert_sorted<_Node, decltype(cmp)>(head, block, cmp);
    }

    return block;
}

template<typename _Node>
_Node *free_list_insert(_Node *block)
{
    return free_list_insert_sorted_by_size(gFreeList, block);
}

template<typename _Node>
_Node *free_list_find_first_fit(_Node *head, size_t size)
{
    auto cmp = [](_Node *block, size_t size)
    { return mem_block_size(block->toBlock()) >= size; };
    return __free_list_find_first_fit(head, size, cmp);
}

template<typename _Node>
_Node *bin_insert(_Node *block)
{
    // Block needs to be free here otherwise size is invalid since allocated bit is set
    size_t index = block->size;

    if (index < (kBinCount - 2)) {
        gBinList[index] = free_list_insert_sorted_by_addr(gBinList[index], block);
    }
    else {
        index = kBinCount - 1;
        gBinList[index] = free_list_insert_sorted_by_size(gBinList[index], block);
    }

    return block;
}

static size_t bin_index_from_size(size_t size)
{
    if (size < (kBinCount - 2)) {
        return size;
    }

    return kBinCount - 1;
}

static void bin_erase(MemBlock *block)
{
    size_t index = bin_index_from_size(mem_block_size(block->toBlock()));
    auto bin = gBinList[index];

    if (bin) {
        MemBlock *prev = nullptr;

        while (bin) {
            if (bin == block) {
                break;
            }

            prev = bin;
            bin = bin->next;
        }

        if (bin) {
            prev ? prev->next = bin->next : gBinList[index] = bin->next;
        }
    }
}

static MemBlock *bin_find_free_block_and_erase(size_t size)
{
    size_t index = bin_index_from_size(size);
    auto block = gBinList[index];

    if (block) {
        MemBlock *prev = nullptr;

        while (block) {
            if (block->size >= size) {
                break;
            }

            prev = block;
            block = block->next;
        }

        if (block) {
            prev ? prev->next = block->next : gBinList[index] = block->next;
        }
    }

    return block;
}

static void *mem_block_place(void *block, size_t sz)
{
    size_t cur_size = mem_block_size(block);

    if (sz < cur_size) {
        size_t new_size = cur_size - sz - kHeaderSize;
        mem_block_put_to_footer(block, new_size, kBlockFree);
        mem_block_put_to_header(block, sz, kBlockAllocated);
        mem_block_put_to_footer(block, sz, kBlockAllocated);
        mem_block_put_to_header(mem_block_next(block), new_size, kBlockFree);
        return block;
    }
    else if (sz == cur_size) {
        mem_block_put_to_header(block, sz, kBlockAllocated);
        mem_block_put_to_footer(block, sz, kBlockAllocated);
        return block;
    }
    else {
        ALOGD("%s(): cur_size is %zu size is %zu. Ignore request\n",
              __func__, cur_size, sz);
    }

    return nullptr;
}

static void *mem_block_merge(void *ptr)
{
    bool next_allocated = mem_block_is_allocated(mem_block_next(ptr));
    bool prev_allocated = mem_block_is_allocated(mem_block_prev(ptr));
    size_t size = mem_block_size(ptr);

    if (prev_allocated && next_allocated) {
        /* Do nothing */
    }

    else if (prev_allocated && !next_allocated) {
        size += mem_block_size(mem_block_next(ptr)) + kFooterSize;
        void *footer = mem_block_footer(ptr);
        mem_block_put_to_header(ptr, size, kBlockFree);
        mem_block_pack(footer, size, kBlockFree);
    }

    else if (!prev_allocated && next_allocated) {
        void *prev = mem_block_prev(ptr);
        void *footer = mem_block_footer(ptr);
        size += mem_block_size(prev) + kFooterSize;
        mem_block_put_to_header(prev, size, kBlockFree);
        mem_block_pack(footer, size, kBlockFree);
        return prev;
    }

    else if (!prev_allocated && !next_allocated) {
        void *prev = mem_block_prev(ptr);
        void *header = mem_block_header(prev);
        void *footer = mem_block_footer(mem_block_next(ptr));
        size += mem_block_size(mem_block_prev(ptr)) +
            mem_block_size(mem_block_next(ptr)) + kOverheadSize;
        mem_block_pack(header, size, kBlockFree);
        mem_block_pack(footer, size, kBlockFree);
        return prev;
    }

    return ptr;
}

static size_t mem_block_aligned_size(size_t size)
{
    size_t aligned_size = 0;

    if (size < kOverheadSize) {
        aligned_size = kMinBlockSize;
    }
    else {
        aligned_size = kPointerSize * ((size + kOverheadSize + kPointerSize - 1) / kPointerSize);
    }

    return aligned_size;
}
// TODO
static void *mem_block_erase_merge(void *block)
{
    auto current = mem_block_prev(block);

    if (mem_block_is_free(current)) {
        bin_erase(MemBlock::place(current));
    }

    current = mem_block_next(block);

    if (mem_block_is_free(current)) {
        bin_erase(MemBlock::place(current));
    }

    return mem_block_merge(block);
}

void *mem_malloc(size_t size)
{
    void *block = nullptr;

    if (gMemStart) {
        if (size > 0) {
            size_t aligned_size = mem_block_aligned_size(size);
            auto memoryBlock = bin_find_free_block_and_erase(aligned_size);
            bool blockFromBin = memoryBlock;

            if (!memoryBlock) {
                memoryBlock = free_list_find_first_fit(gFreeList, aligned_size);

                if (memoryBlock) {
                    gFreeList = free_list_erase(gFreeList, memoryBlock);
                }
            }

            if (memoryBlock) {
                block = memoryBlock->toBlock();

                if (block != nullptr) {
                    block = mem_block_place(block, aligned_size);

                    auto next = mem_block_next(block);

                    if (mem_block_is_free(next)) {
                        auto nextBlock = MemBlock::createWithNullNext(next);

                        if (blockFromBin) {
                            bin_insert(nextBlock);
                        }
                        else {
                            gFreeList = free_list_insert(nextBlock);
                        }
                    }
                }
            }
            else {
                ALOGE("Out of memory on allocation request with size %zu aligned size %zu", size, aligned_size);
                errno = ENOMEM;
            }
        }
        else {
            ALOGE("Could not allocate block with size %zu", size);
            errno = EINVAL;
        }
    }
    else {
        errno = EINVAL;
        ALOGE("Not initialized");
    }

    return block;
}

void *mem_calloc(size_t num, size_t size)
{
    size_t sz = size * num;
    void *p = mem_malloc(sz);

    if (p != nullptr) {
        sz = mem_block_aligned_size(sz);
        memset(p, 0, sz);
    }

    return p;
}

void *mem_realloc(void *p, size_t new_sz)
{
    if (p) {
        if (new_sz > 0) {
            size_t size = mem_block_size(p);

            if (new_sz != size) {
                void *newBlock = nullptr;

                if (new_sz > size) {
                    newBlock = mem_block_erase_merge(p);
                    auto aligned_size = mem_block_aligned_size(new_sz);
                    auto newBlockSize = mem_block_size(newBlock);
                    ALOGD("block (%p) size %zu newBlock (%p) size %zu", p, size, newBlock, newBlockSize);

                    if (newBlockSize >= aligned_size) {
                        newBlock = mem_block_place(newBlock, aligned_size);

                        if (newBlock && newBlock != p) {
                            return memmove(newBlock, p, size);
                        }

                        return p;
                    }
                }

                ALOGD("new_sz %zu aligned size %zu size %zu", new_sz, mem_block_aligned_size(new_sz), size);
                newBlock = mem_malloc(new_sz);

                if (newBlock) {
                    memmove(newBlock, p, std::min(size, new_sz));
                    mem_free(p);
                    return newBlock;
                }
            }
            else {
                ALOGD("new size is equal to current size. do nothing");
            }
        }
        else {
            ALOGE("Wrong size %zu", new_sz);
            errno = EINVAL;
        }
    }
    else {
        errno = EINVAL;
        ALOGE("Invalid pointer (%p)", p);
    }

    return nullptr;
}

void mem_free(void *ptr)
{
    if (ptr != nullptr && mem_block_is_allocated(ptr)) {
        size_t size = mem_block_size(ptr);
        mem_block_init_block(ptr, size, kBlockFree);
        ptr = mem_block_erase_merge(ptr);
        bin_insert(MemBlock::createWithNullNext(ptr));
    }
    else {
        ALOGE("%s(): Invalid pointer (%p)\n", __func__, ptr);
    }
}

int mem_initialize(void *base, size_t size)
{
    if (base && size > 0) {
        size_t binsSize = kBinCount * sizeof(void *);

        if (size > binsSize) {
            gMemStart = mem_block_char_ptr(base) + binsSize;
            gBinList = reinterpret_cast<MemBlock **>(base);
            memset(gBinList, 0, binsSize);
            ALOGD("gBinList %p binsSize %zu", gBinList, binsSize);
            mem_block_init_block(mem_block_user_ptr(gMemStart), kPointerSize, kBlockAllocated);
            gMemEnd = mem_block_char_ptr(gMemStart) + size - (kOverheadSize + kPointerSize);
            mem_block_init_block(mem_block_user_ptr(gMemEnd), kPointerSize, kBlockAllocated);
            void *heap = mem_block_next(mem_block_user_ptr(gMemStart));
            size_t reserved_size = (kOverheadSize + kPointerSize) << 1;
            size_t heap_size = size - reserved_size;
            mem_block_init_block(heap, heap_size, kBlockFree);
            gFreeList = free_list_insert(MemBlock::createWithNullNext(heap));
            return 0;
        }
    }
    else {
        ALOGE("Could not initialize memory with params base %p size %zu", base, size);
    }

    ALOGD("gMemStart %p gMemEnd %p size %td",
          gMemStart,
          gMemEnd,
          mem_block_char_ptr(gMemEnd) - mem_block_char_ptr(gMemStart));

    return EINVAL;
}

void mem_unuinitialize()
{
    gMemStart = nullptr;
    gMemEnd = nullptr;
}

void mem_dump()
{
    void *cur_blk = nullptr;
    size_t total_memory = 0;
    size_t total_with_overhead = 0;
    size_t total_blocks = 0;
    size_t total_free_blocks = 0;
    size_t total_allocated_blocks = 0;

    ALOGD(
        "*************************MEMORY DUMP*************************");

    if (gMemStart != nullptr && gMemEnd != nullptr) {
        for (cur_blk = mem_block_user_ptr(gMemStart); cur_blk <= mem_block_user_ptr(gMemEnd); total_blocks++) {
            size_t blk_size = mem_block_size(cur_blk);
            size_t blk_size_with_overhead = mem_block_size_with_overhead(cur_blk);
            total_memory += blk_size;
            total_with_overhead += blk_size_with_overhead;
            auto is_allocated = mem_block_is_allocated(cur_blk);
            const char *format = cur_blk == gMemStart || cur_blk == gMemEnd
                                 ? "service block address %p size %12lu \t size with overhead %8lu state %s"
                                 : "block address         %p size %12lu \t size with overhead %8lu state %s";

            ALOGD(format, cur_blk,
                  blk_size, blk_size_with_overhead,
                  is_allocated == kBlockAllocated ? "allocated" : "free");

            if (is_allocated == kBlockAllocated) {
                ++total_allocated_blocks;
            }
            else {
                ++total_free_blocks;
            }

            cur_blk = mem_block_next(cur_blk);
        }
    }
    else {
        ALOGD("Not initialized");
    }

    ALOGD(
        "**********************END OF MEMORY DUMP**********************");

    ALOGD("total memory               %12zu bytes", total_memory);
    ALOGD("total memory with overhead %12zu bytes", total_with_overhead);
    ALOGD("total total_blocks count   %12zu", total_blocks);
    ALOGD("total allocated blocks     %12zu", total_allocated_blocks);
    ALOGD("total free blocks          %12zu", total_free_blocks);
}

static size_t dump_list(MemBlock *list, size_t index = 0)
{
    size_t count = 0;
    auto ptr = list;

    while (ptr) {
        ++count;
        void *block = &ptr->next;
        ALOGD("block %p size %zu size with overhead %zu mem bin[%zu]",
              block,
              mem_block_size(block),
              mem_block_size_with_overhead(block), index);
        ptr = ptr->next;
    }

    return count;
}

void dump_bins()
{
    size_t count = 0;

    for (size_t index = 0; index < kBinCount; ++index) {
        if (gBinList[index]) {
            count += dump_list(gBinList[index], index);
        }
    }

    ALOGD("Total free blocks in all bins %zu", count);
}

void dump_free_mem()
{
    dump_list(gFreeList);
}

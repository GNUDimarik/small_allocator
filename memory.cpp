#include <errno.h>
#include <string.h>

#include "memory.h"

// #define LOG_NDEBUG 1
#define LOG_TAG "memory"
#include "logging.h"

#ifndef min
#define min(a, b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})
#endif

struct ListHead
{
    ListHead *next = nullptr;
    ListHead *prev = nullptr;
};

char *gMemStart{};

char *gMemEnd{};

static constexpr const size_t kPointerSize = sizeof(void *);

static constexpr const size_t kHeaderSize = kPointerSize;

static constexpr const size_t kFooterSize = kPointerSize;

static constexpr const size_t kOverheadSize = kHeaderSize + kFooterSize;

static constexpr const size_t kBlockAllocated = 1;

static constexpr const size_t kBlockFree = 0;

static constexpr const size_t kBinCount = 256;

static constexpr const size_t kHugeBlockMinSize = 256;

static constexpr size_t kHugeBinIndex = kBinCount - 1;

static constexpr size_t kMaxMessageLen = 256;

#ifdef BINS_ARE_IN_HEAP
ListHead **gBinList;
#else

ListHead *gBinList[kBinCount];

#endif

/**
 * Memory block related stuff
 */

static size_t *mem_block_size_t_ptr(void *_p)
{
    return reinterpret_cast<size_t *>(_p);
}

static size_t mem_block_get_size(void *_p)
{
    return *mem_block_size_t_ptr(_p) & ~0x01;
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
    return mem_block_get_size(mem_block_header(_p));
}

static char *mem_block_footer(void *_p)
{
    return mem_block_char_ptr(_p) + mem_block_size(_p);
}

static size_t mem_block_get_alloc(void *p)
{
    return (*mem_block_size_t_ptr(p) & 0x01);
}

static bool mem_block_is_allocated(void *p)
{
    return mem_block_get_alloc(mem_block_header(p)) == kBlockAllocated;
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
    return mem_block_char_ptr(_p) + mem_block_size(_p) + kOverheadSize;
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
    char *prev_footer = header - kFooterSize;
    size_t prev_size = mem_block_get_size(prev_footer);
    return prev_footer - prev_size;
}

static inline size_t mem_block_size_with_overhead(void *ptr)
{
    return mem_block_size(ptr) + kOverheadSize;
}

static inline ListHead *mem_block_list_head(void *ptr)
{
    auto head = reinterpret_cast<ListHead *>(ptr);
    head->next = nullptr;
    head->prev = nullptr;
    return head;
}

static ListHead *list_erase(ListHead *head)
{
    auto prev = head->prev;
    auto next = head->next;

    if (prev) {
        prev->next = next;
    }

    if (next) {
        next->prev = prev;
    }

    head->next = nullptr;
    head->prev = nullptr;
    return next;
}

template<typename _Node>
_Node *free_list_prepend(_Node *head,
                         _Node *block)
{
    block->next = nullptr;

    if (!head) {
        return block;
    }

    if (head != block) {
        block->next = head;
        head->prev = block;
    }

    return block;
}

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

    if (cmp(block, cursor)) {
        return free_list_prepend(cursor, head);
    }

    while (cursor->next && !cmp(block, cursor->next)) {
        cursor = cursor->next;
    }

    block->next = cursor->next;
    cursor->next = block;
    block->prev = cursor;

    if (cursor->next) {
        cursor->next->prev = block;
    }

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
        { return mem_block_size(first) <= mem_block_size(second); };
        return __free_list_insert_sorted<_Node, decltype(cmp)>(head, block, cmp);
    }

    return block;
}

template<typename _Node>
_Node *bin_insert(_Node *block)
{
    size_t index = mem_block_size(block);

    if (index < kHugeBinIndex) {
        gBinList[index] = free_list_prepend(gBinList[index], block);
    }
    else {
        gBinList[kHugeBinIndex] = free_list_insert_sorted_by_size(gBinList[kHugeBinIndex], block);
    }

    return block;
}

static size_t bin_index_from_size(size_t size)
{
    if (size >= kHugeBlockMinSize) {
        return kHugeBinIndex;
    }

    return size;
}

static void bin_erase(void *block)
{
    auto *head = reinterpret_cast<ListHead *>(block);
    size_t index = bin_index_from_size(mem_block_size(block));
    list_erase(head);

    if (gBinList[index] == head) {
        gBinList[index] = head->next;
    }

    head->next = nullptr;
    head->prev = nullptr;
}

static ListHead *bin_find(size_t index, size_t size)
{
    auto block = gBinList[index];

    while (block) {
        if (mem_block_size(block) >= size) {
            break;
        }

        block = block->next;
    }

    return block;
}

static ListHead *bin_find_free_block(size_t size)
{
    size_t index = bin_index_from_size(size);
    auto block = gBinList[index];

    if (block) {
        if (index == kHugeBinIndex) {
            block = bin_find(index, size);
        }
    }
    else {
        for (size_t i = index; i <= kHugeBinIndex && !block; ++i) {
            block = bin_find(i, size);
        }
    }

    return block;
}

static void *mem_block_place(void *block, size_t sz)
{
    size_t cur_size = mem_block_size(block);
    size_t remain = cur_size - sz;

    if (remain >= kOverheadSize * 2) {
        remain -= kOverheadSize;
        mem_block_put_to_header(block, sz, kBlockAllocated);
        mem_block_put_to_footer(block, sz, kBlockAllocated);
        auto next = mem_block_next(block);
        mem_block_put_to_header(next, remain, kBlockFree);
        mem_block_put_to_footer(next, remain, kBlockFree);
        return block;
    }

    mem_block_put_to_header(block, cur_size, kBlockAllocated);
    mem_block_put_to_footer(block, cur_size, kBlockAllocated);

    return block;
}

static void *mem_block_merge(void *ptr)
{
    auto next = mem_block_next(ptr);
    auto prev = mem_block_prev(ptr);
    bool next_allocated = mem_block_is_allocated(next);
    bool prev_allocated = mem_block_is_allocated(prev);
    size_t size = mem_block_size(ptr);

    if (prev_allocated && next_allocated) {
        /* Do nothing */
    }

    else if (prev_allocated && !next_allocated) {
        size += mem_block_size(next) + kOverheadSize;
        mem_block_put_to_header(ptr, size, kBlockFree);
        void *footer = mem_block_footer(ptr);
        mem_block_pack(footer, size, kBlockFree);
    }

    else if (!prev_allocated && next_allocated) {
        void *footer = mem_block_footer(ptr);
        size += mem_block_size(prev) + kOverheadSize;
        mem_block_put_to_header(prev, size, kBlockFree);
        mem_block_pack(footer, size, kBlockFree);
        return prev;
    }

    else if (!prev_allocated && !next_allocated) {
        void *header = mem_block_header(prev);
        void *footer = mem_block_footer(next);
        size += mem_block_size(prev) +
            mem_block_size(next) + kOverheadSize * 2;
        mem_block_pack(header, size, kBlockFree);
        mem_block_pack(footer, size, kBlockFree);
        return prev;
    }

    return ptr;
}

static size_t mem_block_aligned_size(size_t size)
{
    size_t aligned_size = 0;

    if (size <= kOverheadSize) {
        aligned_size = kOverheadSize;
    }
    else {
        aligned_size = kOverheadSize * ((size + (kOverheadSize) + (kOverheadSize - 1)) / kOverheadSize);
    }

    return aligned_size;
}

static void *mem_block_erase_merge(void *block)
{
    auto current = mem_block_prev(block);

    if (mem_block_is_free(current)) {
        bin_erase(current);
    }

    current = mem_block_next(block);

    if (mem_block_is_free(current)) {
        bin_erase(current);
    }

    return mem_block_merge(block);
}

void *mem_malloc(size_t size)
{
    void *block = nullptr;

    if (gMemStart) {
        if (size > 0) {
            size_t aligned_size = mem_block_aligned_size(size);
            auto memoryBlock = bin_find_free_block(aligned_size);
            bool blockFromBin = memoryBlock;

            if (memoryBlock) {
                block = memoryBlock;
                bin_erase(block);
                block = mem_block_place(block, aligned_size);
                auto next = mem_block_next(block);

                if (next < gMemEnd && next > gMemStart && mem_block_is_free(next)) {
                    auto nextBlock = mem_block_list_head(next);
                    bin_insert(nextBlock);
                }
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
    size_t count = size * num;
    void *p = mem_malloc(count);

    if (p != nullptr) {
        memset(p, 0, count);
    }

    return p;
}

void *mem_realloc(void *p, size_t new_sz)
{
    if (!p) {
        return mem_malloc(new_sz);
    }

    auto block = mem_malloc(new_sz);

    if (block) {
        memmove(block, p, min(new_sz, mem_block_size(p)));
        mem_free(p);
    }

    return block;
}

void mem_free(void *ptr)
{
    if (ptr != nullptr && mem_block_is_allocated(ptr)) {
        size_t size = mem_block_size(ptr);
        mem_block_init_block(ptr, size, kBlockFree);
        ptr = mem_block_erase_merge(ptr);
        bin_insert(mem_block_list_head(ptr));
    }
    else {
        ALOGE("%s(): Invalid pointer (%p)\n", __func__, ptr);
    }
}

int mem_initialize(void *base, size_t size)
{
    if (base && size > 0 && (size % 2 == 0)) {
        size_t binsSize = kBinCount * sizeof(void *);

        if (size > binsSize) {
            gMemStart = mem_block_char_ptr(base);
#if BINS_ARE_IN_HEAP
            gMemStart = mem_block_char_ptr(base) + binsSize;
            size -= binsSize;
            gBinList = reinterpret_cast<ListHead **>(base);
            ALOGD("gBinList %p binsSize %zu gMemStart %p", gBinList, binsSize, gMemStart);
#endif
            memset(gBinList, 0, binsSize);
            mem_block_pack(gMemStart, kOverheadSize, kBlockAllocated);
            mem_block_pack(gMemStart + kHeaderSize + kOverheadSize, kOverheadSize, kBlockAllocated);
            size_t heapSize = size - (kOverheadSize * 5);
            void *heap = mem_block_next(mem_block_user_ptr(gMemStart));
            mem_block_init_block(heap, heapSize, kBlockFree);
            gMemEnd = mem_block_char_ptr(mem_block_next(heap)) - kHeaderSize;
            mem_block_pack(gMemEnd, kOverheadSize, kBlockAllocated);
            mem_block_pack(gMemEnd + kOverheadSize + kHeaderSize, kOverheadSize, kBlockAllocated);
            auto firstBlock = mem_block_list_head(heap);
            bin_insert(firstBlock);
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

void dump_mem()
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
            const char *format = cur_blk == mem_block_user_ptr(gMemStart) || cur_blk == mem_block_user_ptr(gMemEnd)
                                 ? "service block address %p size %12lu \t size with overhead %8lu state %s"
                                 : "block address         %p size %12lu \t size with overhead %8lu state %s";

            ALOGD(format, cur_blk,
                  blk_size, blk_size_with_overhead,
                  is_allocated ? "allocated" : "free");

            if (is_allocated) {
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

static size_t dump_list(ListHead *list, size_t index = 0)
{
    size_t count = 0;
    auto ptr = list;

    while (ptr) {
        ++count;
        void *block = ptr;
        ALOGD("block %p size %zu size with overhead %zu mem bin[%zu] prev addr %p next addr %p",
              block,
              mem_block_size(block),
              mem_block_size_with_overhead(block), index, ptr->prev, ptr->next);
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

static char *mem_print_block_to_str(void *p, char *str)
{
    auto header = mem_block_header(p);
    auto footer = mem_block_footer(p);
    sprintf(str, "block (%p) header (%p) [%zu:%zu] footer (%p) [%zu:%zu]",
            p,
            header,
            mem_block_get_size(header),
            mem_block_get_alloc(header),
            footer,
            mem_block_get_size(footer),
            mem_block_get_alloc(footer));
    return str;
}

bool mem_check_block(void *p)
{
    if (p) {
        if ((reinterpret_cast<size_t>(p) % kPointerSize) == 0) {
            if (*mem_block_header(p) == *mem_block_footer(p)) {
                return true;
            }
            else {
                ALOGE("Bad block. Header and footer are not the same");
            }
        }
        else {
            ALOGE("Bad block (%p). The address is not aligned by %zu", p, kPointerSize);
        }
    }
    else {
        ALOGE("Bad block (%p)", p);
    }

    return false;
}

bool mem_check(bool verbose)
{
    char buffer[kMaxMessageLen];

    if (gMemStart != nullptr && gMemEnd != nullptr) {
        for (void *cur_blk = mem_block_user_ptr(gMemStart); cur_blk <= mem_block_user_ptr(gMemEnd);
             cur_blk = mem_block_next(cur_blk)) {
            if (verbose) {
                mem_print_block_to_str(cur_blk, buffer);

                if (!mem_check_block(cur_blk)) {
                    ALOGD("block %s BAD", buffer);
                    return false;
                }

                ALOGD("%s OK", buffer);
            }
        }
    }

    return true;
}
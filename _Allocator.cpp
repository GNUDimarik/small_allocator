#include "_Allocator.h"
#include "Utils.h"

using namespace mem::utils;
using namespace mem::details;

_Allocator::_Allocator(void *__start, size_t __size_in_bytes) noexcept
{
    // first and last blocks must be always allocated
    _M_heap_start = _MemBlock::__construct(__start); // take in account size of prev block
    _M_heap_start->__put_to_header(_MemBlock::_S_total_overhead_size);
    _M_heap_start->__allocate();

    size_t __reserved_block_size = _MemBlock::_S_total_overhead_size << 1;
    auto __heap = _M_heap_start->__next_implicit();
    __heap->__put_to_header(__size_in_bytes - (__reserved_block_size << 1) - _MemBlock::_S_total_overhead_size);
    __heap->_M_prev_blk_size = _M_heap_start->__size();

    _M_heap_end = __heap->__next_implicit();
    _M_heap_end->__put_to_header(_MemBlock::_S_total_overhead_size);
    _M_heap_end->_M_prev_blk_size = __heap->__size();
    _M_heap_end->__allocate();
    _M_free_list_manager.set_head(__heap);
}

void *_Allocator::malloc(size_t __size) noexcept
{
    if (__size > 0) {
        auto __fit = _M_find_first_fit(_MemBlock::__aligned_size(__size));

        if (__fit) {
            return __fit->__slice(__size)->__user_memory();
        }

        ALOGE("Out of memory");
    }

    return nullptr;
}

void *_Allocator::calloc(size_t __num, size_t __size) noexcept
{
    size_t __sz = __size * __num;
    void *p = malloc(__sz);

    if (p != nullptr) {
        __sz = _MemBlock::__aligned_size(__sz);
        memset(p, 0, __sz);
    }


    return p;
}

void *_Allocator::realloc(void *__p, size_t __new_sz) noexcept
{
    if (__p == nullptr) {
        ALOGD("Block '%p' is null. Allocating memory with size(%lu)...", __p, __new_sz);
        return malloc(__new_sz);
    }

    auto __blk = _MemBlock::__from_user_memory(__p);

    if (__blk && __blk->__is_allocated()) {
        if (__new_sz > 0) {
            if (__blk->__size() == __new_sz) {
                ALOGD("new block size exactly as needed!");
                return __p;
            }

            if (__new_sz > __blk->__size()) {
                ALOGD("new size is more than current. Trying to consalidate ...");
                __blk->__try_consolidate();
            }

            if (__blk->__size() >= __new_sz) {
                ALOGD("new block size more than needed, slicing ...");
                auto __b = __blk->__slice(__new_sz);

                // Check. Aligned size may be more than possible to slice
                if (__b) {
                    return __b->__user_memory();
                }
            }

            // Here new block needs to be allocated and previous block needs to be copied to
            void *__new_blk = malloc(__new_sz);

            if (__new_blk) {
                ALOGD("Trying to allocate new block and copy contents of old one there");
                memcpy(__new_blk, __blk->__user_memory(), __blk->__size());
                free(__blk->__user_memory());
                return __new_blk;
            }

            ALOGD("No memory for new block. mem_realloc failed!");
        }
    }
    else {
        // if block is not allocated do nothing, just return nullptr. actually it's UB
        ALOGE("Block '%p' is not allocated!", __p);
    }

    return nullptr;
}

void _Allocator::free(void *__p) noexcept
{
    // we should be able savely called with nullptr
    if (__p != nullptr) {
        auto __blk = _MemBlock::__from_user_memory(__p);

        if (__blk && __blk->__is_allocated()) {
            __blk->__unallocate();
            __blk->__try_consolidate();
        }
    }
    else {
        ALOGE("Unknown block '%p'", __p);
    }
}
mem::details::_MemBlock *_Allocator::_M_find_first_fit(size_t __size) const noexcept
{
    if (_M_heap_start != nullptr && _M_heap_end != nullptr) {
        for (auto __cur_blk = _M_heap_start; __cur_blk != _M_heap_end->__next_implicit();
             __cur_blk = __cur_blk->__next_implicit()) {
            if (__cur_blk->__is_free() && __cur_blk->__size() >= __size) {
                return __cur_blk;
            }
        }
    }

    return nullptr;
}

void _Allocator::dump() const noexcept
{
    size_t __total_memory = 0;
    size_t __total_free_memory = 0;
    size_t __total_allocated_memory = 0;
    size_t __total_with_overhead = 0;
    size_t __total_blocks = 0;
    ALOGD(
        "*************************MEMORY DUMP*************************");
    if (_M_heap_start != nullptr && _M_heap_end != nullptr) {
        for (auto __cur_blk = _M_heap_start; __cur_blk != _M_heap_end->__next_implicit();
             __cur_blk = __cur_blk->__next_implicit(), __total_blocks++) {
            utils::Utils::dumpMemoryBlock(__cur_blk);
            __total_memory += __cur_blk->__size();
            __total_with_overhead += __cur_blk->__size_with_overhead();

            if (__cur_blk->__is_allocated()) {
                __total_allocated_memory += __cur_blk->__size();
            }
            else if (__cur_blk->__is_free()) {
                __total_free_memory += __cur_blk->__size();
            }
        }
    }
    else {
        ALOGD("Not initialized");
    }

    ALOGD(
        "**************************MEMORY INFO**************************");

    ALOGD("total memory               %12lu bytes", __total_memory);
    ALOGD("total memory with overhead %12lu bytes", __total_with_overhead);
    ALOGD("total allocated memory     %12lu bytes", __total_allocated_memory);
    ALOGD("total free memory          %12lu bytes", __total_free_memory);
    ALOGD("total total_blocks count   %12lu", __total_blocks);

    ALOGD(
        "**********************END OF MEMORY DUMP**********************");
}
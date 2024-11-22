#include "allocator.h"
#include <cerrno>
#include <cstring>

namespace utils
{
namespace alloc_malloc
{

static void __try_merge_free_blocks()
{
    ALOGD("trying to merge blocks ...");

    if (detail::_S_heap_start != nullptr && detail::_S_heap_end != nullptr)
    {
        for (_MemoryBlock_t __cur_blk = _MemoryBlock_t::__from_user_space_memory(detail::_S_heap_start);
                __cur_blk.__header() < detail::_S_heap_end;
                __cur_blk = __cur_blk.__next_implicit_block())
        {
            __cur_blk.__try_coalesc_with_next_implicit_block();
        }
    }
}

int mem_init(uint8_t *__start, size_t __size_in_bytes) noexcept
{
    if (__start != nullptr && __size_in_bytes > 0)
    {
        // first and last blocks must be always allocated
        size_t __reserved_block_size = _MemoryBlock_t::_S_header_size << 1;
        size_t __total_reserved_space = __reserved_block_size << 1;
        detail::_S_heap_start = __start;
        detail::_S_heap_end = detail::_S_heap_start + __size_in_bytes - __reserved_block_size;

        // Set to heap end pointer to usable space of the last block
        // Mark it as allocated block with size __SIZEOF_POINTER__
        _MemoryBlock_t __heap_start(detail::_S_heap_start);
        __heap_start.__put_to_header(_MemoryBlock_t::_S_header_size, _MemoryBlock_t::_BlockState::_S_allocated);

        _MemoryBlock_t heap = __heap_start.__next_implicit_block();
        // Take in account overhead for heap memory block it self so formula is
        // total heap size  - reserved space - heap memory block overhead size
        heap.__put_to_header(__size_in_bytes - __total_reserved_space - _MemoryBlock_t::_S_header_size,
                             _MemoryBlock_t::_BlockState::_S_free);

        // Create the last allocated service block
        _MemoryBlock_t __heap_end = heap.__next_implicit_block();
        __heap_end.__put_to_header(_MemoryBlock_t::_S_header_size, _MemoryBlock_t::_BlockState::_S_allocated);
        return 0;
    }

    return EINVAL;
}

/**
 * @brief __mem_find_fist_fit
 * @param __size
 * @return
 */
_MemoryBlock_t __mem_find_fist_fit(size_t __size)
{
    if (detail::_S_heap_start != nullptr && detail::_S_heap_end != nullptr)
    {
        for (_MemoryBlock_t __cur_blk = detail::_S_heap_start; __cur_blk.__header() <= detail::_S_heap_end;
                __cur_blk = __cur_blk.__next_implicit_block())
        {
            if (__cur_blk.__is_free() && __cur_blk.__size() >= __size)
            {
                return __cur_blk;
            }
        }
    }

    return _MemoryBlock_t::__null_block();
}

/**
 * @brief mem_malloc - allocates unused space for an object whose size in bytes is
 *                 specified by size and whose value is unspecified.
 * @param size   - desired size of block in bytes
 * @return         Upon successful completion with size not equal to 0, mem_malloc()
 *                 shall return a pointer to the allocated space. If size is 0,
 *                 either:
 *
 * A null pointer shall be returned and errno set to 0
 * Otherwise, it shall return a null pointer and set errno to ENOMEM
 */
void* mem_malloc(size_t __size) noexcept
{
    if (__size > 0)
    {
        auto block = __mem_find_fist_fit(_MemoryBlock_t::__aligned_size(__size));

        if (block.__is_null())
        {
            // No block. Try to merge some free blocks if possible and search again
            __try_merge_free_blocks();
            block = __mem_find_fist_fit(__size);
        }

        if (!block.__is_null())
        {
            return block.__slice(__size).__user_space_memory();
        }

        ALOGE("Out of memory");
    }

    return nullptr;
}

/**
 * @brief mem_calloc Allocates memory for an array of num objects of size and initializes all bytes in the allocated storage to zero.
 * @param num number of objects
 * @param size 	size of each object
 * @return On success, returns the pointer to the beginning of newly allocated memory
 */
void* mem_calloc(size_t __num, size_t __size) noexcept
{
    size_t __sz = __size * __num;
    void *p = mem_malloc(__sz);

    if (p != nullptr)
    {
        __sz = _MemoryBlock_t::__aligned_size(__sz);
        memset(p, 0, __sz);
    }


    return p;
}

/**
 * @brief mem_realloc Reallocates the given area of memory. If ptr is not nullptr it must be previously
 *                    allocated by mem_malloc, mem_calloc or mem_realloc.
 *                    If __p is nullptr, the behavior is the same as calling mem_malloc(__new_sz).
 * @param __p pointer to the memory area to be reallocated
 * @param __new_sz new size of the array in bytes
 * @return On success, returns the pointer to the beginning of newly allocated memory
 */
void* mem_realloc(void *__p, size_t __new_sz) noexcept
{
    if (__p == nullptr)
    {
        ALOGD("Block '%p' is null. Allocating memory with size(%lu)...", __p, __new_sz);
        return mem_malloc(__new_sz);
    }

    _MemoryBlock_t __blk = _MemoryBlock_t::__from_user_space_memory(__p);

    if (__blk.__is_allocated())
    {
        if (__new_sz > 0)
        {
            if (__blk.__size() == __new_sz)
            {
                ALOGD("new block size exactly as needed!");
                return __p;
            }

            if (__new_sz > __blk.__size())
            {
                ALOGD("new size is above than current. Trying to coleasc");
                __blk.__try_coalesc_with_next_implicit_block();
            }

            if (__blk.__size() > __new_sz)
            {
                ALOGD("new block size more than needed, slicing ...");
                return __blk.__slice(__new_sz).__user_space_memory();
            }

            // Here new block needs to be allocated and previous block needs to be copied to
            void* __new_blk = mem_malloc(__new_sz);

            if (__new_blk != nullptr)
            {
                ALOGD("Trying to allocate new block and copy contents of old one there");
                memcpy(__new_blk, __blk.__user_space_memory(), __blk.__size());
                __blk.__unallocate();
                __blk.__try_coalesc_with_next_implicit_block();
                return __new_blk;
            }

            ALOGD("No memory for new block. mem_realloc failed!");
        }
    }
    else
    {
        // if block is not allocated do nothing, just return nullptr. actually it's UB
        ALOGE("Block '%p' is not allocated!", __p);
    }

    return nullptr;
}

/**
 * @brief mem_free free previously allocated memory
 * @param ptr pointer to user space address of block
 */
void mem_free(void* __p) noexcept
{
    // we should be able savely called with nullptr
    if (__p != nullptr)
    {
        auto block = _MemoryBlock_t::__from_user_space_memory(__p);

        if (!block.__is_null() && block.__is_allocated()) {
            block.__unallocate();
            block.__try_coalesc_with_next_implicit_block();
        }
    } else {
        ALOGE("Unknown block '%p'", __p);
    }
}

/**
 * @brief __dump __dump memory
 */

void __dump() noexcept
{
    size_t __total_memory = 0;
    size_t __total_free_memory = 0;
    size_t __total_allocated_memory = 0;
    size_t __total_with_overhead = 0;
    size_t __total_blocks = 0;

    ALOGD(
        "*************************MEMORY DUMP*************************");

    if (detail::_S_heap_start != nullptr && detail::_S_heap_end != nullptr)
    {
        for (_MemoryBlock_t __cur_blk = detail::_S_heap_start; __cur_blk.__header() <= detail::_S_heap_end;
                __cur_blk = __cur_blk.__next_implicit_block(), __total_blocks++)
        {
            __cur_blk.__dump();
            __total_memory += __cur_blk.__size();
            __total_with_overhead += __cur_blk.__size_with_overhead();

            if (__cur_blk.__is_allocated())
            {
                __total_allocated_memory += __cur_blk.__size();
            }
            else if (__cur_blk.__is_free())
            {
                __total_free_memory += __cur_blk.__size();
            }
        }
    }
    else
    {
        ALOGD("Not initialized");
    }

    ALOGD(
        "**********************END OF MEMORY DUMP**********************");

    ALOGD("total memory               %12lu bytes", __total_memory);
    ALOGD("total memory with overhead %12lu bytes", __total_with_overhead);
    ALOGD("total allocated memory     %12lu bytes", __total_allocated_memory);
    ALOGD("total free memory          %12lu bytes", __total_free_memory);
    ALOGD("total total_blocks count   %12lu", __total_blocks);
}

} // namespace alloc_malloc
} // namespace utils

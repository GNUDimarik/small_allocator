#ifndef ALLOCATOR_H
#define ALLOCATOR_H

#include <cstdint>
#include <stddef.h>

#define LOG_NDEBUG 1
#define LOG_TAG "_MemoryBlock"
#include "logging.h"

namespace utils
{
namespace alloc_malloc
{
namespace detail
{

static uint8_t* _S_heap_start = nullptr;
static uint8_t* _S_heap_end = nullptr;

template <std::size_t _PointerSize> struct _MemoryBlock
{

    /**
     * @brief __aligned_size
     * @param __size
     * @return aligned by _PointerSize size
     */
    static size_t __aligned_size(size_t __size) noexcept
    {
        if (__size < _S_header_size)
        {
            return _S_header_size;
        }

        __size  = ((__size + (_S_header_size - 1)) & ~(_S_header_size - 1));
        __size += _S_header_size;
        return __size;
    }

    /**
     * @brief __null_block
     * @return nullptr block
     */
    static _MemoryBlock __null_block() noexcept
    {
        return _MemoryBlock(nullptr);
    }

    /**
     * @brief _S_size_of_pointer actually word should be address size but in terms of
     * asm it's even qword.
     */
    static constexpr const int _S_size_of_pointer = _PointerSize;

    /**
     * @brief _S_header_size size of implcit header
     */
    static constexpr const int _S_header_size = _S_size_of_pointer;

    /**
     * @brief The _BlockState enum means block state allocated or free
     */
    enum class _BlockState : int
    {
        _S_free = 0,       //!< Block is not allocated i.e. free
        _S_allocated = 1,  //!< Block is allocated
    };

    /**
     * @brief _M_block pointer to memory block which starts from implicit header
     */
    uint8_t* _M_block = nullptr;

    /**
     * @brief _MemoryBlock creates _MemoryBlock from __addr. It's header address not user space
     * @param __addr start of block
     */
    _MemoryBlock(uint8_t* __addr) : _M_block(__addr) {}

    /**
     * @brief __from_user_space_memory
     * @param __address - block address which starts from first byte of usable
     * space. i.e. after implicit header
     * @return
     */
    inline static _MemoryBlock __from_user_space_memory(void* __address)
    {
        return _MemoryBlock(reinterpret_cast<uint8_t*> (__address) - _S_header_size);
    }

    /**
     * @brief header
     * @return pointer to implcit header of the block
     */
    inline uint8_t* __header() const noexcept
    {
        return _M_block;
    }

    /**
     * @brief __user_space_memory
     * @return usable space of block. i.e. space which may be used by user
     * clients
     */
    inline uint8_t* __user_space_memory() const noexcept
    {
        return __header() + _S_header_size;
    }

    /**
     * @brief size
     * @return size of block
     */
    inline size_t __size() const noexcept
    {
        return (*reinterpret_cast<size_t*>(__header()) & ~0x01);
    }

    /**
     * @brief __size_with_overhead
     * @return total size of block including implicit header and footer
     */
    inline size_t __size_with_overhead() const noexcept
    {
        return __size() + _S_header_size;
    }

    /**
     * @brief __is_allocated
     * @return true if block is allocated
     */
    inline bool __is_allocated() const noexcept
    {
        return (*reinterpret_cast<size_t*>(__header()) & 0x01) ==
               static_cast<int>(_BlockState::_S_allocated);
    }

    /**
     * @brief __is_free
     * @return true if block is free
     */
    bool __is_free() const noexcept
    {
        return !__is_allocated();
    }

    /**
     * @brief __put_to_header set data to implicit header packed to one word
     * @param size size of block
     * @param state block state
     */
    void __put_to_header(size_t __size, _BlockState __state = _BlockState::_S_free) noexcept
    {
        *reinterpret_cast<size_t*>(__header()) = __size | static_cast<int>(__state);
    }

    /**
     * @brief __next_implicit_block
     * @return next implicit block
     */
    _MemoryBlock __next_implicit_block() const noexcept
    {
        return _MemoryBlock(__header() + __size() + _S_header_size);
    }

    /**
     * @brief previousImplicitBlock
     * @return previous implicit block
     */
    _MemoryBlock previousImplicitBlock() const noexcept
    {
        return _MemoryBlock(__header() - __size_with_overhead());
    }

    /**
     * @brief __slice tries to split this block to two if current block has enough space
     * @param __sz requested size of second block
     * @return next block after the split with size __sz and current block has size before this operation - __sz
     * (values may be corrected depend from block overheads)
     * slicing happens from end of the block.
     * if this block had no enough space for that null block is returned @see __is_null
     */
    _MemoryBlock __slice(size_t __sz) noexcept
    {
        if (__sz > 0)
        {
            __sz = __aligned_size(__sz);

            if (__sz < __size())
            {
                size_t __new_sz = __size() - (__sz + _S_header_size);
#if SLICE_FROM_BEINNING
                __put_to_header(__size, _BlockState::_S_allocated);
                __next_implicit_block().__put_to_header(__new_sz, _BlockState::_S_free);
                return *this;
#else
                __put_to_header(__new_sz, _BlockState::_S_free);
                __next_implicit_block().__put_to_header(__sz, _BlockState::_S_allocated);
                return __next_implicit_block();
#endif
            }
            else if (__sz == __size())
            {
                __put_to_header(__sz, _BlockState::_S_allocated);
                return *this;
            }
            else
            {
                ALOGD(
                    "%s(): current block size (%lu) less than requested (%lu). Ignore "
                    "request\n",
                    __func__, __size(), __sz);
            }
        }

        return __null_block();
    }

    /**
     * @brief __try_coalesc_with_next_implicit_block tries to merge as much as possible contiguous blocks
     * @return true if at least once merge operation been succeeded (this block and it's next)
     */
    bool __try_coalesc_with_next_implicit_block() noexcept
    {
        if (__is_free())
        {
            auto __next = __next_implicit_block();

            if (!__next.__is_null() && __next.__is_free())
            {
                __put_to_header(__size() + __next.__size() + _S_header_size);
                __next = __next_implicit_block();

                while (__next.__try_coalesc_with_next_implicit_block())
                {
                    __put_to_header(__size() + __next.__size() + _S_header_size);
                    __next = __next_implicit_block();
                }

                return true;
            }
        }

        return false;
    }

    /**
     * @brief __unallocate set free state to implicit header of the block
     */
    inline void __unallocate() noexcept
    {
        __put_to_header(__size(), _BlockState::_S_free);
    }

    /**
     * @brief __is_null
     * @return true if block points to nullptr
     */
    bool __is_null() const noexcept
    {
        return _M_block == nullptr;
    }

    /**
     * @brief operator < should help us to compare blocks during search of first or best fit
     * @param rhs - another block to compare
     * @return true if this < rhs by size
     */
    bool operator < (const _MemoryBlock<_PointerSize> &__rhs) const noexcept
    {
        return __size() < __rhs.__size();
    }

    void __dump() const noexcept
    {
        const char* format = _M_block == _S_heap_start || _M_block == _S_heap_end
                             ? "service block address %p size %12lu \t size "
                             "with overhead %8lu state %s"
                             : "block address         %p size %12lu \t size "
                             "with overhead %8lu state %s";

        ALOGD(format, __user_space_memory(), __size(), __size_with_overhead(),
              __is_allocated() ? "allocated" : "free");
    }
};

}  // namespace details

using _MemoryBlock_t = detail::_MemoryBlock<__SIZEOF_POINTER__>;

int mem_init(uint8_t *__start, size_t __size_in_bytes) noexcept;
void* mem_malloc(size_t __size) noexcept;
void* mem_calloc(size_t __num, size_t __size) noexcept;
void* mem_realloc(void *__p, size_t __new_sz) noexcept;
void mem_free(void* __p) noexcept;
void __dump() noexcept;
}  // namespace alloc_malloc
} // namespace utils

#endif  // ALLOCATOR_H

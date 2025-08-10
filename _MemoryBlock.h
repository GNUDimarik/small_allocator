#pragma once

#include "defs.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "mem::details::_MemoryBlock"
#define LOG_NDEBUG 1
#include "logging.h"

namespace mem::details
{
template<size_t _PointerSize> struct _MemoryBlock
{
    friend struct Utils;

    /**
     * @brief __aligned_size
     * @param __size
     * @return aligned by _PointerSize size
     */
    static size_t __aligned_size(size_t __size) noexcept
    {
        if (__size < _S_total_overhead_size) {
            return _S_total_overhead_size;
        }

        __size = ((__size + (_S_total_overhead_size - 1))
            & ~(_S_total_overhead_size - 1));
        return __size;
    }

    /**
     * @brief _S_header_size size of implcit header
     */
    static constexpr const int _S_header_size = _PointerSize;

    /**
     * @brief _S_total_overhead_size size of block ovehead with implicit header etc
     */
    static constexpr const int _S_total_overhead_size =
        _S_header_size + sizeof(size_t);

    /**
     * @brief The _BlockState enum means block state allocated or free
     */
    enum class _BlockState: int
    {
        _S_free = 0,       //!< Block is not allocated i.e. free
        _S_allocated = 1,  //!< Block is allocated
    };

    /**
     * @brief _M_prev_blk_size size of previous block
     */
    size_t _M_prev_blk_size = 0;

    /**
     * @brief _M_size
     */
    size_t _M_size = 0;

    /**
     * @brief _M_next - next item for free block
     */
    _MemoryBlock *_M_next = nullptr;

    /**
     * @brief _M_prev - previous item for free block
     */
    _MemoryBlock *_M_prev = nullptr;

    /**
     * @brief _MemoryBlock this constructor needs for free blocks list test
     * @param size
     */
    _MemoryBlock(size_t size) noexcept
        : _M_size(size)
    {}

    /**
     * @brief _MemoryBlock creates _MemoryBlock from __addr. It's header address not user space
     * @param __addr start of block
     */
    _MemoryBlock(uint8_t *__addr) noexcept
    {
        _M_size = reinterpret_cast<size_t>(__addr);

        if (_M_size > 0) {
            _M_prev_blk_size = *reinterpret_cast<size_t *>
            (__header() - _S_header_size);
        }
    }

    /**
     * @brief __construct _MemoryBlock object on the __address
     * @param __address where object to be contstructed to
     * @return
     */
    inline static _MemoryBlock *__construct(void *__address) noexcept
    {
        return reinterpret_cast<_MemoryBlock *> (__address);
    }

    /**
     * @brief __from_user_memory
     * @param __address - block address which starts from first byte of usable
     * space. i.e. after implicit header
     * @return
     */
    inline static _MemoryBlock *__from_user_memory(void *__address)
    {
        if (__address != nullptr) {
            return __construct(reinterpret_cast<uint8_t *>
                               (__address) - _S_total_overhead_size);
        }

        return nullptr;
    }

    /**
     * @brief header
     * @return pointer to implcit header of the block
     */
    inline uint8_t *__header() const noexcept
    {
        size_t *__p = const_cast<size_t *>(&_M_size);
        return reinterpret_cast<uint8_t *>(__p);
    }

    inline uint8_t *__begin() const noexcept
    {
        size_t *__p = const_cast<size_t *>(&_M_prev_blk_size);
        return reinterpret_cast<uint8_t *>(__p);
    }

    /**
     * @brief __user_memory
     * @return usable space of block. i.e. space which may be used by user
     * clients
     */
    inline uint8_t *__user_memory() const noexcept
    {
        return __header() + _S_header_size;
    }

    /**
     * @brief size
     * @return size of block
     */
    inline size_t __size() const noexcept
    {
        return _M_size & ~0x01;
    }

    /**
     * @brief __size_with_overhead
     * @return total size of block including implicit header and footer
     */
    inline size_t __size_with_overhead() const noexcept
    {
        return __size() + _S_total_overhead_size;
    }

    /**
     * @brief __is_allocated
     * @return true if block is allocated
     */
    inline bool __is_allocated() const noexcept
    {
        return (_M_size & 0x01) == static_cast<int>(_BlockState::_S_allocated);
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
    void __put_to_header(size_t __size, _BlockState __state =
    _BlockState::_S_free) noexcept
    {
        _M_size = __size | static_cast<int>(__state);
    }

    /**
     * @brief __next_implicit_block
     * @return next implicit block
     */
    _MemoryBlock *__next_implicit() const noexcept
    {
        return __construct(__begin() + __size_with_overhead());
    }

    /**
     * @brief previousImplicitBlock
     * @return previous implicit block
     */
    _MemoryBlock *__prev_implicit() const noexcept
    {
        return __construct(__begin() - (_M_prev_blk_size + _S_total_overhead_size));
    }

    /**
     * @brief __slice tries to split this block to two if current block has enough space
     * @param __sz requested size of second block
     * @return next block after the split with size __sz and current block has size before this operation - __sz
     * (values may be corrected depend from block overheads)
     * slicing happens from end of the block.
     * if this block had no enough space for that nullptr
     */
    _MemoryBlock *__slice(size_t __sz) noexcept
    {
        // Should not be called without successfull __find_best_fit
        // will crash otherwise
        if (__sz > 0) {
            __sz = __aligned_size(__sz);

            if (__sz < __size()) {
                size_t __new_sz = __size() - __sz;
                __put_to_header(__sz, _BlockState::_S_allocated);
                auto __n = __next_implicit();
                __n->__put_to_header(__new_sz - _S_total_overhead_size,
                                     _BlockState::_S_free);
                __n->_M_prev_blk_size = __size();
                return this;
            }
            else if (__sz == __size()) {
                __put_to_header(__sz, _BlockState::_S_allocated);
                return this;
            }
            else {
                ALOGD(
                    "%s(): current block size (%lu) less than requested (%lu). Ignore "
                    "request",
                    __func__, __size(), __sz);
            }
        }

        return nullptr;
    }

    /**
     * @brief __try_consolidate tries to merge contiguous blocks
     * @return
     */
    _MemoryBlock *__try_consolidate() noexcept
    {
        auto __res = this;
        auto __blk = __next_implicit();
        _MemoryBlock *__b = nullptr;

        if (__blk && __blk->__is_free()) {
            __put_to_header(__size() + __blk->__size_with_overhead());
            __b = __next_implicit();

            if (__b) {
                __b->_M_prev_blk_size = __size();
            }
        }

        __blk = __res->__prev_implicit();

        if (__blk && __blk->__is_free()) {
            __blk->__put_to_header(__size() + __blk->__size_with_overhead());
            __b = __blk->__next_implicit();

            if (__b) {
                __b->_M_prev_blk_size = __blk->__size();
            }
        }

        return __res;
    }

    /**
     * @brief __unallocate set free state to implicit header of the block
     */
    inline void __unallocate() noexcept
    {
        __put_to_header(__size(), _BlockState::_S_free);
    }

    /**
     * @brief __allocate sets allocatd state to header
     */
    inline void __allocate() noexcept
    {
        __put_to_header(__size(), _BlockState::_S_allocated);
    }

    /**
     * @brief operator < should help us to compare blocks during search of first or best fit
     * @param rhs - another block to compare
     * @return true if this < rhs by size
     */
    bool operator<(const _MemoryBlock<_PointerSize> &__rhs) const noexcept
    {
        return __size() < __rhs.__size();
    }
};

using _MemBlock = _MemoryBlock<__SIZEOF_POINTER__>;

} // mem::details
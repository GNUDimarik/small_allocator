#include "_FreeListManager.h"
#include "free_list.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "mem::details::_FreeListManager"
#define LOG_NDEBUG 1
#include "logging.h"

using namespace mem::details;
using namespace utils::alloc_malloc::detail;

void _FreeListManager::set_head(_MemBlock *__head) noexcept
{
    _M_free_list_head = __head;
}

void _FreeListManager::add_block(_MemBlock *__b) noexcept
{
    _M_add_block(__b);
}

void _FreeListManager::remove_block(_MemBlock *__b) noexcept
{
    for (size_t __i = 0; __i < kSmallBinCount; ++__i) {
        if (_M_small_bins[__i] == __b) {
            ALOGD("Found block %p with size %lu as head of small bin[%lu]", __b->__user_memory(), __b->__size(), __i);
            _M_small_bins[__i] = nullptr;
            break;
        }
    }

    __list_erase(__b);
}

_MemBlock *_FreeListManager::find_first_fit(size_t __size) noexcept
{
    _MemBlock *__b = nullptr;
    size_t __i = _M_bin_index(__size);
    ALOGD("find_first_fit: got size %lu and bin index %lu", __size, __i);

    if (_M_is_small_bin_index(__i)) {
        if (_M_small_bins[__i]) {
            ALOGD("Found block with size %lu in small bin[%lu]", __size, __i);
            __b = _M_small_bins[__i];
            _M_small_bins[__i] = __list_erase(__b);
        }
    }
    else if (_M_free_list_head) {
        size_t __head_size = _M_free_list_head->__size();
        ALOGD("Checking global bin for block with size %lu. head size %lu", __size, __head_size);

        if (__head_size == __size) {
            ALOGD("__head_size %lu and __size %lu are equal", __head_size, __size);
            __b = _M_free_list_head;
            _M_free_list_head = __list_erase(_M_free_list_head);
        }
        else if (__head_size < __size) {
            ALOGD("__head_size %lu > __size %lu, trying to find good block", __head_size, __size);
            auto __p = _M_free_list_head;

            while (__p) {
                if (__p->__size() >= __size) {
                    ALOGD("found block with size %lu. required size is %lu. slicing ...", __p->__size(), __size);
                    __list_erase(__p);
                    __b = __p->__slice(__size);
                    _M_free_list_head = __list_insert_sorted(_M_free_list_head, __p);
                    break;
                }

                __p = __p->_M_next;
            }
        }
        else if (__head_size < __size) {
            ALOGD("Needs consolidate");
        }
    }

    return __b;
}

size_t _FreeListManager::_M_bin_index(size_t __size) const noexcept
{
    return ((__size - kSmallBinMin) >> 3);
}

bool _FreeListManager::_M_is_small_bin_index(size_t __i) const noexcept
{
    return (__i >= kSmallBinMin && __i <= kSmallBinMax);
}

void _FreeListManager::_M_add_block(_MemBlock *__b) noexcept
{
    size_t __i = _M_bin_index(__b->__size());
    ALOGD("Add block: got index %lu", __i);

    if (_M_is_small_bin_index(__i)) {
        ALOGD("it's small bin index");

        if (_M_small_bins[__i]) {
            _M_small_bins[__i] = __list_insert_sorted(_M_small_bins[__i], __b);
            ALOGD("Inserted block with size %lu to small bin[%lu]", __b->__size(), __i);
        }
        else {
            _M_small_bins[__i] = __b;
        }
    }
    else {
        ALOGD("Adding block with size %lu to global bin", __b->__size());
        _M_free_list_head = __list_insert_sorted(_M_free_list_head, __b);
    }
}
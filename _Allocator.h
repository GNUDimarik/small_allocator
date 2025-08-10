#pragma once

#include "defs.h"
#include "_MemoryBlock.h"
#include "_FreeListManager.h"

#include <string.h>

#ifdef LOG_TAG
#undef LOG_TAG
#endif

#define LOG_TAG "mem::utils::Allocator"
#define LOG_NDEBUG 1
#include "logging.h"

namespace mem::utils
{
class _Allocator
{
public:
    explicit _Allocator(void *__start, size_t __size_in_bytes) noexcept;
    void *malloc(size_t __size) noexcept;
    void *calloc(size_t __num, size_t __size) noexcept;
    void *realloc(void *__p, size_t __new_sz) noexcept;
    void free(void *__p) noexcept;
    void dump() const noexcept;

    // no copy
    _Allocator(const _Allocator &) noexcept = delete;
    _Allocator &operator=(const _Allocator &) = delete;
private:
    [[nodiscard]] mem::details::_MemBlock *_M_find_first_fit(size_t __size) const noexcept;
    mem::details::_MemBlock *_M_heap_start = nullptr;
    mem::details::_MemBlock *_M_heap_end = nullptr;
    details::_FreeListManager _M_free_list_manager;
};

} // namespace mem::utils
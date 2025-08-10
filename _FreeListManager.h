#pragma once

#include "_MemoryBlock.h"

namespace mem::details
{

class _FreeListManager
{
public:
    static constexpr const size_t kSmallBinMin = 8;
    static constexpr const size_t kSmallBinMax = 256;
    static constexpr const size_t kSmallBinCount = ((kSmallBinMax - kSmallBinMin) >> 3) + 1;

    explicit _FreeListManager() noexcept = default;
    void set_head(_MemBlock *__head) noexcept;
    void add_block(_MemBlock *__b) noexcept;
    void remove_block(_MemBlock *__b) noexcept;
    [[nodiscard]] _MemBlock *find_first_fit(size_t __size) noexcept;
private:
    [[nodiscard]] size_t _M_bin_index(size_t __size) const noexcept;
    [[nodiscard]] bool _M_is_small_bin_index(size_t __i) const noexcept;
    void _M_add_block(_MemBlock *__b) noexcept;

    _MemBlock *_M_free_list_head = nullptr;
    _MemBlock *_M_small_bins[kSmallBinCount] = {nullptr};
};

} // namespace mem:details
#pragma once

#include "_MemoryBlock.h"

namespace mem::utils
{

struct Utils
{
    static void dumpMemoryBlock(const mem::details::_MemBlock *const block) noexcept;
};

} // namespace mem::utils
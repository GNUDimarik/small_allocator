#include "Utils.h"

using namespace mem::utils;

void Utils::dumpMemoryBlock(const mem::details::_MemBlock *const block) noexcept
{
    const char *format = "block address         %p size %12lu \t size "
                         "with overhead %8lu state %s\tprev blk size %lu";

    ALOGD(format, block->__user_memory(), block->__size(), block->__size_with_overhead(),
          block->__is_allocated() ? "allocated" : "free", block->_M_prev_blk_size);
}
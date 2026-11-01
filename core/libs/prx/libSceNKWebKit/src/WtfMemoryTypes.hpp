#pragma once

#include <cstddef>

struct FastMallocStatistics {
    std::size_t reservedVMBytes;
    std::size_t committedVMBytes;
    std::size_t freeListBytes;
};

namespace WTF {
struct TryMallocReturnValue {
    void* data;
};
}  // namespace WTF

static_assert(sizeof(WTF::TryMallocReturnValue) == sizeof(void*));

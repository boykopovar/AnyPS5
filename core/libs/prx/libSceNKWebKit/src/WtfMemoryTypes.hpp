#pragma once

#include <cstddef>

struct FastMallocStatistics {
    std::size_t reservedVMBytes;
    std::size_t committedVMBytes;
    std::size_t freeListBytes;
};

namespace WTF {
struct MappedFileData {
    void* data;
    unsigned size;
};

static_assert(sizeof(MappedFileData) == 16);
static_assert(offsetof(MappedFileData, data) == 0);
static_assert(offsetof(MappedFileData, size) == 8);

struct TryMallocReturnValue {
    void* data;
};
}  // namespace WTF

static_assert(sizeof(WTF::TryMallocReturnValue) == sizeof(void*));

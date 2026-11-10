#pragma once

#include <cstddef>
#include <cstdint>

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

namespace bmalloc {

struct FreeCell {
    std::uintptr_t scrambledNext;
};

struct FreeList {
    std::uintptr_t m_scrambledHead;
    std::uintptr_t m_secret;
    char* m_payloadEnd;
    unsigned m_remaining;
    unsigned m_originalSize;
};

}  // namespace bmalloc

static_assert(sizeof(WTF::TryMallocReturnValue) == sizeof(void*));
static_assert(sizeof(bmalloc::FreeList) == 32);
static_assert(offsetof(bmalloc::FreeList, m_scrambledHead) == 0);
static_assert(offsetof(bmalloc::FreeList, m_secret) == 8);
static_assert(offsetof(bmalloc::FreeList, m_payloadEnd) == 16);
static_assert(offsetof(bmalloc::FreeList, m_remaining) == 24);
static_assert(offsetof(bmalloc::FreeList, m_originalSize) == 28);

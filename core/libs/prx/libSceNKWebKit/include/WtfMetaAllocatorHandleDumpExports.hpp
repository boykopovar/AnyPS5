#pragma once

#include "prx/libSceJsc/include/WtfDataLogExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WtfDumpAbi {

struct MetaAllocatorHandle {
    std::uint32_t refCount;
    void* left;
    void* right;
    std::uintptr_t parentAndRed;
    void* allocator;
    void* start;
    void* end;
};

static_assert(sizeof(MetaAllocatorHandle) == 56);
static_assert(offsetof(MetaAllocatorHandle, left) == 8);
static_assert(offsetof(MetaAllocatorHandle, allocator) == 32);
static_assert(offsetof(MetaAllocatorHandle, start) == 40);
static_assert(offsetof(MetaAllocatorHandle, end) == 48);

}  // namespace NKWebKit::WtfDumpAbi

extern "C" {

void APS5_VABI _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(const NKWebKit::WtfDumpAbi::MetaAllocatorHandle* self, Jsc::WtfAbi::PrintStream* out);

}

#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WtfStreamPrintAbi {

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
static_assert(offsetof(MetaAllocatorHandle, refCount) == 0);
static_assert(offsetof(MetaAllocatorHandle, left) == 8);
static_assert(offsetof(MetaAllocatorHandle, right) == 16);
static_assert(offsetof(MetaAllocatorHandle, parentAndRed) == 24);
static_assert(offsetof(MetaAllocatorHandle, allocator) == 32);
static_assert(offsetof(MetaAllocatorHandle, start) == 40);
static_assert(offsetof(MetaAllocatorHandle, end) == 48);

}

extern "C" {

void APS5_VABI _ZN3WTF11PrintStream20printfVariableFormatEPKcz(NKWebKit::WtfDumpAbi::PrintStream* self, const char* format, ...);
void APS5_VABI _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(const NKWebKit::WtfStreamPrintAbi::MetaAllocatorHandle* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}

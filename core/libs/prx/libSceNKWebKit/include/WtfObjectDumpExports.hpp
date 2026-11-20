#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfBitVector.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"

#include <cstddef>

namespace NKWebKit::WtfDumpAbi {

struct MetaAllocatorHandle {
    unsigned refCount;
    void* left;
    void* right;
    void* parentAndRed;
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

void APS5_VABI _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(const NKWebKit::WtfDumpAbi::MetaAllocatorHandle* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(const void* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(const WTF::BitVector* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}

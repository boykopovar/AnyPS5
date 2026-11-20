#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfBitVector.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"

#include <cstddef>
#include <cstdint>

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

struct FreeList {
    char* intervalStart;
    char* intervalEnd;
    void* nextInterval;
    std::uint64_t secret;
    unsigned originalSize;
    unsigned cellSize;
};

static_assert(sizeof(FreeList) == 40);
static_assert(offsetof(FreeList, nextInterval) == 16);
static_assert(offsetof(FreeList, secret) == 24);
static_assert(offsetof(FreeList, originalSize) == 32);
static_assert(offsetof(FreeList, cellSize) == 36);

}  // namespace NKWebKit::WtfDumpAbi

extern "C" {

void APS5_VABI _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(const NKWebKit::WtfDumpAbi::FreeList* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE(const NKWebKit::WtfDumpAbi::MetaAllocatorHandle* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF6Thread4dumpERNS_11PrintStreamE(const void* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3WTF9BitVector4dumpERNS_11PrintStreamE(const WTF::BitVector* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}

#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfDumpStreamAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscObjectDumpAbi {

struct ClassInfo {
    const char* className;
};

struct FreeList {
    char* intervalStart;
    char* intervalEnd;
    void* nextInterval;
    std::uint64_t secret;
    std::uint32_t originalSize;
    std::uint32_t cellSize;
};

static_assert(offsetof(ClassInfo, className) == 0);
static_assert(sizeof(FreeList) == 40);
static_assert(offsetof(FreeList, intervalStart) == 0);
static_assert(offsetof(FreeList, intervalEnd) == 8);
static_assert(offsetof(FreeList, nextInterval) == 16);
static_assert(offsetof(FreeList, secret) == 24);
static_assert(offsetof(FreeList, originalSize) == 32);
static_assert(offsetof(FreeList, cellSize) == 36);

}

extern "C" {

void APS5_VABI _ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE(const NKWebKit::JscObjectDumpAbi::ClassInfo* self, NKWebKit::WtfDumpAbi::PrintStream* out);
void APS5_VABI _ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE(const NKWebKit::JscObjectDumpAbi::FreeList* self, NKWebKit::WtfDumpAbi::PrintStream* out);

}

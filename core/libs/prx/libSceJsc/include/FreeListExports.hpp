#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::HeapAbi {

struct FreeCell;

struct FreeList {
    std::uintptr_t scrambledHead;
    std::uintptr_t secret;
    char* payloadEnd;
    unsigned remaining;
    unsigned originalSize;
    unsigned cellSize;
};

static_assert(sizeof(FreeList) == 40);
static_assert(offsetof(FreeList, scrambledHead) == 0);
static_assert(offsetof(FreeList, secret) == 8);
static_assert(offsetof(FreeList, payloadEnd) == 16);
static_assert(offsetof(FreeList, remaining) == 24);
static_assert(offsetof(FreeList, originalSize) == 28);
static_assert(offsetof(FreeList, cellSize) == 32);

}  // namespace Jsc::HeapAbi

extern "C" {

void APS5_VABI _ZN3JSC8FreeList14initializeBumpEPcj(Jsc::HeapAbi::FreeList* self, char* payloadEnd, unsigned remaining);
void APS5_VABI _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(Jsc::HeapAbi::FreeList* self, Jsc::HeapAbi::FreeCell* head, std::uintptr_t secret, unsigned bytes);

}

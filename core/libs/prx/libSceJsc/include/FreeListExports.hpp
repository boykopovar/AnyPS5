#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::JscAbi {

struct FreeCell;

struct FreeList {
    std::uintptr_t scrambledHead;
    std::uintptr_t secret;
    char* payloadEnd;
    std::uint32_t remaining;
    std::uint32_t originalSize;
    std::uint32_t cellSize;
};

static_assert(sizeof(FreeList) == 40);
static_assert(alignof(FreeList) == 8);
static_assert(offsetof(FreeList, scrambledHead) == 0);
static_assert(offsetof(FreeList, secret) == 8);
static_assert(offsetof(FreeList, payloadEnd) == 16);
static_assert(offsetof(FreeList, remaining) == 24);
static_assert(offsetof(FreeList, originalSize) == 28);
static_assert(offsetof(FreeList, cellSize) == 32);

} // namespace Jsc::JscAbi

extern "C" {

void APS5_VABI _ZN3JSC8FreeList14initializeListEPNS_8FreeCellEmj(Jsc::JscAbi::FreeList* self, Jsc::JscAbi::FreeCell* head, std::uintptr_t secret, unsigned bytes);
void APS5_VABI _ZN3JSC8FreeList14initializeBumpEPcj(Jsc::JscAbi::FreeList* self, char* payloadEnd, unsigned remaining);

}

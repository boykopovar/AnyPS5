#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>

struct UCharIterator;

using IteratorGetIndex = std::int32_t (APS5_VABI*)(UCharIterator*, std::int32_t);
using IteratorMove = std::int32_t (APS5_VABI*)(UCharIterator*, std::int32_t, std::int32_t);
using IteratorHasNext = std::int8_t (APS5_VABI*)(UCharIterator*);
using IteratorCurrent = std::int32_t (APS5_VABI*)(UCharIterator*);
using IteratorReserved = std::int32_t (APS5_VABI*)(UCharIterator*, std::int32_t);
using IteratorGetState = std::uint32_t (APS5_VABI*)(const UCharIterator*);
using IteratorSetState = void (APS5_VABI*)(UCharIterator*, std::uint32_t, std::int32_t*);

struct UCharIterator {
    const void* context;
    std::int32_t length;
    std::int32_t start;
    std::int32_t index;
    std::int32_t limit;
    std::int32_t reservedField;
    IteratorGetIndex getIndex;
    IteratorMove move;
    IteratorHasNext hasNext;
    IteratorHasNext hasPrevious;
    IteratorCurrent current;
    IteratorCurrent next;
    IteratorCurrent previous;
    IteratorReserved reservedFn;
    IteratorGetState getState;
    IteratorSetState setState;
};

static_assert(sizeof(UCharIterator) == 112);

extern "C" {
void APS5_VABI uiter_setString_nid_postfix(UCharIterator*, const char16_t*, std::int32_t);
void APS5_VABI uiter_setUTF8_nid_postfix(UCharIterator*, const char*, std::int32_t);
}

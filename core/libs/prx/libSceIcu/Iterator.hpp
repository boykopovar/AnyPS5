#pragma once
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>

enum UCharIteratorOrigin : std::int32_t {
    UITER_START,
    UITER_CURRENT,
    UITER_LIMIT,
    UITER_ZERO,
    UITER_LENGTH
};

struct UCharIterator {
    using GetIndexFn = std::int32_t (APS5_VABI *)(UCharIterator* iterator, UCharIteratorOrigin origin);
    using MoveFn = std::int32_t (APS5_VABI *)(UCharIterator* iterator, std::int32_t delta, UCharIteratorOrigin origin);
    using HasNextFn = std::int8_t (APS5_VABI *)(UCharIterator* iterator);
    using HasPreviousFn = std::int8_t (APS5_VABI *)(UCharIterator* iterator);
    using CurrentFn = std::int32_t (APS5_VABI *)(UCharIterator* iterator);
    using NextFn = std::int32_t (APS5_VABI *)(UCharIterator* iterator);
    using PreviousFn = std::int32_t (APS5_VABI *)(UCharIterator* iterator);
    using ReservedFn = std::int32_t (APS5_VABI *)(UCharIterator* iterator, std::int32_t something);
    using GetStateFn = std::uint32_t (APS5_VABI *)(const UCharIterator* iterator);
    using SetStateFn = void (APS5_VABI *)(UCharIterator* iterator, std::uint32_t state, std::int32_t* errorCode);

    const void* context;
    std::int32_t length;
    std::int32_t start;
    std::int32_t index;
    std::int32_t limit;
    std::int32_t reservedField;
    GetIndexFn getIndex;
    MoveFn move;
    HasNextFn hasNext;
    HasPreviousFn hasPrevious;
    CurrentFn current;
    NextFn next;
    PreviousFn previous;
    ReservedFn reservedFn;
    GetStateFn getState;
    SetStateFn setState;
};

static_assert(sizeof(UCharIteratorOrigin) == 4);
static_assert(offsetof(UCharIterator, getIndex) == 32);
static_assert(offsetof(UCharIterator, setState) == 104);
static_assert(sizeof(UCharIterator) == 112);

extern "C" {
std::int32_t APS5_VABI u_strCompareIter_nid_postfix(UCharIterator* first, UCharIterator* second, std::uint8_t codePointOrder);
}

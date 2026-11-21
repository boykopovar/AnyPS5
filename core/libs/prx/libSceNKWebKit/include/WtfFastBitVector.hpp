#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WTF {

struct FastBitVectorWordOwner {
    std::uint32_t* words = nullptr;
    std::size_t numBits = 0;
};

struct FastBitVector {
    FastBitVectorWordOwner storage;
};

static_assert(std::is_standard_layout_v<FastBitVectorWordOwner>);
static_assert(sizeof(FastBitVectorWordOwner) == 16);
static_assert(alignof(FastBitVectorWordOwner) == 8);
static_assert(offsetof(FastBitVectorWordOwner, words) == 0);
static_assert(offsetof(FastBitVectorWordOwner, numBits) == 8);
static_assert(std::is_standard_layout_v<FastBitVector>);
static_assert(sizeof(FastBitVector) == 16);
static_assert(alignof(FastBitVector) == 8);
static_assert(offsetof(FastBitVector, storage) == 0);

}

extern "C" {

void APS5_VABI _ZN3WTF13FastBitVector10clearRangeEmm(WTF::FastBitVector* vector, std::size_t begin, std::size_t end);
void APS5_VABI _ZN3WTF22FastBitVectorWordOwner10resizeSlowEm(WTF::FastBitVectorWordOwner* owner, std::size_t numBits);
void APS5_VABI _ZN3WTF22FastBitVectorWordOwner13setEqualsSlowERKS0_(WTF::FastBitVectorWordOwner* owner,
    const WTF::FastBitVectorWordOwner* other);

}

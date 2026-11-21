#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace WTF {

struct BitVector {
    struct OutOfLineBits {
        std::size_t numBits;
    };

    std::uintptr_t bitsOrPointer;
};

}  // namespace WTF

static_assert(sizeof(WTF::BitVector) == 8);
static_assert(sizeof(WTF::BitVector::OutOfLineBits) == 8);

extern "C" {

WTF::BitVector::OutOfLineBits* APS5_VABI _ZN3WTF9BitVector13OutOfLineBits6createEm(std::size_t numBits);
void APS5_VABI _ZN3WTF9BitVector6resizeEm(WTF::BitVector* self, std::size_t numBits);
void APS5_VABI _ZN3WTF9BitVector7setSlowERKS0_(WTF::BitVector* self, const WTF::BitVector* other);
void APS5_VABI _ZN3WTF9BitVector8clearAllEv(WTF::BitVector* self);
void APS5_VABI _ZN3WTF9BitVector9mergeSlowERKS0_(WTF::BitVector* self, const WTF::BitVector* other);
void APS5_VABI _ZN3WTF9BitVector11excludeSlowERKS0_(WTF::BitVector* self, const WTF::BitVector* other);
bool APS5_VABI _ZNK3WTF9BitVector11isEmptySlowEv(const WTF::BitVector* self);
std::size_t APS5_VABI _ZNK3WTF9BitVector12bitCountSlowEv(const WTF::BitVector* self);
std::uintptr_t APS5_VABI _ZNK3WTF9BitVector12hashSlowCaseEv(const WTF::BitVector* self);
bool APS5_VABI _ZNK3WTF9BitVector14equalsSlowCaseERKS0_(const WTF::BitVector* self, const WTF::BitVector* other);

}

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace Jsc::WtfAbi {

struct BitVector {
    std::uintptr_t bitsOrPointer;
};

struct BitVectorOutOfLineBits {
    std::size_t numBits;
};

static_assert(sizeof(BitVector) == 8);
static_assert(sizeof(BitVectorOutOfLineBits) == 8);

}  // namespace Jsc::WtfAbi

extern "C" {

void APS5_VABI _ZN3WTF9BitVector10filterSlowERKS0_(Jsc::WtfAbi::BitVector* self, const Jsc::WtfAbi::BitVector* other);

}

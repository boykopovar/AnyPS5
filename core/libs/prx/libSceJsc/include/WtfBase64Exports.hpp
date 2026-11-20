#pragma once

#include "prx/libSceJsc/src/WtfStringAbi.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Jsc::WtfAbi {

struct String {
    StringImpl* impl;
};

struct CharVector {
    std::uint8_t* buffer;
    unsigned capacity;
    unsigned size;
};

struct SignedOrUnsignedCharVectorAdapter {
    CharVector* vector;
    bool isSigned;
};

static_assert(sizeof(String) == 8);
static_assert(sizeof(CharVector) == 16);
static_assert(offsetof(CharVector, buffer) == 0);
static_assert(offsetof(CharVector, capacity) == 8);
static_assert(offsetof(CharVector, size) == 12);
static_assert(sizeof(SignedOrUnsignedCharVectorAdapter) == 16);
static_assert(offsetof(SignedOrUnsignedCharVectorAdapter, vector) == 0);
static_assert(offsetof(SignedOrUnsignedCharVectorAdapter, isSigned) == 8);
static_assert(std::is_trivially_copyable_v<SignedOrUnsignedCharVectorAdapter>);

} // namespace Jsc::WtfAbi

extern "C" {

bool APS5_VABI _ZN3WTF15base64URLDecodeERKNS_6StringENS_33SignedOrUnsignedCharVectorAdapterE(const Jsc::WtfAbi::String* in, Jsc::WtfAbi::SignedOrUnsignedCharVectorAdapter out);

}

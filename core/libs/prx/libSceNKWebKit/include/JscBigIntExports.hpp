#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace JSC {

struct JSBigInt {
    std::uint8_t cellHeader[8];
    std::uint32_t length;
    std::uint32_t hash;
    std::uint8_t sign;
    std::uint64_t* data;
};

static_assert(sizeof(JSBigInt) == 32);
static_assert(offsetof(JSBigInt, length) == 8);
static_assert(offsetof(JSBigInt, sign) == 16);
static_assert(offsetof(JSBigInt, data) == 24);

}

extern "C" {
bool APS5_VABI _ZN3JSC8JSBigInt6equalsEPS0_S1_(JSC::JSBigInt* x, JSC::JSBigInt* y);
bool APS5_VABI _ZN3JSC8JSBigInt13equalsToInt32Ei(JSC::JSBigInt* self, std::int32_t value);
}

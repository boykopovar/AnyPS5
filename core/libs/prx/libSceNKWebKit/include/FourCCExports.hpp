#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct OptionalFourCC {
    bool engaged;
    std::uint32_t value;
};

static_assert(sizeof(OptionalFourCC) == 8);
static_assert(alignof(OptionalFourCC) == 4);
static_assert(offsetof(OptionalFourCC, engaged) == 0);
static_assert(offsetof(OptionalFourCC, value) == 4);

}

extern "C" {

NKWebKit::WebCoreAbi::OptionalFourCC* APS5_VABI _ZN7WebCore6FourCC10fromStringERKN3WTF6StringE(NKWebKit::WebCoreAbi::OptionalFourCC* result, const NKWebKit::WtfAbi::String* string);

}

#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <type_traits>

namespace Jsc::WtfAbi {

struct StringImpl {
    unsigned refCount;
    unsigned length;
    const void* data;
    mutable unsigned hashAndFlags;
};

struct StringView {
    const void* characters;
    unsigned length;
    bool is8Bit;
};

using CodeUnitMatchFunction = bool (APS5_VABI*)(char16_t);

inline constexpr unsigned StringImplFlag8Bit = 1u << 2;

static_assert(sizeof(StringImpl) == 24);
static_assert(alignof(StringImpl) == 8);
static_assert(offsetof(StringImpl, refCount) == 0);
static_assert(offsetof(StringImpl, length) == 4);
static_assert(offsetof(StringImpl, data) == 8);
static_assert(offsetof(StringImpl, hashAndFlags) == 16);
static_assert(sizeof(StringView) == 16);
static_assert(offsetof(StringView, characters) == 0);
static_assert(offsetof(StringView, length) == 8);
static_assert(offsetof(StringView, is8Bit) == 12);
static_assert(std::is_trivially_copyable_v<StringView>);
static_assert(std::is_trivially_destructible_v<StringView>);

} // namespace Jsc::WtfAbi

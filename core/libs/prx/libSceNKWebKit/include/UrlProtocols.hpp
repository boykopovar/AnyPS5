#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit {

struct StringView {
    const void* data;
    std::uint32_t length;
    bool is8Bit;
};

static_assert(sizeof(StringView) == 16);
static_assert(offsetof(StringView, data) == 0);
static_assert(offsetof(StringView, length) == 8);
static_assert(offsetof(StringView, is8Bit) == 12);
static_assert(std::is_trivially_copyable_v<StringView>);

}

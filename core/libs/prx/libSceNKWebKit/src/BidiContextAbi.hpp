#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct BidiContext {
    unsigned refCount;
    std::uint32_t bits;
    BidiContext* parent;
};

struct BidiContextRef {
    BidiContext* ptr;
};

inline constexpr std::uint32_t kBidiLevelMask = 0x3F;
inline constexpr unsigned kBidiDirectionShift = 6;
inline constexpr std::uint32_t kBidiDirectionMask = 0x1F;
inline constexpr unsigned kBidiOverrideShift = 11;
inline constexpr unsigned kBidiSourceShift = 12;

static_assert(sizeof(BidiContext) == 16);
static_assert(offsetof(BidiContext, bits) == 4);
static_assert(offsetof(BidiContext, parent) == 8);
static_assert(std::is_trivially_copyable_v<BidiContext>);

}  // namespace NKWebKit::WebCoreAbi

#pragma once

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscAbi {

struct JSPromise {
    std::uint64_t cellHeader;
    void* butterfly;
    std::uint64_t flags;
    std::uint64_t reactionsOrResult;
};

constexpr std::uint64_t JSValueNumberTag = 0xfffe000000000000ull;
constexpr std::uint64_t JSValueDoubleEncodeOffset = 1ull << 49;
constexpr std::uint64_t JSValueUndefined = 0xa;
constexpr std::uint32_t JSPromiseStateMask = 0b11;
constexpr std::uint32_t JSPromiseIsHandledFlag = 4;

static_assert(sizeof(JSPromise) == 0x20);
static_assert(offsetof(JSPromise, flags) == 0x10);
static_assert(offsetof(JSPromise, reactionsOrResult) == 0x18);

}

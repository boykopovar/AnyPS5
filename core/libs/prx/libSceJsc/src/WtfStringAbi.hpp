#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

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

struct String {
    StringImpl* impl;
    explicit String(StringImpl* value = nullptr) noexcept : impl(value) {}
    String(String&& other) noexcept : impl(std::exchange(other.impl, nullptr)) {}
    String(const String&) = delete;
    String& operator=(const String&) = delete;
    String& operator=(String&&) = delete;
    ~String();
    StringImpl* Release() noexcept { return std::exchange(impl, nullptr); }
};

struct StringVector {
    String* buffer;
    unsigned capacity;
    unsigned size;
    StringVector() noexcept : buffer(nullptr), capacity(0), size(0) {}
    StringVector(StringVector&& other) noexcept
        : buffer(std::exchange(other.buffer, nullptr)), capacity(std::exchange(other.capacity, 0)), size(std::exchange(other.size, 0)) {}
    StringVector(const StringVector&) = delete;
    StringVector& operator=(const StringVector&) = delete;
    StringVector& operator=(StringVector&&) = delete;
    ~StringVector();
};

struct ASCIILiteral {
    const char* characters;
};

inline constexpr unsigned StringImplFlag8Bit = 1u << 2;
inline constexpr unsigned StringImplFlagDidReportCost = 1u << 3;
inline constexpr unsigned StringImplFlagAtom = 1u << 4;
inline constexpr unsigned StringImplFlagSymbol = 1u << 5;
inline constexpr unsigned StringImplMaskBufferOwnership = 3u;

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
static_assert(sizeof(String) == 8);
static_assert(!std::is_trivially_destructible_v<String>);
static_assert(sizeof(StringVector) == 16);
static_assert(offsetof(StringVector, buffer) == 0);
static_assert(offsetof(StringVector, capacity) == 8);
static_assert(offsetof(StringVector, size) == 12);
static_assert(!std::is_trivially_destructible_v<StringVector>);
static_assert(sizeof(ASCIILiteral) == 8);
static_assert(std::is_trivially_copyable_v<ASCIILiteral>);
static_assert(std::is_trivially_copyable_v<StringView>);
static_assert(std::is_trivially_destructible_v<StringView>);

} // namespace Jsc::WtfAbi

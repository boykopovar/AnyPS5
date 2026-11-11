#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <new>
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

struct CStringBuffer {
    unsigned refCount;
    std::size_t length;
};

struct CString {
    CStringBuffer* buffer;
    explicit CString(CStringBuffer* value = nullptr) noexcept : buffer(value) {}
    CString(CString&& other) noexcept : buffer(std::exchange(other.buffer, nullptr)) {}
    CString(const CString&) = delete;
    CString& operator=(const CString&) = delete;
    CString& operator=(CString&&) = delete;
    ~CString();
};

enum ConversionMode : int {
    LenientConversion,
    StrictConversion,
    StrictConversionReplacingUnpairedSurrogatesWithFFFD,
};

enum class UTF8ConversionError : int {
    None,
    OutOfMemory,
    IllegalSource,
    SourceExhausted,
};

struct ExpectedCString {
    union {
        CString value;
        UTF8ConversionError error;
    };
    bool hasValue;
    ExpectedCString(CString&& string) noexcept : value(std::move(string)), hasValue(true) {}
    explicit ExpectedCString(UTF8ConversionError failure) noexcept : error(failure), hasValue(false) {}
    ExpectedCString(ExpectedCString&& other) noexcept : hasValue(other.hasValue) {
        if (hasValue) new (&value) CString(std::move(other.value));
        else error = other.error;
    }
    ExpectedCString(const ExpectedCString&) = delete;
    ExpectedCString& operator=(const ExpectedCString&) = delete;
    ExpectedCString& operator=(ExpectedCString&&) = delete;
    ~ExpectedCString() {
        if (hasValue) value.~CString();
    }
};

void* AllocateBlock(std::size_t size);
void FreeBlock(void* pointer);
StringImpl* RefString(StringImpl& impl);
void DerefString(StringImpl* impl);

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
static_assert(sizeof(CStringBuffer) == 16);
static_assert(alignof(CStringBuffer) == 8);
static_assert(offsetof(CStringBuffer, refCount) == 0);
static_assert(offsetof(CStringBuffer, length) == 8);
static_assert(sizeof(CString) == 8);
static_assert(offsetof(CString, buffer) == 0);
static_assert(!std::is_trivially_destructible_v<CString>);
static_assert(sizeof(ConversionMode) == 4);
static_assert(sizeof(UTF8ConversionError) == 4);
static_assert(sizeof(ExpectedCString) == 16);
static_assert(alignof(ExpectedCString) == 8);
static_assert(offsetof(ExpectedCString, value) == 0);
static_assert(offsetof(ExpectedCString, error) == 0);
static_assert(offsetof(ExpectedCString, hasValue) == 8);
static_assert(!std::is_trivially_destructible_v<ExpectedCString>);
static_assert(std::is_trivially_copyable_v<StringView>);
static_assert(std::is_trivially_destructible_v<StringView>);

} // namespace Jsc::WtfAbi

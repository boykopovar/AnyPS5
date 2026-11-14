#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>

namespace NKWebKit::WtfAbi {

struct StringImpl {
    unsigned refCount;
    unsigned length;
    const void* data;
    mutable unsigned hashAndFlags;
};

struct StringView {
    const void* data;
    unsigned length;
    bool is8Bit;
};

enum class HTMLIntegerParsingError : int { NegativeOverflow, PositiveOverflow, Other };

struct HTMLNonNegativeIntegerResult {
    union {
        unsigned value;
        HTMLIntegerParsingError error;
    };
    bool hasValue;
};

static_assert(sizeof(HTMLIntegerParsingError) == 4);
static_assert(sizeof(HTMLNonNegativeIntegerResult) == 8);
static_assert(alignof(HTMLNonNegativeIntegerResult) == 4);
static_assert(offsetof(HTMLNonNegativeIntegerResult, value) == 0);
static_assert(offsetof(HTMLNonNegativeIntegerResult, error) == 0);
static_assert(offsetof(HTMLNonNegativeIntegerResult, hasValue) == 4);
static_assert(std::is_trivially_copy_constructible_v<HTMLNonNegativeIntegerResult>);
static_assert(std::is_trivially_move_constructible_v<HTMLNonNegativeIntegerResult>);
static_assert(std::is_trivially_destructible_v<HTMLNonNegativeIntegerResult>);

struct URL {
    StringImpl* impl;
    unsigned bits;
    unsigned userStart;
    unsigned userEnd;
    unsigned passwordEnd;
    unsigned hostEnd;
    unsigned pathAfterLastSlash;
    unsigned pathEnd;
    unsigned queryEnd;
};

struct LogSiteIdentifier {
    const char* className;
    const char* methodName;
    std::uintptr_t objectPtr;
};

static_assert(sizeof(URL) == 40);
static_assert(offsetof(URL, impl) == 0);
static_assert(offsetof(URL, bits) == 8);
static_assert(offsetof(URL, userStart) == 12);
static_assert(offsetof(URL, userEnd) == 16);
static_assert(offsetof(URL, passwordEnd) == 20);
static_assert(offsetof(URL, hostEnd) == 24);
static_assert(offsetof(URL, pathAfterLastSlash) == 28);
static_assert(offsetof(URL, pathEnd) == 32);
static_assert(offsetof(URL, queryEnd) == 36);

struct CStringBuffer {
    unsigned refCount;
    std::size_t length;
};

void Release(StringImpl* impl);
void Release(CStringBuffer* buffer);

struct String {
    StringImpl* impl;
    explicit String(StringImpl* value = nullptr) : impl(value) {}
    String(const String& other) : impl(other.impl) { if (impl) impl->refCount += 2; }
    String(String&& other) noexcept : impl(std::exchange(other.impl, nullptr)) {}
    ~String() { Release(impl); }
    String& operator=(String other) noexcept { std::swap(impl, other.impl); return *this; }
};

StringImpl* EmptyStringImpl();
StringImpl* TryCreateUninitialized(unsigned length, bool is8Bit, void*& characters);
String SubstringSharingImpl(const String& string, unsigned start, unsigned length);
String FromUTF8ReplacingInvalidSequences_nid_no_patch(const unsigned char* input, std::size_t length);

struct StringVector {
    String* buffer;
    unsigned capacity;
    unsigned size;
    StringVector() : buffer(nullptr), capacity(0), size(0) {}
    StringVector(StringVector&& other) noexcept
        : buffer(std::exchange(other.buffer, nullptr)), capacity(std::exchange(other.capacity, 0)), size(std::exchange(other.size, 0)) {}
    StringVector(const StringVector&) = delete;
    StringVector& operator=(const StringVector&) = delete;
    ~StringVector();
};

struct CString {
    CStringBuffer* buffer;
    explicit CString(CStringBuffer* value = nullptr) : buffer(value) {}
    CString(const CString& other) : buffer(other.buffer) { if (buffer) ++buffer->refCount; }
    CString(CString&& other) noexcept : buffer(std::exchange(other.buffer, nullptr)) {}
    ~CString() { Release(buffer); }
};

const char* CStringBytes(const CString& value);

struct ProcessPrivileges {
    unsigned storage;
};

static_assert(alignof(StringImpl) == 8);
static_assert(alignof(StringView) == 8);
static_assert(alignof(CStringBuffer) == 8);
static_assert(!std::is_trivially_destructible_v<String>);
static_assert(!std::is_trivially_destructible_v<CString>);
static_assert(sizeof(StringImpl) == 24);
static_assert(offsetof(StringImpl, refCount) == 0);
static_assert(offsetof(StringImpl, length) == 4);
static_assert(offsetof(StringImpl, data) == 8);
static_assert(offsetof(StringImpl, hashAndFlags) == 16);
static_assert(sizeof(String) == 8);
static_assert(offsetof(String, impl) == 0);
static_assert(sizeof(StringVector) == 16);
static_assert(offsetof(StringVector, buffer) == 0);
static_assert(offsetof(StringVector, capacity) == 8);
static_assert(offsetof(StringVector, size) == 12);
static_assert(!std::is_trivially_destructible_v<StringVector>);
static_assert(sizeof(StringView) == 16);
static_assert(offsetof(StringView, data) == 0);
static_assert(offsetof(StringView, length) == 8);
static_assert(offsetof(StringView, is8Bit) == 12);
static_assert(sizeof(CString) == 8);
static_assert(offsetof(CString, buffer) == 0);
static_assert(sizeof(CStringBuffer) == 16);
static_assert(offsetof(CStringBuffer, refCount) == 0);
static_assert(offsetof(CStringBuffer, length) == 8);
static_assert(sizeof(ProcessPrivileges) == 4);
static_assert(offsetof(ProcessPrivileges, storage) == 0);
static_assert(sizeof(LogSiteIdentifier) == 24);
static_assert(offsetof(LogSiteIdentifier, className) == 0);
static_assert(offsetof(LogSiteIdentifier, methodName) == 8);
static_assert(offsetof(LogSiteIdentifier, objectPtr) == 16);

}  // namespace NKWebKit::WtfAbi

#pragma once

#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::JscBytecodeCacheErrorAbi {

struct JSToken {
    std::int32_t type;
    std::uint64_t data[3];
    std::int32_t location[4];
    std::int32_t startPosition[3];
    std::int32_t endPosition[3];
};

struct ParserError {
    JSToken token;
    WtfAbi::StringImpl* message;
    std::int32_t line;
    std::uint8_t type;
    std::uint8_t syntaxErrorType;
};

struct StandardError {
    std::int32_t error;
};

struct WriteError {
    std::uint64_t written;
    std::uint64_t expected;
};

struct BytecodeCacheError {
    union {
        ParserError parserError;
        StandardError standardError;
        WriteError writeError;
    };
    std::uint32_t index;
};

static_assert(sizeof(JSToken) == 72);
static_assert(offsetof(JSToken, data) == 8);
static_assert(offsetof(JSToken, location) == 32);
static_assert(offsetof(JSToken, startPosition) == 48);
static_assert(offsetof(JSToken, endPosition) == 60);
static_assert(sizeof(ParserError) == 88);
static_assert(offsetof(ParserError, message) == 72);
static_assert(offsetof(ParserError, line) == 80);
static_assert(offsetof(ParserError, type) == 84);
static_assert(offsetof(ParserError, syntaxErrorType) == 85);
static_assert(sizeof(StandardError) == 4);
static_assert(sizeof(WriteError) == 16);
static_assert(offsetof(WriteError, expected) == 8);
static_assert(sizeof(BytecodeCacheError) == 96);
static_assert(offsetof(BytecodeCacheError, parserError) == 0);
static_assert(offsetof(BytecodeCacheError, standardError) == 0);
static_assert(offsetof(BytecodeCacheError, writeError) == 0);
static_assert(offsetof(BytecodeCacheError, index) == 88);
static_assert(std::is_trivially_copyable_v<BytecodeCacheError>);

}

extern "C" {

bool APS5_VABI _ZNK3JSC18BytecodeCacheError7isValidEv(const NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError* self);
NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError* APS5_VABI _ZN3JSC18BytecodeCacheErroraSERKNS0_13StandardErrorE(NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError* self, const NKWebKit::JscBytecodeCacheErrorAbi::StandardError* error);
NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError* APS5_VABI _ZN3JSC18BytecodeCacheErroraSERKNS0_10WriteErrorE(NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError* self, const NKWebKit::JscBytecodeCacheErrorAbi::WriteError* error);

}

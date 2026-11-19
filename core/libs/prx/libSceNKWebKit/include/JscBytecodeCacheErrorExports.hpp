#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::JscBytecodeCacheErrorAbi {

inline constexpr std::uint8_t ParserErrorIndex = 0;
inline constexpr std::uint8_t StandardErrorIndex = 1;
inline constexpr std::uint8_t WriteErrorIndex = 2;
inline constexpr std::uint8_t ParserErrorNone = 0;

struct ParserError {
    std::uint8_t token[72];
    void* message;
    std::int32_t line;
    std::uint8_t type;
    std::uint8_t syntaxErrorType;
};

struct BytecodeCacheError {
    union {
        ParserError parser;
        std::int32_t standardErrno;
        struct {
            std::size_t written;
            std::size_t expected;
        } write;
    } storage;
    std::uint8_t index;
};

static_assert(sizeof(ParserError) == 88);
static_assert(offsetof(ParserError, message) == 72);
static_assert(offsetof(ParserError, line) == 80);
static_assert(offsetof(ParserError, type) == 84);
static_assert(offsetof(ParserError, syntaxErrorType) == 85);
static_assert(offsetof(BytecodeCacheError, index) == 88);
static_assert(sizeof(BytecodeCacheError) == 96);

}

extern "C" {

bool APS5_VABI _ZNK3JSC18BytecodeCacheError7isValidEv(const NKWebKit::JscBytecodeCacheErrorAbi::BytecodeCacheError* self);

}

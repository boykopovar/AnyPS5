#pragma once

#include "HttpHeaderNames.hpp"
#include "WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::HttpAbi {

using WtfAbi::String;

struct CommonHeader {
    HTTPHeaderName key;
    String value;
};

struct UncommonHeader {
    String key;
    String value;
};

template<class TEntry>
struct HeaderVector {
    TEntry* buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

using CommonHeadersVector = HeaderVector<CommonHeader>;
using UncommonHeadersVector = HeaderVector<UncommonHeader>;

struct HTTPHeaderMap {
    CommonHeadersVector common;
    UncommonHeadersVector uncommon;
};

struct ASCIILiteral {
    const char* characters;
};

static_assert(sizeof(CommonHeader) == 16);
static_assert(alignof(CommonHeader) == 8);
static_assert(offsetof(CommonHeader, key) == 0);
static_assert(offsetof(CommonHeader, value) == 8);
static_assert(sizeof(UncommonHeader) == 16);
static_assert(alignof(UncommonHeader) == 8);
static_assert(offsetof(UncommonHeader, key) == 0);
static_assert(offsetof(UncommonHeader, value) == 8);
static_assert(sizeof(CommonHeadersVector) == 16);
static_assert(alignof(CommonHeadersVector) == 8);
static_assert(offsetof(CommonHeadersVector, buffer) == 0);
static_assert(offsetof(CommonHeadersVector, capacity) == 8);
static_assert(offsetof(CommonHeadersVector, size) == 12);
static_assert(sizeof(UncommonHeadersVector) == 16);
static_assert(alignof(UncommonHeadersVector) == 8);
static_assert(offsetof(UncommonHeadersVector, buffer) == 0);
static_assert(offsetof(UncommonHeadersVector, capacity) == 8);
static_assert(offsetof(UncommonHeadersVector, size) == 12);
static_assert(sizeof(HTTPHeaderMap) == 32);
static_assert(alignof(HTTPHeaderMap) == 8);
static_assert(offsetof(HTTPHeaderMap, common) == 0);
static_assert(offsetof(HTTPHeaderMap, uncommon) == 16);
static_assert(sizeof(ASCIILiteral) == 8);
static_assert(alignof(ASCIILiteral) == 8);
static_assert(offsetof(ASCIILiteral, characters) == 0);
static_assert(std::is_trivial_v<ASCIILiteral>);
static_assert(std::is_trivially_copyable_v<ASCIILiteral>);
static_assert(std::is_trivially_destructible_v<ASCIILiteral>);
static_assert(!std::is_trivially_destructible_v<String>);

}

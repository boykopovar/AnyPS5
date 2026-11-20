#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreParsersExports.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WebCore {

struct ParsedContentTypeParameter {
    NKWebKit::WtfAbi::StringImpl* name;
    NKWebKit::WtfAbi::StringImpl* value;
};

struct ParsedContentTypeNames {
    NKWebKit::WtfAbi::StringImpl** buffer;
    std::uint32_t capacity;
    std::uint32_t size;
};

struct ParsedContentType {
    NKWebKit::WtfAbi::StringImpl* m_contentType;
    ParsedContentTypeParameter* m_parameterValues;
    ParsedContentTypeNames m_parameterNames;
    NKWebKit::WtfAbi::StringImpl* m_mimeType;
};

struct OptionalParsedContentType {
    bool engaged;
    ParsedContentType value;
};

static_assert(std::is_trivially_copyable_v<ParsedContentTypeParameter>);
static_assert(sizeof(ParsedContentTypeParameter) == 16);
static_assert(std::is_trivially_copyable_v<ParsedContentTypeNames>);
static_assert(sizeof(ParsedContentTypeNames) == 16);
static_assert(offsetof(ParsedContentTypeNames, capacity) == 8);
static_assert(offsetof(ParsedContentTypeNames, size) == 12);
static_assert(std::is_trivially_copyable_v<ParsedContentType>);
static_assert(sizeof(ParsedContentType) == 40);
static_assert(offsetof(ParsedContentType, m_contentType) == 0);
static_assert(offsetof(ParsedContentType, m_parameterValues) == 8);
static_assert(offsetof(ParsedContentType, m_parameterNames) == 16);
static_assert(offsetof(ParsedContentType, m_mimeType) == 32);
static_assert(std::is_trivially_copyable_v<OptionalParsedContentType>);
static_assert(sizeof(OptionalParsedContentType) == 48);
static_assert(offsetof(OptionalParsedContentType, engaged) == 0);
static_assert(offsetof(OptionalParsedContentType, value) == 8);

}

extern "C" {

WebCore::OptionalParsedContentType* APS5_VABI _ZN7WebCore17ParsedContentType6createERKN3WTF6StringENS_4ModeE(
    WebCore::OptionalParsedContentType* result, const NKWebKit::WtfAbi::String* contentType, WebCore::Mode mode);
NKWebKit::WtfAbi::String APS5_VABI _ZNK7WebCore17ParsedContentType9serializeEv(const WebCore::ParsedContentType* self);

}

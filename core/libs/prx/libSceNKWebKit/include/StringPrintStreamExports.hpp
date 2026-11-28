#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WtfAbi {

struct StringPrintStream {
    const void* const* vtable;
    char* buffer;
    std::size_t next;
    std::size_t size;
    char inlineBuffer[128];
};

struct StringOrUTF8ConversionError {
    union {
        StringImpl* impl;
        std::uint32_t error;
    };
    bool hasValue;
};

static_assert(sizeof(StringPrintStream) == 160);
static_assert(offsetof(StringPrintStream, buffer) == 8);
static_assert(offsetof(StringPrintStream, next) == 16);
static_assert(offsetof(StringPrintStream, size) == 24);
static_assert(offsetof(StringPrintStream, inlineBuffer) == 32);
static_assert(sizeof(StringOrUTF8ConversionError) == 16);
static_assert(offsetof(StringOrUTF8ConversionError, hasValue) == 8);

}

extern "C" {

void APS5_VABI _ZN3WTF17StringPrintStreamC1Ev(NKWebKit::WtfAbi::StringPrintStream* self);
void APS5_VABI _ZN3WTF17StringPrintStreamC2Ev(NKWebKit::WtfAbi::StringPrintStream* self);
void APS5_VABI _ZN3WTF17StringPrintStreamD0Ev(NKWebKit::WtfAbi::StringPrintStream* self);
void APS5_VABI _ZN3WTF17StringPrintStreamD1Ev(NKWebKit::WtfAbi::StringPrintStream* self);
void APS5_VABI _ZN3WTF17StringPrintStreamD2Ev(NKWebKit::WtfAbi::StringPrintStream* self);
void APS5_VABI _ZN3WTF17StringPrintStream7vprintfEPKcP13__va_list_tag(NKWebKit::WtfAbi::StringPrintStream* self, const char* format, VaList* arguments);
NKWebKit::WtfAbi::CString APS5_VABI _ZN3WTF17StringPrintStream9toCStringEv(NKWebKit::WtfAbi::StringPrintStream* self);
NKWebKit::WtfAbi::StringOrUTF8ConversionError APS5_VABI _ZN3WTF17StringPrintStream11tryToStringEv(NKWebKit::WtfAbi::StringPrintStream* self);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF17StringPrintStream8toStringEv(NKWebKit::WtfAbi::StringPrintStream* self);
NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF17StringPrintStream26toStringWithLatin1FallbackEv(NKWebKit::WtfAbi::StringPrintStream* self);
void APS5_VABI _ZN3WTF17StringPrintStream5resetEv(NKWebKit::WtfAbi::StringPrintStream* self);

}

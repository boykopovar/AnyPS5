// Derived from Source/WTF/wtf/StringPrintStream.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/include/StringPrintStreamExports.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringOrUTF8ConversionError;
using NKWebKit::WtfAbi::StringPrintStream;

constexpr std::uint32_t UTF8ConversionErrorOutOfMemory = 1;
constexpr std::size_t StringMaxLength = std::numeric_limits<std::int32_t>::max();

void APS5_VABI StringPrintStreamVtableFlush(StringPrintStream*)
{
}

const void* const Vtable[] = {
    nullptr,
    nullptr,
    reinterpret_cast<const void*>(&_ZN3WTF17StringPrintStreamD1Ev),
    reinterpret_cast<const void*>(&_ZN3WTF17StringPrintStreamD0Ev),
    reinterpret_cast<const void*>(&_ZN3WTF17StringPrintStream7vprintfEPKcP13__va_list_tag),
    reinterpret_cast<const void*>(&StringPrintStreamVtableFlush),
    reinterpret_cast<const void*>(&_ZN3WTF11PrintStream5beginEv),
    reinterpret_cast<const void*>(&_ZN3WTF11PrintStream3endEv),
};

StringPrintStream& Checked(StringPrintStream* self, const char* caller)
{
    if (!self)
        throw std::invalid_argument(std::string("StringPrintStream::") + caller + " requires an object");
    return *self;
}

void Construct(StringPrintStream* self)
{
    auto& stream = Checked(self, "StringPrintStream");
    stream.vtable = &Vtable[2];
    stream.buffer = stream.inlineBuffer;
    stream.next = 0;
    stream.size = sizeof(stream.inlineBuffer);
    stream.buffer[0] = 0;
}

void Destroy(StringPrintStream& stream)
{
    if (stream.buffer == stream.inlineBuffer)
        return;
    _ZN3WTF8fastFreeEPv(stream.buffer);
}

void IncreaseSize(StringPrintStream& stream, std::size_t newSize)
{
    stream.size = newSize << 1;
    char* newBuffer = static_cast<char*>(_ZN3WTF10fastMallocEm(stream.size));
    std::memcpy(newBuffer, stream.buffer, stream.next + 1);
    if (stream.buffer != stream.inlineBuffer)
        _ZN3WTF8fastFreeEPv(stream.buffer);
    stream.buffer = newBuffer;
}

int Format(StringPrintStream& stream, const char* format, VaList* arguments)
{
    const int length = vsnprintf_nid_postfix(stream.buffer + stream.next, stream.size - stream.next, format, arguments);
    if (length < 0)
        throw std::runtime_error("StringPrintStream::vprintf: the format failed");
    return length;
}

}

extern "C" {

void APS5_VABI _ZN3WTF17StringPrintStreamC1Ev(StringPrintStream* self)
{
    Construct(self);
}

void APS5_VABI _ZN3WTF17StringPrintStreamC2Ev(StringPrintStream* self)
{
    Construct(self);
}

void APS5_VABI _ZN3WTF17StringPrintStreamD1Ev(StringPrintStream* self)
{
    Destroy(Checked(self, "~StringPrintStream"));
}

void APS5_VABI _ZN3WTF17StringPrintStreamD2Ev(StringPrintStream* self)
{
    Destroy(Checked(self, "~StringPrintStream"));
}

void APS5_VABI _ZN3WTF17StringPrintStreamD0Ev(StringPrintStream* self)
{
    Destroy(Checked(self, "~StringPrintStream"));
    _ZN3WTF8fastFreeEPv(self);
}

void APS5_VABI _ZN3WTF17StringPrintStream7vprintfEPKcP13__va_list_tag(StringPrintStream* self, const char* format, VaList* arguments)
{
    auto& stream = Checked(self, "vprintf");
    if (!format || !arguments)
        throw std::invalid_argument("StringPrintStream::vprintf requires a format and arguments");

    VaList firstPassArguments = *arguments;
    const int numberOfBytesNotIncludingTerminatorThatWouldHaveBeenWritten = Format(stream, format, &firstPassArguments);
    const std::size_t numberOfBytesThatWouldHaveBeenWritten = static_cast<std::size_t>(numberOfBytesNotIncludingTerminatorThatWouldHaveBeenWritten) + 1;
    if (stream.next + numberOfBytesThatWouldHaveBeenWritten <= stream.size) {
        stream.next += numberOfBytesNotIncludingTerminatorThatWouldHaveBeenWritten;
        return;
    }

    IncreaseSize(stream, stream.next + numberOfBytesThatWouldHaveBeenWritten);
    stream.next += Format(stream, format, arguments);
}

CString APS5_VABI _ZN3WTF17StringPrintStream9toCStringEv(StringPrintStream* self)
{
    auto& stream = Checked(self, "toCString");
    CString result;
    _ZN3WTF7CStringC1EPKcm(&result, stream.buffer, stream.next);
    return result;
}

StringOrUTF8ConversionError APS5_VABI _ZN3WTF17StringPrintStream11tryToStringEv(StringPrintStream* self)
{
    auto& stream = Checked(self, "tryToString");
    StringOrUTF8ConversionError result {};
    if (stream.next > StringMaxLength) {
        result.error = UTF8ConversionErrorOutOfMemory;
        result.hasValue = false;
        return result;
    }
    String value = _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(stream.buffer), stream.next);
    result.impl = std::exchange(value.impl, nullptr);
    result.hasValue = true;
    return result;
}

String APS5_VABI _ZN3WTF17StringPrintStream8toStringEv(StringPrintStream* self)
{
    auto& stream = Checked(self, "toString");
    return _ZN3WTF6String8fromUTF8EPKhm(reinterpret_cast<const unsigned char*>(stream.buffer), stream.next);
}

String APS5_VABI _ZN3WTF17StringPrintStream26toStringWithLatin1FallbackEv(StringPrintStream* self)
{
    auto& stream = Checked(self, "toStringWithLatin1Fallback");
    return _ZN3WTF6String26fromUTF8WithLatin1FallbackEPKhm(reinterpret_cast<const unsigned char*>(stream.buffer), stream.next);
}

void APS5_VABI _ZN3WTF17StringPrintStream5resetEv(StringPrintStream* self)
{
    auto& stream = Checked(self, "reset");
    stream.next = 0;
    stream.buffer[0] = 0;
}

}

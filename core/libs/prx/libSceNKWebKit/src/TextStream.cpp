/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
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

// Derived from Source/WTF/wtf/text/TextStream.cpp, Source/WTF/wtf/Seconds.cpp and Source/WTF/wtf/URL.cpp (WebKit c55364d53f97)

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/WindowsFormatting.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

#include "WtfTextAbi.hpp"

namespace {

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::TextStream;
using NKWebKit::WtfAbi::URL;

constexpr unsigned NumberRespectingIntegers = 1 << 1;

TextStream& Checked(TextStream* stream) {
    if (!stream) throw std::invalid_argument("null WTF::TextStream");
    return *stream;
}

int TruncateToInt(double value) {
    if (!(value > -2147483649.0 && value < 2147483648.0)) return std::numeric_limits<int>::min();
    return static_cast<int>(value);
}

bool HasFractions(double value) {
    constexpr double epsilon = 0.0001;
    return std::fabs(value - static_cast<double>(TruncateToInt(value))) > epsilon;
}

template<typename T> TextStream& AppendNumber(TextStream& stream, T value) {
    char buffer[32];
    auto result = std::to_chars(buffer, buffer + sizeof(buffer) - 1, value);
    if (result.ec != std::errc{}) throw std::runtime_error("WTF integer formatting failed");
    *result.ptr = 0;
    NKWebKit::WtfAbi::AppendCString(stream.text, buffer);
    return stream;
}

TextStream& AppendFixedWidth(TextStream& stream, double value) {
    auto text = NKWebKit::WtfAbi::NumberToFixedWidthString(value, 2);
    NKWebKit::WtfAbi::AppendAdapted(stream.text, text.data(), static_cast<unsigned>(text.size()));
    return stream;
}

TextStream& AppendNumberRespectingIntegers(TextStream& stream, double value) {
    if (HasFractions(value)) return AppendFixedWidth(stream, value);
    return AppendNumber(stream, TruncateToInt(value));
}

TextStream& AppendCString(TextStream& stream, const char* string) {
    NKWebKit::WtfAbi::AppendCString(stream.text, string);
    return stream;
}

TextStream& AppendFloatingPoint(TextStream& stream, double value) {
    if (stream.formattingFlags & NumberRespectingIntegers) return AppendNumberRespectingIntegers(stream, value);
    return AppendFixedWidth(stream, value);
}

void WriteIndent(TextStream& stream) {
    if (!stream.multiLineMode) return;
    for (int i = 0; i < stream.indent; ++i) AppendCString(stream, "  ");
}

}  // namespace

extern "C" {

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEb(TextStream* self, bool value) {
    return AppendCString(Checked(self), value ? "1" : "0");
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEc(TextStream* self, char value) {
    auto& stream = Checked(self);
    NKWebKit::WtfAbi::AppendCharacter(stream.text, static_cast<unsigned char>(value));
    return stream;
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEi(TextStream* self, int value) {
    return AppendNumber(Checked(self), value);
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEj(TextStream* self, unsigned value) {
    return AppendNumber(Checked(self), value);
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEl(TextStream* self, std::int64_t value) {
    return AppendNumber(Checked(self), value);
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEm(TextStream* self, std::uint64_t value) {
    return AppendNumber(Checked(self), value);
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEx(TextStream* self, long long value) {
    return AppendNumber(Checked(self), value);
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEy(TextStream* self, unsigned long long value) {
    return AppendNumber(Checked(self), value);
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEf(TextStream* self, float value) {
    return AppendFloatingPoint(Checked(self), value);
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEd(TextStream* self, double value) {
    return AppendFloatingPoint(Checked(self), value);
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEPKc(TextStream* self, const char* string) {
    return AppendCString(Checked(self), string);
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsEPKv(TextStream* self, const void* pointer) {
    auto& stream = Checked(self);
    auto text = LibcDetail::FormatPointer("%p", pointer);
    return AppendCString(stream, text.c_str());
}

TextStream& APS5_VABI _ZN3WTF10TextStreamlsERKNS_6StringE(TextStream* self, const String& string) {
    auto& stream = Checked(self);
    NKWebKit::WtfAbi::AppendString(stream.text, string.impl);
    return stream;
}

String APS5_VABI _ZN3WTF10TextStream7releaseEv(TextStream* self) {
    auto& stream = Checked(self);
    String result = NKWebKit::WtfAbi::ToString(stream.text);
    NKWebKit::WtfAbi::Clear(stream.text);
    return result;
}

void APS5_VABI _ZN3WTF10TextStream10startGroupEv(TextStream* self) {
    auto& stream = Checked(self);
    if (stream.multiLineMode) {
        AppendCString(stream, "\n");
        WriteIndent(stream);
        AppendCString(stream, "(");
        ++stream.indent;
    } else {
        AppendCString(stream, " (");
    }
}

void APS5_VABI _ZN3WTF10TextStream8endGroupEv(TextStream* self) {
    auto& stream = Checked(self);
    AppendCString(stream, ")");
    if (stream.multiLineMode) --stream.indent;
}

void APS5_VABI _ZN3WTF10TextStream8nextLineEv(TextStream* self) {
    auto& stream = Checked(self);
    if (stream.multiLineMode) {
        AppendCString(stream, "\n");
        WriteIndent(stream);
    } else {
        AppendCString(stream, " ");
    }
}

void APS5_VABI _ZN3WTF10TextStream11writeIndentEv(TextStream* self) {
    WriteIndent(Checked(self));
}

TextStream& APS5_VABI _ZN3WTFlsERNS_10TextStreamENS_7SecondsE(TextStream& stream, WTF::Seconds seconds) {
    AppendFloatingPoint(stream, seconds.m_value);
    return AppendCString(stream, "s");
}

TextStream& APS5_VABI _ZN3WTFlsERNS_10TextStreamERKNS_3URLE(TextStream& stream, const URL& url) {
    NKWebKit::WtfAbi::AppendString(stream.text, url.impl);
    return stream;
}

}

/*
 * Derived from Source/WTF/wtf/PrintStream.cpp at https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/PrintStream.cpp
 *
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

#include "prx/libSceNKWebKit/include/WtfPrintInternalStringImplExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdarg>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::PrintStream;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::CStringBuffer;
using NKWebKit::WtfAbi::StringImpl;
using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);

constexpr unsigned MaxLength = std::numeric_limits<std::int32_t>::max();
constexpr unsigned LenientConversion = 0;

Vprintf VirtualVprintf(PrintStream* stream) {
    if (!stream || !stream->vtable) throw std::invalid_argument("WTF::printInternal(StringImpl*): null stream or vtable");
    const void* entry = stream->vtable[Jsc::WtfAbi::PrintStreamVprintf];
    if (!entry) throw std::invalid_argument("WTF::printInternal(StringImpl*): null vprintf");
    return reinterpret_cast<Vprintf>(const_cast<void*>(entry));
}

void Printf(PrintStream* stream, Vprintf vprintf, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
#else
    std::va_list args;
    va_start(args, format);
#endif
    vprintf(stream, format, reinterpret_cast<VaList*>(args));
#ifdef _WIN32
    __builtin_sysv_va_end(args);
#else
    va_end(args);
#endif
}

void PrintCString(PrintStream* stream, Vprintf vprintf, const char* string) {
    Printf(stream, vprintf, "%s", string);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(PrintStream* out, const StringImpl* string) {
    const Vprintf vprintf = VirtualVprintf(out);
    if (!string) {
        PrintCString(out, vprintf, "(null StringImpl*)");
        return;
    }
    if (string->length > MaxLength / 3) {
        PrintCString(out, vprintf, "(Out of memory while converting ");
        PrintCString(out, vprintf, "StringImpl*");
        PrintCString(out, vprintf, " to utf8)");
        return;
    }
    const CString utf8 = _ZNK3WTF10StringImpl4utf8ENS_14ConversionModeE(string, LenientConversion);
    if (!utf8.buffer) throw std::logic_error("WTF::printInternal(StringImpl*): UTF-8 conversion returned a null CString");
    PrintCString(out, vprintf, reinterpret_cast<const char*>(utf8.buffer + 1));
}

}

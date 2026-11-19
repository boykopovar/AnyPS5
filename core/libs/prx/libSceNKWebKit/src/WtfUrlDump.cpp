/*
 * Derived from Source/WTF/wtf/URL.cpp and Source/WTF/wtf/PrintStream.cpp
 * at https://github.com/WebKit/WebKit/tree/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf
 *
 * URL.cpp:
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
 *
 * PrintStream.cpp:
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

#include "prx/libSceNKWebKit/include/WtfUrlDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::PrintStream;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;
using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using Begin = PrintStream* (APS5_VABI*)(PrintStream*);
using End = void (APS5_VABI*)(PrintStream*);

constexpr unsigned MaxLength = std::numeric_limits<std::int32_t>::max();
constexpr unsigned LenientConversion = 0;

template <typename T>
T Slot(PrintStream* stream, std::size_t slot) {
    if (!stream || !stream->vtable) throw std::invalid_argument("WTF::PrintStream: null stream or vtable");
    const void* entry = stream->vtable[slot];
    if (!entry) throw std::invalid_argument("WTF::PrintStream: null virtual");
    return reinterpret_cast<T>(const_cast<void*>(entry));
}

void Printf(PrintStream* stream, const char* format, ...) {
    const Vprintf vprintf = Slot<Vprintf>(stream, Jsc::WtfAbi::PrintStreamVprintf);
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

void PrintString(PrintStream* out, const StringImpl* impl) {
    if (!impl) {
        Printf(out, "%s", "");
        return;
    }
    if (impl->length > MaxLength / 3) {
        Printf(out, "%s", "(Out of memory while converting ");
        Printf(out, "%s", "String");
        Printf(out, "%s", " to utf8)");
        return;
    }
    const CString utf8 = _ZNK3WTF10StringImpl4utf8ENS_14ConversionModeE(impl, LenientConversion);
    if (!utf8.buffer) throw std::logic_error("WTF::printInternal(String): UTF-8 conversion returned a null CString");
    Printf(out, "%s", reinterpret_cast<const char*>(utf8.buffer + 1));
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamERKNS_6StringE(PrintStream* out, const String* string) {
    if (!string) throw std::invalid_argument("WTF::printInternal(String): null string");
    PrintString(out, string->impl);
}

void APS5_VABI _ZNK3WTF3URL4dumpERNS_11PrintStreamE(const URL* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("WTF::URL::dump: null URL");
    const End end = Slot<End>(out, Jsc::WtfAbi::PrintStreamEnd);
    PrintStream* begun = Slot<Begin>(out, Jsc::WtfAbi::PrintStreamBegin)(out);
    PrintString(begun, self->impl);
    end(out);
}

}

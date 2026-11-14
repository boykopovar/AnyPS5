/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfPrintStreamAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::PrintStream;
using NKWebKit::WtfAbi::RawPointer;

#ifdef _WIN32
using GuestVaList = __builtin_sysv_va_list;
#else
using GuestVaList = std::va_list;
#endif

using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, GuestVaList);
using BeginFunction = PrintStream* (APS5_VABI*)(PrintStream*);
using EndFunction = void (APS5_VABI*)(PrintStream*);

template<typename Function>
Function Slot(PrintStream& out, std::size_t slot) {
    return reinterpret_cast<Function>(const_cast<void*>(out.vtable[slot]));
}

const char* BoolForPrinting(bool value) {
    return value ? "true" : "false";
}

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF11PrintStream6printfEPKcz(PrintStream* self, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    Slot<VprintfFunction>(*self, NKWebKit::WtfAbi::PrintStreamVprintf)(self, format, arguments);
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    Slot<VprintfFunction>(*self, NKWebKit::WtfAbi::PrintStreamVprintf)(self, format, arguments);
    va_end(arguments);
#endif
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEPKc(PrintStream& out, const char* string) {
    _ZN3WTF11PrintStream6printfEPKcz(&out, "%s", string);
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamERKNS_7CStringE(PrintStream& out, const CString& string) {
    _ZN3WTF13printInternalERNS_11PrintStreamEPKc(out, CStringBytes(string));
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEb(PrintStream& out, bool value) {
    PrintStream* stream = Slot<BeginFunction>(out, NKWebKit::WtfAbi::PrintStreamBegin)(&out);
    _ZN3WTF13printInternalERNS_11PrintStreamEPKc(*stream, BoolForPrinting(value));
    Slot<EndFunction>(out, NKWebKit::WtfAbi::PrintStreamEnd)(&out);
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEi(PrintStream& out, int value) {
    _ZN3WTF11PrintStream6printfEPKcz(&out, "%d", value);
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEj(PrintStream& out, unsigned value) {
    _ZN3WTF11PrintStream6printfEPKcz(&out, "%u", value);
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEl(PrintStream& out, std::int64_t value) {
    _ZN3WTF11PrintStream6printfEPKcz(&out, "%ld", value);
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEm(PrintStream& out, std::uint64_t value) {
    _ZN3WTF11PrintStream6printfEPKcz(&out, "%lu", value);
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEd(PrintStream& out, double value) {
    _ZN3WTF11PrintStream6printfEPKcz(&out, "%lf", value);
}

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamENS_10RawPointerE(PrintStream& out, RawPointer value) {
    _ZN3WTF11PrintStream6printfEPKcz(&out, "%p", value.value);
}

void APS5_VABI _ZN3WTF13dumpCharacterERNS_11PrintStreamEc(PrintStream& out, char value) {
    _ZN3WTF11PrintStream6printfEPKcz(&out, "%c", value);
}

}

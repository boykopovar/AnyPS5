/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
 *
 * Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/PlatformTimeRanges.cpp
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
#include "prx/libSceNKWebKit/include/PlatformTimeRangesDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfTimeDumpExports.hpp"
#include "SceTypes.hpp"

#include <cstdarg>
#include <cstddef>
#include <stdexcept>

namespace {

using NKWebKit::WtfDumpAbi::PrintStream;

using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using BeginFunction = PrintStream* (APS5_VABI*)(PrintStream*);
using EndFunction = void (APS5_VABI*)(PrintStream*);

template<typename TFunction>
TFunction Slot(PrintStream& out, std::size_t slot) {
    return reinterpret_cast<TFunction>(const_cast<void*>(out.vtable[slot]));
}

void APS5_VABI PlatformTimeRangesPrintf_nid_no_patch(PrintStream& out, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    Slot<VprintfFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamVprintf)(&out, format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    Slot<VprintfFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamVprintf)(&out, format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
}

void PrintInternal(PrintStream& out, const char* string) {
    PlatformTimeRangesPrintf_nid_no_patch(out, "%s", string);
}

void PrintInternal(PrintStream& out, const WTF::MediaTime& time) {
    _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(&time, &out);
}

template<typename... TValues>
void Print(PrintStream& out, const TValues&... values) {
    PrintStream* stream = Slot<BeginFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamBegin)(&out);
    (PrintInternal(*stream, values), ...);
    Slot<EndFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamEnd)(&out);
}

}  // namespace

extern "C" {

void APS5_VABI _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(const WebCore::PlatformTimeRanges* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    const WebCore::PlatformTimeRanges::VectorRange& ranges = self->m_ranges;
    if (ranges.m_size && !ranges.m_buffer) throw std::invalid_argument(__func__);
    for (unsigned index = 0; index < ranges.m_size; ++index) {
        const WebCore::PlatformTimeRanges::Range& range = ranges.m_buffer[index];
        Print(*out, "[", range.start, "..", range.end, "] ");
    }
}

}

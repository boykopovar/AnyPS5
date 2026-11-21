// Derived from Source/WebCore/platform/graphics/PlatformTimeRanges.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
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
#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/PlatformTimeRangesDumpExports.hpp"
#include "prx/libSceNKWebKit/include/WtfTimeDumpExports.hpp"

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

void APS5_VABI PlatformTimeRangesDumpPrintf_nid_no_patch(PrintStream& out, const char* format, ...) {
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

PrintStream& Checked(PrintStream* out) {
    if (!out || !out->vtable) throw std::invalid_argument("null WTF::PrintStream");
    return *out;
}

PrintStream& Begin(PrintStream& out) {
    return Checked(Slot<BeginFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamBegin)(&out));
}

void End(PrintStream& out) {
    Slot<EndFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamEnd)(&out);
}

void PrintText(PrintStream& out, const char* text) {
    PlatformTimeRangesDumpPrintf_nid_no_patch(out, "%s", text);
}

}

extern "C" {

void APS5_VABI _ZNK7WebCore18PlatformTimeRanges4dumpERN3WTF11PrintStreamE(const WebCore::PlatformTimeRanges* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null WebCore::PlatformTimeRanges");
    PrintStream& outer = Checked(out);
    const WebCore::PlatformTimeRanges::VectorRange& ranges = self->m_ranges;
    if (ranges.m_size > ranges.m_capacity || (ranges.m_size && !ranges.m_buffer)) {
        throw std::invalid_argument("invalid WebCore::PlatformTimeRanges ranges");
    }
    for (std::size_t i = 0; i < _ZNK7WebCore18PlatformTimeRanges6lengthEv(self); ++i) {
        const WTF::MediaTime start = _ZNK7WebCore18PlatformTimeRanges5startEj(self, static_cast<unsigned>(i));
        const WTF::MediaTime end = _ZNK7WebCore18PlatformTimeRanges3endEj(self, static_cast<unsigned>(i));
        PrintStream& stream = Begin(outer);
        PrintText(stream, "[");
        _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(&start, &stream);
        PrintText(stream, "..");
        _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(&end, &stream);
        PrintText(stream, "] ");
        End(outer);
    }
}

}

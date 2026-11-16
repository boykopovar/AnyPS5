/*
 * Copyright (C) 2012-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Yusuke Suzuki <utatane.tea@gmail.com>
 *
 * Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/Seconds.cpp,
 * https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/WallTime.cpp,
 * https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/MonotonicTime.cpp,
 * https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/TimeWithDynamicClockType.cpp,
 * https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/ClockType.cpp,
 * https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.h,
 * https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.cpp and
 * https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/text/ASCIILiteral.cpp
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

/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
 *
 * Derived from https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/MediaTime.cpp
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/MediaExports.hpp"
#include "prx/libSceNKWebKit/include/WtfTimeDumpExports.hpp"
#include "SceTypes.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
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

void APS5_VABI Printf_nid_no_patch(PrintStream& out, const char* format, ...) {
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
    Printf_nid_no_patch(out, "%s", string);
}

void PrintInternal(PrintStream& out, double value) {
    Printf_nid_no_patch(out, "%lf", value);
}

void PrintInternal(PrintStream& out, std::int64_t value) {
    Printf_nid_no_patch(out, "%ld", value);
}

void PrintInternal(PrintStream& out, std::uint32_t value) {
    Printf_nid_no_patch(out, "%u", value);
}

void PrintInternal(PrintStream& out, WTF::ClockType type);

template<typename... TValues>
void Print(PrintStream& out, const TValues&... values) {
    PrintStream* stream = Slot<BeginFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamBegin)(&out);
    (PrintInternal(*stream, values), ...);
    Slot<EndFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamEnd)(&out);
}

const char* ClockTypeName(WTF::ClockType type) {
    switch (type) {
    case WTF::ClockType::Wall:
        return "Wall";
    case WTF::ClockType::Monotonic:
        return "Monotonic";
    case WTF::ClockType::Approximate:
        return "Approximate";
    }
    return nullptr;
}

void PrintInternal(PrintStream& out, WTF::ClockType type) {
    Print(out, ClockTypeName(type));
}

}  // namespace

extern "C" {

void APS5_VABI _ZNK3WTF7Seconds4dumpERNS_11PrintStreamE(const WTF::Seconds* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    Print(*out, self->m_value, " sec");
}

void APS5_VABI _ZNK3WTF8WallTime4dumpERNS_11PrintStreamE(const WTF::WallTime* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    Print(*out, "Wall(", self->m_value, " sec)");
}

void APS5_VABI _ZNK3WTF13MonotonicTime4dumpERNS_11PrintStreamE(const WTF::MonotonicTime* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    Print(*out, "Monotonic(", self->m_value, " sec)");
}

void APS5_VABI _ZNK3WTF24TimeWithDynamicClockType4dumpERNS_11PrintStreamE(const WTF::TimeWithDynamicClockType* self, PrintStream* out) {
    if (!self || !out || !ClockTypeName(self->m_type)) throw std::invalid_argument(__func__);
    Print(*out, self->m_type, "(", self->m_value, " sec)");
}

void APS5_VABI _ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE(const WTF::MediaTime* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    Print(*out, "{");
    if (!(self->m_timeFlags & WTF::MediaTime::DoubleValue)) {
        const std::int64_t timeValue = self->m_timeValue;
        const std::uint32_t timeScale = self->m_timeScale;
        Print(*out, timeValue, "/", timeScale, " = ");
    }
    Print(*out, _ZNK3WTF9MediaTime8toDoubleEv(self), "}");
}

void APS5_VABI _ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE(const NKWebKit::HttpAbi::ASCIILiteral* self, PrintStream* out) {
    if (!self || !out) throw std::invalid_argument(__func__);
    Print(*out, self->characters);
}

}

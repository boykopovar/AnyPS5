/*
 * Derived from Source/JavaScriptCore/runtime/JSArrayBufferView.cpp and Source/WTF/wtf/PrintStream.cpp
 * at https://github.com/WebKit/WebKit/tree/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source
 *
 * JSArrayBufferView.cpp:
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#include "prx/libSceNKWebKit/include/WtfTypedArrayModePrintExports.hpp"

#include <array>
#include <cstdarg>
#include <cstddef>
#include <stdexcept>

namespace {

using Jsc::WtfAbi::PrintStream;
using Vprintf = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using Begin = PrintStream* (APS5_VABI*)(PrintStream*);
using End = void (APS5_VABI*)(PrintStream*);

constexpr std::array<const char*, 4> ModeNames{
    "FastTypedArray",
    "OversizeTypedArray",
    "WastefulTypedArray",
    "DataViewMode",
};

template <typename T>
T Slot(PrintStream* stream, std::size_t slot) {
    if (!stream || !stream->vtable) throw std::invalid_argument("WTF::printInternal(TypedArrayMode): null stream or vtable");
    const void* entry = stream->vtable[slot];
    if (!entry) throw std::invalid_argument("WTF::printInternal(TypedArrayMode): null PrintStream virtual");
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

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(PrintStream* out, std::uint32_t mode) {
    if (mode >= ModeNames.size()) throw std::out_of_range("WTF::printInternal(TypedArrayMode): mode outside JSC::TypedArrayMode");
    const End end = Slot<End>(out, Jsc::WtfAbi::PrintStreamEnd);
    PrintStream* begun = Slot<Begin>(out, Jsc::WtfAbi::PrintStreamBegin)(out);
    Printf(begun, "%s", ModeNames[mode]);
    end(out);
}

}

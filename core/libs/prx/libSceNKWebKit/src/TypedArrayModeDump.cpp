/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

// https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSArrayBufferView.cpp
// https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSArrayBufferView.h

#include "prx/libSceNKWebKit/include/TypedArrayModeDumpExports.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using NKWebKit::WtfDumpAbi::PrintStream;
using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using BeginFunction = PrintStream* (APS5_VABI*)(PrintStream*);
using EndFunction = void (APS5_VABI*)(PrintStream*);

const char* ModeName(std::uint8_t mode) {
    switch (mode) {
    case 0x10: return "FastTypedArray";
    case 0x30: return "OversizeTypedArray";
    case 0x58: return "WastefulTypedArray";
    case 0x5A: return "GrowableSharedWastefulTypedArray";
    case 0x5B: return "GrowableSharedAutoLengthWastefulTypedArray";
    case 0x5C: return "ResizableNonSharedWastefulTypedArray";
    case 0x5D: return "ResizableNonSharedAutoLengthWastefulTypedArray";
    case 0x88: return "DataViewMode";
    case 0x8A: return "GrowableSharedDataViewMode";
    case 0x8B: return "GrowableSharedAutoLengthDataViewMode";
    case 0x8C: return "ResizableNonSharedDataViewMode";
    case 0x8D: return "ResizableNonSharedAutoLengthDataViewMode";
    default: return nullptr;
    }
}

template<typename TFunction>
TFunction Slot(PrintStream* stream, std::size_t slot, const char* function) {
    PrintStream value{};
    if (!GuestCopyIn_nid_no_patch(&value, stream, sizeof(value)) || !value.vtable) NotImplemented_nid_no_patch(function);
    const auto address = reinterpret_cast<std::uintptr_t>(value.vtable);
    const auto offset = slot * sizeof(void*);
    if (offset > std::numeric_limits<std::uintptr_t>::max() - address) NotImplemented_nid_no_patch(function);
    TFunction callback = nullptr;
    if (!GuestCopyIn_nid_no_patch(&callback, reinterpret_cast<const void*>(address + offset), sizeof(callback)) || !callback) {
        NotImplemented_nid_no_patch(function);
    }
    return callback;
}

void APS5_VABI Printf_nid_no_patch(PrintStream* stream, VprintfFunction callback, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    callback(stream, format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    callback(stream, format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
}

}

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE(PrintStream* out, std::uint8_t mode) {
    const char* name = ModeName(mode);
    if (!name) NotImplemented_nid_no_patch(__func__);
    PrintStream* target = Slot<BeginFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamBegin, __func__)(out);
    const auto write = Slot<VprintfFunction>(target, NKWebKit::WtfDumpAbi::PrintStreamVprintf, __func__);
    Printf_nid_no_patch(target, write, "%s", name);
    Slot<EndFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamEnd, __func__)(out);
}

}

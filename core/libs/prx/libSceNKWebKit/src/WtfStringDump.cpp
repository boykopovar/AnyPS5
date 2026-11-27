// Derived from Source/WTF/wtf/PrintStream.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WTF/wtf/URL.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/include/WtfStringDumpExports.hpp"

#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace {

using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WtfDumpAbi::PrintStream;

using VprintfFunction = void (APS5_VABI*)(PrintStream*, const char*, VaList*);
using BeginFunction = PrintStream* (APS5_VABI*)(PrintStream*);
using EndFunction = void (APS5_VABI*)(PrintStream*);

constexpr unsigned LenientConversion = 0;
constexpr unsigned kStringImplFlag8Bit = 4u;

static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(std::is_trivially_copyable_v<URL>);

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template<typename TFunction>
TFunction ReadSlot(PrintStream* out, std::size_t slot, const char* function) {
    if (!out) throw std::invalid_argument("null WTF::PrintStream");
    RequireReadableGuestBytes(out, sizeof(void*), function);
    void* const* vtablePtr = nullptr;
    if (!GuestCopyIn_nid_no_patch(&vtablePtr, out, sizeof(vtablePtr)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!vtablePtr) throw std::invalid_argument("null WTF::PrintStream");
    const std::size_t byteOffset = slot * sizeof(void*);
    const auto baseAddress = reinterpret_cast<std::uintptr_t>(vtablePtr);
    if (byteOffset > std::numeric_limits<std::uintptr_t>::max() - baseAddress)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const void* slotAddress = reinterpret_cast<const void*>(baseAddress + byteOffset);
    RequireReadableGuestBytes(slotAddress, sizeof(void*), function);
    void* slotValue = nullptr;
    if (!GuestCopyIn_nid_no_patch(&slotValue, slotAddress, sizeof(slotValue)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!slotValue) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return reinterpret_cast<TFunction>(slotValue);
}

String CopyGuestString(const StringImpl* guestImpl, const char* function) {
    if (!guestImpl) return {};
    RequireReadableGuestBytes(guestImpl, sizeof(StringImpl), function);
    StringImpl header{};
    if (!GuestCopyIn_nid_no_patch(&header, guestImpl, sizeof(header)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (!header.length) return {};
    if (!header.data) throw std::invalid_argument("null WTF character buffer");
    const bool is8Bit = (header.hashAndFlags & kStringImplFlag8Bit) != 0;
    const std::size_t unitSize = is8Bit ? 1u : sizeof(char16_t);
    if (header.length > std::numeric_limits<std::size_t>::max() / unitSize)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * unitSize;
    RequireReadableGuestBytes(header.data, byteLength, function);
    void* characters = nullptr;
    String copy(NKWebKit::WtfAbi::CreateUninitialized(header.length, is8Bit, characters));
    if (!GuestCopyIn_nid_no_patch(characters, header.data, byteLength))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return copy;
}

void APS5_VABI StringDumpPrintf_nid_no_patch(PrintStream* out, VprintfFunction vprintfFn, const char* format, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, format);
    vprintfFn(out, format, reinterpret_cast<VaList*>(arguments));
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, format);
    vprintfFn(out, format, reinterpret_cast<VaList*>(arguments));
    va_end(arguments);
#endif
}

void PrintCString(PrintStream* out, VprintfFunction vprintfFn, const char* string) {
    StringDumpPrintf_nid_no_patch(out, vprintfFn, "%s", string);
}

void PrintCopiedUTF8(PrintStream* out, VprintfFunction vprintfFn, const String& string) {
    if (!string.impl) {
        PrintCString(out, vprintfFn, "");
        return;
    }
    CString utf8 = _ZNK3WTF10StringImpl4utf8ENS_14ConversionModeE(string.impl, LenientConversion);
    PrintCString(out, vprintfFn, utf8.buffer ? reinterpret_cast<const char*>(utf8.buffer + 1) : "");
}

struct PrintStreamScope {
    PrintStream* out{nullptr};
    EndFunction endFn{nullptr};

    ~PrintStreamScope() {
        if (endFn) endFn(out);
    }
};

}  // namespace

extern "C" {

void APS5_VABI _ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE(PrintStream* out, const StringImpl* string) {
    const auto vprintfFn = ReadSlot<VprintfFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamVprintf, __func__);
    if (!string) {
        PrintCString(out, vprintfFn, "(null StringImpl*)");
        return;
    }
    const String copied = CopyGuestString(string, __func__);
    PrintCopiedUTF8(out, vprintfFn, copied);
}

void APS5_VABI _ZNK3WTF3URL4dumpERNS_11PrintStreamE(const URL* self, PrintStream* out) {
    if (!self) throw std::invalid_argument("null WTF::URL");
    RequireReadableGuestBytes(self, sizeof(URL), __func__);
    URL local{};
    if (!GuestCopyIn_nid_no_patch(&local, self, sizeof(local)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    const auto beginFn = ReadSlot<BeginFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamBegin, __func__);
    const auto endFn = ReadSlot<EndFunction>(out, NKWebKit::WtfDumpAbi::PrintStreamEnd, __func__);
    const String copied = CopyGuestString(local.impl, __func__);
    PrintStream* stream = beginFn(out);
    const PrintStreamScope scope{out, endFn};
    const auto innerVprintfFn = ReadSlot<VprintfFunction>(stream, NKWebKit::WtfDumpAbi::PrintStreamVprintf, __func__);
    PrintCopiedUTF8(stream, innerVprintfFn, copied);
}

}

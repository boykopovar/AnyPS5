/* Derived from Source/WebCore/dom/ExceptionData.cpp@c55364d53f971f63a709976f168d932d558b8909 */
/* Derived from Source/WebCore/dom/ExceptionData.h@c55364d53f971f63a709976f168d932d558b8909 */
/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libSceNKWebKit/include/ExceptionDataExports.hpp"
#include "prx/libSceNKWebKit/include/WtfValueExports.hpp"
#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::ExceptionData;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static_assert(std::is_trivially_copyable_v<StringImpl>);

template<typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

String IsolatedCopyGuestString(const StringImpl* impl, const char* function) {
    if (!impl) return String();
    const StringImpl header = ReadGuestValue(impl, function);
    if (!header.length) return String(NKWebKit::WtfAbi::EmptyStringImpl());
    if (!header.data) throw std::invalid_argument("null WTF character buffer");
    const bool is8Bit = (header.hashAndFlags & 4u) != 0;
    const std::size_t byteLength = static_cast<std::size_t>(header.length) * (is8Bit ? 1u : sizeof(char16_t));
    RequireReadableGuestBytes(header.data, byteLength, function);
    if (is8Bit) {
        std::vector<unsigned char> narrow(header.length);
        if (!GuestCopyIn_nid_no_patch(narrow.data(), header.data, byteLength))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        return _ZN3WTF10StringImpl6createEPKhj(narrow.data(), header.length);
    }
    std::vector<char16_t> wide(header.length);
    if (!GuestCopyIn_nid_no_patch(wide.data(), header.data, byteLength))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return _ZN3WTF10StringImpl6createEPKDsj(wide.data(), header.length);
}

}  // namespace

extern "C" {

NKWebKit::WebCoreAbi::ExceptionData APS5_VABI _ZNK7WebCore13ExceptionData12isolatedCopyEv(const NKWebKit::WebCoreAbi::ExceptionData* self) {
    if (!self) throw std::invalid_argument("null ExceptionData");
    unsigned char raw[sizeof(ExceptionData)]{};
    if (!GuestCopyIn_nid_no_patch(raw, self, sizeof(raw)))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    std::uint32_t code = 0;
    const StringImpl* impl = nullptr;
    std::memcpy(&code, raw + offsetof(ExceptionData, code), sizeof(code));
    std::memcpy(&impl, raw + offsetof(ExceptionData, message), sizeof(impl));
    return { code, IsolatedCopyGuestString(impl, __func__) };
}

}

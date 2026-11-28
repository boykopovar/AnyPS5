// Derived from Source/WebCore/platform/network/AuthenticationChallengeBase.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WebCore/platform/network/curl/AuthenticationChallenge.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
// Derived from Source/WebCore/platform/network/curl/AuthenticationChallengeCurl.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "prx/libSceNKWebKit/include/AuthenticationChallengeExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <stdexcept>

#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <cstddef>
#include <type_traits>

namespace {

using NKWebKit::WebCoreAbi::AuthenticationChallenge;
using NKWebKit::WtfAbi::StringImpl;

static_assert(std::is_trivially_copyable_v<AuthenticationChallenge>);
static_assert(std::is_trivially_copyable_v<StringImpl>);

constexpr unsigned kStringImplFlag8Bit = 4u;
constexpr std::size_t kCompareChunkUnits = 2048u;

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

StringImpl ReadAndValidateStringImpl(const StringImpl* impl, const char* function) {
    const StringImpl header = ReadGuestValue(impl, function);
    if (header.length) {
        if (!header.data) throw std::invalid_argument("null WTF character buffer");
        const bool is8Bit = (header.hashAndFlags & kStringImplFlag8Bit) != 0;
        const std::size_t byteLength = static_cast<std::size_t>(header.length) * (is8Bit ? 1u : sizeof(char16_t));
        RequireReadableGuestBytes(header.data, byteLength, function);
    }
    return header;
}

bool EqualGuestStrings(const StringImpl* a, const StringImpl* b, const char* function) {
    if (!a && !b) return true;
    if (!a) {
        static_cast<void>(ReadAndValidateStringImpl(b, function));
        return false;
    }
    if (!b) {
        static_cast<void>(ReadAndValidateStringImpl(a, function));
        return false;
    }
    const StringImpl headerA = ReadAndValidateStringImpl(a, function);
    if (a == b) return true;
    const StringImpl headerB = ReadAndValidateStringImpl(b, function);
    if (headerA.length != headerB.length) return false;
    if (!headerA.length) return true;
    const bool a8Bit = (headerA.hashAndFlags & kStringImplFlag8Bit) != 0;
    const bool b8Bit = (headerB.hashAndFlags & kStringImplFlag8Bit) != 0;
    const auto* const rawA = static_cast<const unsigned char*>(headerA.data);
    const auto* const rawB = static_cast<const unsigned char*>(headerB.data);
    unsigned char bufA8[kCompareChunkUnits];
    unsigned char bufB8[kCompareChunkUnits];
    char16_t bufA16[kCompareChunkUnits];
    char16_t bufB16[kCompareChunkUnits];
    std::size_t offset = 0;
    const std::size_t total = headerA.length;
    while (offset < total) {
        const std::size_t chunk = std::min<std::size_t>(kCompareChunkUnits, total - offset);
        if (a8Bit) {
            if (!GuestCopyIn_nid_no_patch(bufA8, rawA + offset, chunk))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        } else {
            if (!GuestCopyIn_nid_no_patch(bufA16, rawA + offset * sizeof(char16_t), chunk * sizeof(char16_t)))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        if (b8Bit) {
            if (!GuestCopyIn_nid_no_patch(bufB8, rawB + offset, chunk))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        } else {
            if (!GuestCopyIn_nid_no_patch(bufB16, rawB + offset * sizeof(char16_t), chunk * sizeof(char16_t)))
                NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        }
        for (std::size_t i = 0; i < chunk; ++i) {
            const char16_t unitA = a8Bit ? static_cast<char16_t>(bufA8[i]) : bufA16[i];
            const char16_t unitB = b8Bit ? static_cast<char16_t>(bufB8[i]) : bufB16[i];
            if (unitA != unitB) return false;
        }
        offset += chunk;
    }
    return true;
}

}

extern "C" {

bool APS5_VABI _ZNK7WebCore27AuthenticationChallengeBase6isNullEv(const AuthenticationChallenge* self) {
    if (!self) throw std::invalid_argument("null WebCore::AuthenticationChallenge");
    return ReadGuestValue(self, __func__).isNull != 0;
}

void APS5_VABI _ZN7WebCore27AuthenticationChallengeBase7nullifyEv(AuthenticationChallenge* self) {
    if (!self) throw std::invalid_argument("null WebCore::AuthenticationChallenge");
    const int savedErrno = errno;
    bool writable = false;
    {
        const GuestArena::HostWrite guard(self, sizeof(*self));
        writable = guard.Open() && GuestWritable_nid_no_patch(self, sizeof(*self));
    }
    if (!writable) {
        errno = savedErrno;
        throw std::invalid_argument("unwritable WebCore::AuthenticationChallenge");
    }
    const unsigned char isNullValue = 1;
    const bool written = GuestCopyOut_nid_no_patch(
        reinterpret_cast<unsigned char*>(self) + offsetof(AuthenticationChallenge, isNull),
        &isNullValue,
        sizeof(isNullValue));
    errno = savedErrno;
    if (!written) throw std::invalid_argument("unwritable WebCore::AuthenticationChallenge");
}

void* APS5_VABI _ZNK7WebCore23AuthenticationChallenge20authenticationClientEv(const AuthenticationChallenge*) {
    return nullptr;
}

bool APS5_VABI _ZN7WebCore23AuthenticationChallenge15platformCompareERKS0_S2_(const AuthenticationChallenge* a, const AuthenticationChallenge* b) {
    if (!a || !b) throw std::invalid_argument("null WebCore::AuthenticationChallenge");
    const AuthenticationChallenge left = ReadGuestValue(a, __func__);
    const AuthenticationChallenge right = ReadGuestValue(b, __func__);
    if (left.isForMainResource != right.isForMainResource) return false;
    if (left.isInMainFrame != right.isInMainFrame) return false;
    return EqualGuestStrings(left.requestURL.impl, right.requestURL.impl, __func__);
}

}

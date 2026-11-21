/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 * 3.  Neither the name of Google, Inc. ("Google") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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
 *
 * Derived from Source/WebCore/page/SecurityPolicy.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba and Source/WTF/wtf/URL.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 */

#include "prx/libSceNKWebKit/include/SecurityPolicyExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

using NKWebKit::WebCoreAbi::LocalLoadPolicy;
using NKWebKit::WtfAbi::String;

LocalLoadPolicy localLoadPolicy = NKWebKit::WebCoreAbi::AllowLocalLoadsForLocalOnly;

constexpr std::string_view LongestProtocol = "https:";
constexpr std::size_t GuestPageSize = 4096;

void RequireCopied(bool copied, int savedErrno, const char* what) {
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string("WebCore::SecurityPolicy::shouldHideReferrer: unreadable ") + what);
}

std::u16string ProtocolPrefix(const String* string) {
    const int savedErrno = errno;
    NKWebKit::WtfAbi::StringImpl* guestImpl = nullptr;
    RequireCopied(GuestCopyIn_nid_no_patch(&guestImpl, &string->impl, sizeof(guestImpl)), savedErrno, "referrer");
    std::u16string prefix;
    if (!guestImpl) return prefix;
    NKWebKit::WtfAbi::StringImpl impl{};
    RequireCopied(GuestCopyIn_nid_no_patch(&impl, guestImpl, sizeof(impl)), savedErrno, "referrer implementation");
    if (impl.length && !impl.data) throw std::invalid_argument("null WTF character buffer");
    const std::size_t width = impl.hashAndFlags & 4 ? 1 : 2;
    const auto* cursor = static_cast<const unsigned char*>(impl.data);
    std::uint64_t remaining = std::uint64_t{impl.length} * width;
    unsigned char chunk[GuestPageSize];
    bool isLeading = true;
    bool decided = false;
    while (remaining && !decided) {
        std::size_t size = GuestPageSize - reinterpret_cast<std::uintptr_t>(cursor) % GuestPageSize;
        size = static_cast<std::size_t>(std::min<std::uint64_t>(size, remaining));
        size -= size % width;
        if (!size) size = width;
        RequireCopied(GuestCopyIn_nid_no_patch(chunk, cursor, size), savedErrno, "referrer characters");
        for (std::size_t offset = 0; offset < size && !decided; offset += width) {
            char16_t codeUnit = chunk[offset];
            if (width == 2) std::memcpy(&codeUnit, chunk + offset, sizeof(codeUnit));
            if (isLeading) {
                if (codeUnit <= ' ') continue;
                isLeading = false;
            } else {
                if (codeUnit == '\t' || codeUnit == '\r' || codeUnit == '\n') continue;
            }
            prefix.push_back(codeUnit);
            decided = prefix.size() == LongestProtocol.size() || (codeUnit | 32) != LongestProtocol[prefix.size() - 1];
        }
        cursor += size;
        remaining -= size;
    }
    return prefix;
}

bool ProtocolIs(const std::u16string& prefix, const char* protocol) {
    for (const char16_t codeUnit : prefix) {
        char expectedCharacter = *protocol++;
        if (!expectedCharacter) return codeUnit == ':';
        if ((codeUnit | 32) != expectedCharacter) return false;
    }
    return false;
}

}

namespace NKWebKit::WebCoreAbi {

bool SecurityPolicyRestrictAccessToLocal() {
    return localLoadPolicy != AllowLocalLoadsForAll;
}

bool SecurityPolicyAllowSubstituteDataAccessToLocal() {
    return localLoadPolicy != AllowLocalLoadsForLocalOnly;
}

}

extern "C" {

void APS5_VABI _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(LocalLoadPolicy policy) {
    localLoadPolicy = policy;
}

bool APS5_VABI _ZN7WebCore14SecurityPolicy18shouldHideReferrerERKN3WTF3URLERKNS1_6StringE(const NKWebKit::WtfAbi::URL* url, const String* referrer) {
    if (!referrer) throw std::invalid_argument("null WTF::String reference");
    const std::u16string prefix = ProtocolPrefix(referrer);
    bool referrerIsSecureURL = ProtocolIs(prefix, "https");
    bool referrerIsWebURL = referrerIsSecureURL || ProtocolIs(prefix, "http");
    if (!referrerIsWebURL) return true;
    if (!referrerIsSecureURL) return false;
    bool urlIsSecureURL = _ZNK3WTF3URL10protocolIsENS_10StringViewE(url, {"https", 5, true});
    return !urlIsSecureURL;
}

}

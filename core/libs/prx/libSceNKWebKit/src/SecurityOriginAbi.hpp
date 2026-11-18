// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/SecurityOrigin.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
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

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/SecurityOriginData.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
 /*
 * Copyright (C) 2011, 2015, 2023 Apple Inc. All rights reserved.
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

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/ProcessQualified.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace NKWebKit::WebCoreAbi {

struct OptionalPort {
    std::uint16_t value;
    bool hasValue;
};

struct OptionalBool {
    bool value;
    bool hasValue;
};

struct SecurityOriginTuple {
    WtfAbi::String protocol;
    WtfAbi::String host;
    OptionalPort port;
};

struct OpaqueOriginIdentifier {
    std::uint64_t object;
    std::uint64_t processIdentifier;
};

union SecurityOriginDataStorage {
    SecurityOriginTuple tuple;
    OpaqueOriginIdentifier opaque;

    SecurityOriginDataStorage() : opaque{} {}
    ~SecurityOriginDataStorage() {}
};

struct SecurityOriginData {
    SecurityOriginDataStorage storage;
    std::uint32_t index;

    SecurityOriginData() = default;
    SecurityOriginData(const SecurityOriginData&);
    SecurityOriginData(SecurityOriginData&&) noexcept;
};

struct SecurityOrigin {
    mutable std::atomic<unsigned> refCount;
    SecurityOriginData data;
    WtfAbi::String domain;
    WtfAbi::String filePath;
    bool universalAccess;
    bool domainWasSetInDOM;
    bool canLoadLocalResources;
    bool enforcesFilePathSeparation;
    bool needsStorageAccessFromFileURLsQuirk;
    mutable OptionalBool isPotentiallyTrustworthy;
    bool isLocal;

    void ref() const { refCount.fetch_add(1); }
    void deref() const;
};

constexpr std::uint32_t kSecurityOriginTupleIndex = 0;
constexpr std::uint32_t kSecurityOriginOpaqueIndex = 1;

inline SecurityOriginData::SecurityOriginData(const SecurityOriginData& other) {
    if (other.index == kSecurityOriginTupleIndex) {
        new (&storage.tuple) SecurityOriginTuple{other.storage.tuple};
        index = kSecurityOriginTupleIndex;
    } else {
        storage.opaque = other.storage.opaque;
        index = other.index;
    }
}

inline SecurityOriginData::SecurityOriginData(SecurityOriginData&& other) noexcept {
    if (other.index == kSecurityOriginTupleIndex) {
        new (&storage.tuple) SecurityOriginTuple{std::move(other.storage.tuple)};
        index = kSecurityOriginTupleIndex;
    } else {
        storage.opaque = other.storage.opaque;
        index = other.index;
    }
}

static_assert(sizeof(OptionalPort) == 4);
static_assert(alignof(OptionalPort) == 2);
static_assert(offsetof(OptionalPort, value) == 0);
static_assert(offsetof(OptionalPort, hasValue) == 2);
static_assert(sizeof(OptionalBool) == 2);
static_assert(offsetof(OptionalBool, value) == 0);
static_assert(offsetof(OptionalBool, hasValue) == 1);
static_assert(sizeof(SecurityOriginTuple) == 24);
static_assert(offsetof(SecurityOriginTuple, protocol) == 0);
static_assert(offsetof(SecurityOriginTuple, host) == 8);
static_assert(offsetof(SecurityOriginTuple, port) == 16);
static_assert(sizeof(OpaqueOriginIdentifier) == 16);
static_assert(offsetof(OpaqueOriginIdentifier, object) == 0);
static_assert(offsetof(OpaqueOriginIdentifier, processIdentifier) == 8);
static_assert(sizeof(SecurityOriginDataStorage) == 24);
static_assert(sizeof(SecurityOriginData) == 32);
static_assert(alignof(SecurityOriginData) == 8);
static_assert(offsetof(SecurityOriginData, storage) == 0);
static_assert(offsetof(SecurityOriginData, index) == 24);
static_assert(sizeof(std::atomic<unsigned>) == 4);
static_assert(sizeof(SecurityOrigin) == 64);
static_assert(alignof(SecurityOrigin) == 8);
static_assert(offsetof(SecurityOrigin, refCount) == 0);
static_assert(offsetof(SecurityOrigin, data) == 8);
static_assert(offsetof(SecurityOrigin, domain) == 40);
static_assert(offsetof(SecurityOrigin, filePath) == 48);
static_assert(offsetof(SecurityOrigin, universalAccess) == 56);
static_assert(offsetof(SecurityOrigin, domainWasSetInDOM) == 57);
static_assert(offsetof(SecurityOrigin, canLoadLocalResources) == 58);
static_assert(offsetof(SecurityOrigin, enforcesFilePathSeparation) == 59);
static_assert(offsetof(SecurityOrigin, needsStorageAccessFromFileURLsQuirk) == 60);
static_assert(offsetof(SecurityOrigin, isPotentiallyTrustworthy) == 61);
static_assert(offsetof(SecurityOrigin, isLocal) == 63);
static_assert(std::is_standard_layout_v<SecurityOrigin>);

bool ShouldTreatURLSchemeAsNoAccess(const WtfAbi::StringImpl* scheme);
bool ShouldPartitionCacheForURLScheme(const WtfAbi::StringImpl* scheme);

}  // namespace NKWebKit::WebCoreAbi

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/page/SecurityOriginData.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
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

#include "prx/libSceNKWebKit/include/SecurityOriginDataEqualityExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

using NKWebKit::WebCoreAbi::OpaqueOriginIdentifier;
using NKWebKit::WebCoreAbi::OptionalPort;
using NKWebKit::WebCoreAbi::SecurityOriginData;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;

constexpr std::uint32_t Valueless = std::numeric_limits<std::uint32_t>::max();

std::uint32_t Index(const SecurityOriginData& data) {
    if (data.index != Valueless && data.index > NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex)
        throw std::invalid_argument("invalid WebCore::SecurityOriginData variant index");
    return data.index;
}

bool EqualPorts(const OptionalPort& a, const OptionalPort& b) {
    return a.hasValue == b.hasValue && (!a.hasValue || a.value == b.value);
}

bool EqualTuples(const SecurityOriginTuple& a, const SecurityOriginTuple& b) {
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.protocol.impl, b.protocol.impl)
        && _ZN3WTF5equalEPKNS_10StringImplES2_(a.host.impl, b.host.impl)
        && EqualPorts(a.port, b.port);
}

bool EqualIdentifiers(const OpaqueOriginIdentifier& a, const OpaqueOriginIdentifier& b) {
    return a.object == b.object && a.processIdentifier == b.processIdentifier;
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(const SecurityOriginData* a, const SecurityOriginData* b) {
    if (!a || !b) throw std::invalid_argument(__func__);
    if (a == b) return true;
    const std::uint32_t index = Index(*a);
    if (index != Index(*b)) return false;
    if (index == Valueless) return true;
    if (index == NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex) return EqualTuples(a->storage.tuple, b->storage.tuple);
    return EqualIdentifiers(a->storage.opaque, b->storage.opaque);
}

}

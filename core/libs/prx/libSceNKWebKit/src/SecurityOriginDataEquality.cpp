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

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <type_traits>

namespace {

using NKWebKit::WebCoreAbi::OpaqueOriginIdentifier;
using NKWebKit::WebCoreAbi::OptionalPort;
using NKWebKit::WebCoreAbi::SecurityOriginData;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WtfAbi::StringImpl;

static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(std::is_trivially_copyable_v<OpaqueOriginIdentifier>);

constexpr std::uint32_t Valueless = std::numeric_limits<std::uint32_t>::max();
constexpr unsigned kStringImplFlag8Bit = 4u;
constexpr std::size_t kCompareChunkUnits = 2048u;

struct PortSnapshot {
    std::uint16_t value = 0;
    bool hasValue = false;
};

struct TupleSnapshot {
    const StringImpl* protocol = nullptr;
    const StringImpl* host = nullptr;
    PortSnapshot port{};
};

struct OriginSnapshot {
    unsigned char raw[sizeof(SecurityOriginData)]{};
    std::uint32_t index = 0;
};

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

OriginSnapshot ReadOriginSnapshot(const SecurityOriginData* data, const char* function) {
    OriginSnapshot snapshot{};
    if (!GuestCopyIn_nid_no_patch(snapshot.raw, data, sizeof(snapshot.raw)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    std::memcpy(&snapshot.index, snapshot.raw + offsetof(SecurityOriginData, index), sizeof(snapshot.index));
    return snapshot;
}

std::uint32_t Index(const OriginSnapshot& data) {
    if (data.index != Valueless && data.index > NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex)
        throw std::invalid_argument("invalid WebCore::SecurityOriginData variant index");
    return data.index;
}

TupleSnapshot ExtractTuple(const OriginSnapshot& snapshot) {
    TupleSnapshot tuple{};
    const unsigned char* const base = snapshot.raw + offsetof(SecurityOriginData, storage);
    std::memcpy(&tuple.protocol, base + offsetof(SecurityOriginTuple, protocol), sizeof(tuple.protocol));
    std::memcpy(&tuple.host, base + offsetof(SecurityOriginTuple, host), sizeof(tuple.host));
    const unsigned char* const portBase = base + offsetof(SecurityOriginTuple, port);
    std::memcpy(&tuple.port.value, portBase + offsetof(OptionalPort, value), sizeof(tuple.port.value));
    unsigned char hasValueByte = 0;
    std::memcpy(&hasValueByte, portBase + offsetof(OptionalPort, hasValue), sizeof(hasValueByte));
    tuple.port.hasValue = hasValueByte != 0;
    return tuple;
}

OpaqueOriginIdentifier ExtractOpaque(const OriginSnapshot& snapshot) {
    OpaqueOriginIdentifier opaque{};
    std::memcpy(&opaque, snapshot.raw + offsetof(SecurityOriginData, storage), sizeof(opaque));
    return opaque;
}

bool EqualPorts(const PortSnapshot& a, const PortSnapshot& b) {
    return a.hasValue == b.hasValue && (!a.hasValue || a.value == b.value);
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

bool EqualTuples(const TupleSnapshot& a, const TupleSnapshot& b, const char* function) {
    return EqualGuestStrings(a.protocol, b.protocol, function)
        && EqualGuestStrings(a.host, b.host, function)
        && EqualPorts(a.port, b.port);
}

bool EqualIdentifiers(const OpaqueOriginIdentifier& a, const OpaqueOriginIdentifier& b) {
    return a.object == b.object && a.processIdentifier == b.processIdentifier;
}

}  // namespace

extern "C" {

bool APS5_VABI _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(const SecurityOriginData* a, const SecurityOriginData* b) {
    if (!a || !b) throw std::invalid_argument(__func__);
    const OriginSnapshot snapshotA = ReadOriginSnapshot(a, __func__);
    if (a == b) return true;
    const OriginSnapshot snapshotB = ReadOriginSnapshot(b, __func__);
    const std::uint32_t index = Index(snapshotA);
    if (index != Index(snapshotB)) return false;
    if (index == Valueless) return true;
    if (index == NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex)
        return EqualTuples(ExtractTuple(snapshotA), ExtractTuple(snapshotB), __func__);
    return EqualIdentifiers(ExtractOpaque(snapshotA), ExtractOpaque(snapshotB));
}

}

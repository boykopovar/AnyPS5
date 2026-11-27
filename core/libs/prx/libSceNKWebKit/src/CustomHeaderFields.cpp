// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/loader/CustomHeaderFields.cpp@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

// Derived from neonmodder123/PS5-WebKit-11.x/Source/WebCore/platform/RegistrableDomain.h@80f77e94c63ee7202eec3d4b5b1a021dd89011ba
/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/CustomHeaderFieldsExports.hpp"
#include "prx/libSceNKWebKit/include/PublicSuffixExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <new>
#include <string>
#include <string_view>
#include <stdexcept>

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <type_traits>
#include <vector>

namespace {

using NKWebKit::WebCoreAbi::CustomHeaderFields;
using NKWebKit::WebCoreAbi::HTTPHeaderField;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

constexpr std::string_view kNullOrigin = "nullOrigin";
constexpr unsigned kStringImplFlag8Bit = 4u;
constexpr unsigned kStringImplBufferOwnershipMask = 3u;
constexpr unsigned kStringImplBufferSubstring = 1u;
constexpr std::size_t kSubstringHeaderBytes = 24u + sizeof(StringImpl*);

struct RawVectorHeader {
    void* buffer{nullptr};
    unsigned capacity{0};
    unsigned size{0};
};

struct RawCustomHeaderFields {
    RawVectorHeader fields;
    RawVectorHeader thirdPartyDomains;
};

struct RawHeaderField {
    StringImpl* name{nullptr};
    StringImpl* value{nullptr};
};

static_assert(std::is_trivially_copyable_v<RawCustomHeaderFields>);
static_assert(sizeof(RawCustomHeaderFields) == sizeof(CustomHeaderFields));
static_assert(std::is_trivially_copyable_v<RawHeaderField>);
static_assert(sizeof(RawHeaderField) == sizeof(HTTPHeaderField));
static_assert(std::is_trivially_copyable_v<StringImpl>);
static_assert(std::is_trivially_copyable_v<URL>);

void RequireReadableGuestBytes(const void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, size);
    if (!GuestReadable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritableGuestBytes(void* pointer, std::size_t size, const char* function) {
    if (!size) return;
    if (!pointer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const GuestArena::HostWrite guard(pointer, size);
    if (!guard.Open() || !GuestWritable_nid_no_patch(pointer, size))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

template <typename T>
T ReadGuestValue(const T* pointer, const char* function) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

RawCustomHeaderFields ReadRawFields(const CustomHeaderFields* pointer, const char* function) {
    RawCustomHeaderFields value{};
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(value)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void ValidateRefCountableString(StringImpl* impl, const char* function) {
    if (!impl) return;
    RequireReadableGuestBytes(impl, sizeof(StringImpl), function);
    RequireWritableGuestBytes(impl, sizeof(StringImpl), function);
}

void ValidateReleasableString(StringImpl* impl, const char* function, unsigned depth = 0) {
    if (!impl) return;
    if (depth >= 64u) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const StringImpl header = ReadGuestValue(impl, function);
    RequireWritableGuestBytes(impl, sizeof(StringImpl), function);
    if ((header.refCount & ~1u) == 2u && (header.hashAndFlags & kStringImplBufferOwnershipMask) == kStringImplBufferSubstring) {
        RequireReadableGuestBytes(impl, kSubstringHeaderBytes, function);
        StringImpl* owner = nullptr;
        if (!GuestCopyIn_nid_no_patch(&owner, reinterpret_cast<const unsigned char*>(impl) + 24, sizeof(owner)))
            NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
        if (owner) ValidateReleasableString(owner, function, depth + 1u);
    }
}

void RetainGuestString(StringImpl* impl, const char* function) {
    if (!impl) return;
    unsigned refCount = 0;
    if (!GuestCopyIn_nid_no_patch(&refCount, reinterpret_cast<const unsigned char*>(impl) + offsetof(StringImpl, refCount), sizeof(refCount)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    refCount += 2u;
    if (!GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(impl) + offsetof(StringImpl, refCount), &refCount, sizeof(refCount)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void UndoRetainGuestString(StringImpl* impl) {
    if (!impl) return;
    unsigned refCount = 0;
    if (!GuestCopyIn_nid_no_patch(&refCount, reinterpret_cast<const unsigned char*>(impl) + offsetof(StringImpl, refCount), sizeof(refCount)))
        return;
    if (refCount >= 2u) {
        refCount -= 2u;
        static_cast<void>(GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(impl) + offsetof(StringImpl, refCount), &refCount, sizeof(refCount)));
    }
}

template <typename T>
std::vector<T> ReadGuestVectorElements(const RawVectorHeader& header, const char* function) {
    std::vector<T> elements;
    if (!header.size) return elements;
    if (!header.buffer) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    if (header.size > std::numeric_limits<std::size_t>::max() / sizeof(T))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const std::size_t totalBytes = static_cast<std::size_t>(header.size) * sizeof(T);
    const auto baseAddress = reinterpret_cast<std::uintptr_t>(header.buffer);
    if (totalBytes > std::numeric_limits<std::uintptr_t>::max() - baseAddress)
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    RequireReadableGuestBytes(header.buffer, totalBytes, function);
    elements.resize(header.size);
    if (!GuestCopyIn_nid_no_patch(elements.data(), header.buffer, totalBytes))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return elements;
}

struct BuiltCopyGuard {
    std::vector<StringImpl*> retained;
    void* fieldsBuffer{nullptr};
    void* domainsBuffer{nullptr};
    bool armed{true};

    ~BuiltCopyGuard() {
        if (!armed) return;
        for (auto it = retained.rbegin(); it != retained.rend(); ++it) UndoRetainGuestString(*it);
        if (domainsBuffer) _ZN3WTF8fastFreeEPv(domainsBuffer);
        if (fieldsBuffer) _ZN3WTF8fastFreeEPv(fieldsBuffer);
    }

    BuiltCopyGuard() = default;
    BuiltCopyGuard(const BuiltCopyGuard&) = delete;
    BuiltCopyGuard& operator=(const BuiltCopyGuard&) = delete;
};

RawCustomHeaderFields BuildClonedFields(const RawCustomHeaderFields& sourceRaw, BuiltCopyGuard& guard, const char* function) {
    const auto sourceFields = ReadGuestVectorElements<RawHeaderField>(sourceRaw.fields, function);
    const auto sourceDomains = ReadGuestVectorElements<StringImpl*>(sourceRaw.thirdPartyDomains, function);
    for (const auto& entry : sourceFields) {
        ValidateRefCountableString(entry.name, function);
        ValidateRefCountableString(entry.value, function);
    }
    for (StringImpl* domain : sourceDomains) {
        ValidateRefCountableString(domain, function);
    }

    RawCustomHeaderFields built{};
    if (!sourceFields.empty()) {
        guard.fieldsBuffer = _ZN3WTF10fastMallocEm(sourceFields.size() * sizeof(RawHeaderField));
        built.fields.buffer = guard.fieldsBuffer;
        built.fields.capacity = static_cast<unsigned>(sourceFields.size());
        built.fields.size = static_cast<unsigned>(sourceFields.size());
    }
    if (!sourceDomains.empty()) {
        guard.domainsBuffer = _ZN3WTF10fastMallocEm(sourceDomains.size() * sizeof(StringImpl*));
        built.thirdPartyDomains.buffer = guard.domainsBuffer;
        built.thirdPartyDomains.capacity = static_cast<unsigned>(sourceDomains.size());
        built.thirdPartyDomains.size = static_cast<unsigned>(sourceDomains.size());
    }

    guard.retained.reserve(sourceFields.size() * 2 + sourceDomains.size());
    for (const auto& entry : sourceFields) {
        if (entry.name) {
            RetainGuestString(entry.name, function);
            guard.retained.push_back(entry.name);
        }
        if (entry.value) {
            RetainGuestString(entry.value, function);
            guard.retained.push_back(entry.value);
        }
    }
    for (StringImpl* domain : sourceDomains) {
        if (domain) {
            RetainGuestString(domain, function);
            guard.retained.push_back(domain);
        }
    }

    if (guard.fieldsBuffer) std::memcpy(guard.fieldsBuffer, sourceFields.data(), sourceFields.size() * sizeof(RawHeaderField));
    if (guard.domainsBuffer) std::memcpy(guard.domainsBuffer, sourceDomains.data(), sourceDomains.size() * sizeof(StringImpl*));
    return built;
}

void ConstructEmpty(CustomHeaderFields* self, const char* function) {
    if (!self) throw std::invalid_argument("WebCore::CustomHeaderFields::CustomHeaderFields: null this");
    RequireWritableGuestBytes(self, sizeof(CustomHeaderFields), function);
    const RawCustomHeaderFields empty{};
    if (!GuestCopyOut_nid_no_patch(self, &empty, sizeof(empty)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void ConstructCopy(CustomHeaderFields* self, const CustomHeaderFields* other, const char* function) {
    if (!other) throw std::invalid_argument("WebCore::CustomHeaderFields::CustomHeaderFields: null source");
    if (!self) throw std::invalid_argument("WebCore::CustomHeaderFields::CustomHeaderFields: null this");
    RequireWritableGuestBytes(self, sizeof(CustomHeaderFields), function);
    const RawCustomHeaderFields sourceRaw = ReadRawFields(other, function);
    BuiltCopyGuard guard;
    const RawCustomHeaderFields built = BuildClonedFields(sourceRaw, guard, function);
    if (!GuestCopyOut_nid_no_patch(self, &built, sizeof(built)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    guard.armed = false;
}

void ReleaseRawFields(const RawCustomHeaderFields& raw, const std::vector<RawHeaderField>& fields, const std::vector<StringImpl*>& domains) {
    for (const auto& entry : fields) {
        NKWebKit::WtfAbi::Release(entry.value);
        NKWebKit::WtfAbi::Release(entry.name);
    }
    if (raw.fields.buffer) _ZN3WTF8fastFreeEPv(raw.fields.buffer);
    for (StringImpl* domain : domains) {
        NKWebKit::WtfAbi::Release(domain);
    }
    if (raw.thirdPartyDomains.buffer) _ZN3WTF8fastFreeEPv(raw.thirdPartyDomains.buffer);
}

String CopyGuestString(const StringImpl* guestImpl, const char* function) {
    if (!guestImpl) return {};
    const StringImpl header = ReadGuestValue(guestImpl, function);
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

String ToString(const StringView& view) {
    if (view.is8Bit) return _ZN3WTF10StringImpl6createEPKhj(static_cast<const unsigned char*>(view.data), view.length);
    return _ZN3WTF10StringImpl6createEPKDsj(static_cast<const char16_t*>(view.data), view.length);
}

unsigned RegistrableDomainLength(const StringView& host) {
    if (!host.length) return kNullOrigin.size();
    const auto hostString = ToString(host);
    const auto domain = _ZN7WebCore28topPrivatelyControlledDomainERKN3WTF6StringE(&hostString);
    return domain.impl && domain.impl->length ? domain.impl->length : host.length;
}

bool MatchesAt(const StringView& view, unsigned offset, const StringView& part) {
    for (unsigned i = 0; i < part.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, offset + i) != NKWebKit::WtfAbi::CodeUnitAt(part, i)) return false;
    }
    return true;
}

bool Equal(const StringView& a, const StringView& b) {
    return a.length == b.length && MatchesAt(a, 0, b);
}

bool EndsWith(const StringView& view, const StringView& suffix) {
    return suffix.length <= view.length && MatchesAt(view, view.length - suffix.length, suffix);
}

bool StartsWithWildcard(const StringView& view) {
    return view.length >= 2 && NKWebKit::WtfAbi::CodeUnitAt(view, 0) == u'*' && NKWebKit::WtfAbi::CodeUnitAt(view, 1) == u'.';
}

StringView DropFirstCharacter(const StringView& view) {
    const auto* characters = static_cast<const unsigned char*>(view.data) + (view.is8Bit ? 1 : 2);
    return {characters, view.length - 1, view.is8Bit};
}

CustomHeaderFields& AssignFields(CustomHeaderFields* self, const CustomHeaderFields* other, const char* function) {
    if (!self) throw std::invalid_argument("WebCore::CustomHeaderFields::operator=: null this");
    if (!other) throw std::invalid_argument("WebCore::CustomHeaderFields::operator=: null source");
    RequireReadableGuestBytes(self, sizeof(CustomHeaderFields), function);
    RequireWritableGuestBytes(self, sizeof(CustomHeaderFields), function);
    const RawCustomHeaderFields targetRaw = ReadRawFields(self, function);
    if (self == other) return *self;

    const RawCustomHeaderFields sourceRaw = ReadRawFields(other, function);
    const auto oldFields = ReadGuestVectorElements<RawHeaderField>(targetRaw.fields, function);
    const auto oldDomains = ReadGuestVectorElements<StringImpl*>(targetRaw.thirdPartyDomains, function);
    for (const auto& entry : oldFields) {
        ValidateReleasableString(entry.name, function);
        ValidateReleasableString(entry.value, function);
    }
    for (StringImpl* domain : oldDomains) {
        ValidateReleasableString(domain, function);
    }

    BuiltCopyGuard guard;
    const RawCustomHeaderFields built = BuildClonedFields(sourceRaw, guard, function);
    if (!GuestCopyOut_nid_no_patch(self, &built, sizeof(built)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    guard.armed = false;
    ReleaseRawFields(targetRaw, oldFields, oldDomains);
    return *self;
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC1Ev(CustomHeaderFields* self) {
    ConstructEmpty(self, __func__);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC2Ev(CustomHeaderFields* self) {
    ConstructEmpty(self, __func__);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC1ERKS0_(CustomHeaderFields* self, const CustomHeaderFields* other) {
    ConstructCopy(self, other, __func__);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC2ERKS0_(CustomHeaderFields* self, const CustomHeaderFields* other) {
    ConstructCopy(self, other, __func__);
}

CustomHeaderFields& APS5_VABI _ZN7WebCore18CustomHeaderFieldsaSERKS0_(CustomHeaderFields* self, const CustomHeaderFields* other) {
    return AssignFields(self, other, __func__);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsD2Ev(CustomHeaderFields* self) {
    if (!self) throw std::invalid_argument("WebCore::CustomHeaderFields::~CustomHeaderFields: null this");
    RequireReadableGuestBytes(self, sizeof(CustomHeaderFields), __func__);
    RequireWritableGuestBytes(self, sizeof(CustomHeaderFields), __func__);
    const RawCustomHeaderFields raw = ReadRawFields(self, __func__);
    const auto fields = ReadGuestVectorElements<RawHeaderField>(raw.fields, __func__);
    const auto domains = ReadGuestVectorElements<StringImpl*>(raw.thirdPartyDomains, __func__);
    for (const auto& entry : fields) {
        ValidateReleasableString(entry.name, __func__);
        ValidateReleasableString(entry.value, __func__);
    }
    for (StringImpl* domain : domains) {
        ValidateReleasableString(domain, __func__);
    }
    ReleaseRawFields(raw, fields, domains);
}

bool APS5_VABI _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(const CustomHeaderFields* self, const URL* url) {
    if (!self) throw std::invalid_argument("WebCore::CustomHeaderFields::thirdPartyDomainsMatch: null this");
    if (!url) throw std::invalid_argument("WebCore::CustomHeaderFields::thirdPartyDomainsMatch: null url");
    const RawCustomHeaderFields raw = ReadRawFields(self, __func__);
    URL localUrl = ReadGuestValue(url, __func__);
    if (!raw.thirdPartyDomains.size) return false;
    const String copiedUrlString = CopyGuestString(localUrl.impl, __func__);
    localUrl.impl = copiedUrlString.impl;
    const auto domainPointers = ReadGuestVectorElements<StringImpl*>(raw.thirdPartyDomains, __func__);
    const auto host = _ZNK3WTF3URL4hostEv(&localUrl);
    const auto registrableDomainLength = RegistrableDomainLength(host);
    for (const StringImpl* domainImpl : domainPointers) {
        const String copiedPattern = CopyGuestString(domainImpl, __func__);
        const auto pattern = NKWebKit::WtfAbi::ViewOf(copiedPattern.impl);
        if (Equal(pattern, host)) return true;
        if (StartsWithWildcard(pattern) && EndsWith(host, DropFirstCharacter(pattern)) && pattern.length > registrableDomainLength) return true;
    }
    return false;
}

}

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

namespace {

using NKWebKit::WebCoreAbi::CustomHeaderFields;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;

constexpr std::string_view kNullOrigin = "nullOrigin";

CustomHeaderFields* Checked(CustomHeaderFields* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::CustomHeaderFields::") + operation + ": null this");
    return self;
}

const CustomHeaderFields& Source(const CustomHeaderFields* other, const char* operation) {
    if (!other) throw std::invalid_argument(std::string("WebCore::CustomHeaderFields::") + operation + ": null source");
    return *other;
}

void ConstructEmpty(CustomHeaderFields* self) {
    new (Checked(self, "CustomHeaderFields")) CustomHeaderFields();
}

void ConstructCopy(CustomHeaderFields* self, const CustomHeaderFields* other) {
    const auto& source = Source(other, "CustomHeaderFields");
    new (Checked(self, "CustomHeaderFields")) CustomHeaderFields(source);
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

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC1Ev(CustomHeaderFields* self) {
    ConstructEmpty(self);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC2Ev(CustomHeaderFields* self) {
    ConstructEmpty(self);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC1ERKS0_(CustomHeaderFields* self, const CustomHeaderFields* other) {
    ConstructCopy(self, other);
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsC2ERKS0_(CustomHeaderFields* self, const CustomHeaderFields* other) {
    ConstructCopy(self, other);
}

CustomHeaderFields& APS5_VABI _ZN7WebCore18CustomHeaderFieldsaSERKS0_(CustomHeaderFields* self, const CustomHeaderFields* other) {
    auto* fields = Checked(self, "operator=");
    const auto& source = Source(other, "operator=");
    if (fields != &source) {
        fields->fields = source.fields;
        fields->thirdPartyDomains = source.thirdPartyDomains;
    }
    return *fields;
}

void APS5_VABI _ZN7WebCore18CustomHeaderFieldsD2Ev(CustomHeaderFields* self) {
    Checked(self, "~CustomHeaderFields")->~CustomHeaderFields();
}

bool APS5_VABI _ZNK7WebCore18CustomHeaderFields22thirdPartyDomainsMatchERKN3WTF3URLE(const CustomHeaderFields* self, const URL* url) {
    if (!self) throw std::invalid_argument("WebCore::CustomHeaderFields::thirdPartyDomainsMatch: null this");
    if (!url) throw std::invalid_argument("WebCore::CustomHeaderFields::thirdPartyDomainsMatch: null url");
    const auto& fields = *self;
    if (!fields.thirdPartyDomains.size) return false;
    const auto host = _ZNK3WTF3URL4hostEv(url);
    const auto registrableDomainLength = RegistrableDomainLength(host);
    for (const auto& domainOrPattern : fields.thirdPartyDomains) {
        const auto pattern = NKWebKit::WtfAbi::ViewOf(domainOrPattern.impl);
        if (Equal(pattern, host)) return true;
        if (StartsWithWildcard(pattern) && EndsWith(host, DropFirstCharacter(pattern)) && pattern.length > registrableDomainLength) return true;
    }
    return false;
}

}

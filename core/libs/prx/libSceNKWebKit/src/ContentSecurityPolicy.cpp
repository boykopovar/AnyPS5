// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/page/csp/ContentSecurityPolicy.cpp
/*
 * Copyright (C) 2011 Google, Inc. All rights reserved.
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

// Derived from https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/URL.cpp
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
#include "prx/libSceNKWebKit/include/ContentSecurityPolicyExports.hpp"
#include "prx/libSceNKWebKit/include/SecurityOriginExports.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/ContentSecurityPolicyAbi.hpp"
#include "prx/libSceNKWebKit/src/ContentSecurityPolicyParser.hpp"
#include "prx/libSceNKWebKit/src/LegacyResourceRequestAbi.hpp"
#include "prx/libSceNKWebKit/src/SecurityOriginAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using NKWebKit::WebCoreAbi::ContentSecurityPolicy;
using NKWebKit::WebCoreAbi::ContentSecurityPolicyClient;
using NKWebKit::WebCoreAbi::LegacyContentSecurityPolicyHeader;
using NKWebKit::WebCoreAbi::LegacyContentSecurityPolicyResponseHeaders;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::LegacySecurityOriginData;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;
namespace Csp = NKWebKit::Csp;

template <typename T>
T& Checked(T* pointer, const char* what) {
    if (!pointer) throw std::invalid_argument(what);
    return *pointer;
}

std::u16string Units(const StringView& view) {
    std::u16string text;
    text.reserve(view.length);
    for (unsigned index = 0; index < view.length; ++index) text.push_back(NKWebKit::WtfAbi::CodeUnitAt(view, index));
    return text;
}

std::u16string Units(const StringImpl* impl) {
    return impl ? Units(NKWebKit::WtfAbi::ViewOf(impl)) : std::u16string();
}

String MakeString(const std::u16string& text) {
    std::string narrow;
    narrow.reserve(text.size());
    for (const char16_t unit : text) {
        if (unit > 0xFF) return _ZN3WTF10StringImpl6createEPKDsj(text.data(), static_cast<unsigned>(text.size()));
        narrow.push_back(static_cast<char>(unit));
    }
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(narrow.data()), static_cast<unsigned>(narrow.size()));
}

std::u16string LowerASCII(std::u16string text) {
    for (char16_t& unit : text) {
        if (unit >= u'A' && unit <= u'Z') unit = static_cast<char16_t>(unit | 0x20);
    }
    return text;
}

bool ProtocolIs(const URL& url, std::string_view protocol) {
    if (!(url.bits & NKWebKit::WebCoreAbi::kURLValidBit)) return false;
    const std::u16string scheme = Units(_ZNK3WTF3URL8protocolEv(&url));
    if (scheme.size() != protocol.size()) return false;
    for (std::size_t index = 0; index < scheme.size(); ++index) {
        if ((scheme[index] | 0x20) != static_cast<unsigned char>(protocol[index])) return false;
    }
    return true;
}

LegacyContentSecurityPolicyResponseHeaders* CachedHeaders(ContentSecurityPolicy& policy) {
    return std::launder(reinterpret_cast<LegacyContentSecurityPolicyResponseHeaders*>(policy.cachedResponseHeaders));
}

void ResetCachedHeaders(ContentSecurityPolicy& policy) {
    if (!policy.hasCachedResponseHeaders) return;
    CachedHeaders(policy)->~LegacyContentSecurityPolicyResponseHeaders();
    policy.hasCachedResponseHeaders = false;
}

class Host final : public Csp::PolicyHost {
public:
    explicit Host(ContentSecurityPolicy& policy) : policy(policy) {}

    void LogToConsole(const std::u16string& message) override {
        if (!policy.isReportingEnabled) return;
        if (!policy.client) return;
        const String text = MakeString(message);
        policy.client->vtable->addConsoleMessage(policy.client, NKWebKit::WebCoreAbi::kMessageSourceSecurity, NKWebKit::WebCoreAbi::kMessageLevelError, &text, 0);
    }

    void EnforceSandboxFlags(int flags) override { policy.sandboxFlags |= flags; }
    void AddHashAlgorithmsForInlineScripts(unsigned algorithms) override { policy.hashAlgorithmsForInlineScripts |= algorithms; }
    void AddHashAlgorithmsForInlineStylesheets(unsigned algorithms) override { policy.hashAlgorithmsForInlineStylesheets |= algorithms; }
    void SetUpgradeInsecureRequests() override { policy.upgradeInsecureRequests = true; }

private:
    ContentSecurityPolicy& policy;
};

void UpdateSourceSelf(ContentSecurityPolicy& policy) {
    const auto origin = _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(&policy.protectedURL);
    const auto& data = origin.m_ptr->data;
    auto source = std::make_unique<Csp::Source>();
    if (data.index == NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex) {
        source->scheme = Units(data.storage.tuple.protocol.impl);
        source->host = Units(data.storage.tuple.host.impl);
        if (data.storage.tuple.port.hasValue) source->port = data.storage.tuple.port.value;
    }
    policy.selfSourceProtocol = MakeString(source->scheme);
    delete std::exchange(policy.selfSource, source.release());
}

void Construct(ContentSecurityPolicy* self, URL* protectedURL, ContentSecurityPolicyClient* client) {
    Checked(self, "null WebCore::ContentSecurityPolicy");
    Checked(protectedURL, "null WTF::URL");
    new (self) ContentSecurityPolicy{};
    self->client = client;
    self->protectedURL = *protectedURL;
    protectedURL->impl = nullptr;
    self->isReportingEnabled = true;
    UpdateSourceSelf(*self);
}

unsigned* NavigationSetMetadata(LegacySecurityOriginData* table) {
    return reinterpret_cast<unsigned*>(table) - 4;
}

unsigned NavigationSetSize(LegacySecurityOriginData* table) {
    return table ? NavigationSetMetadata(table)[3] : 0;
}

bool IsDeleted(const LegacySecurityOriginData& entry) {
    return reinterpret_cast<std::intptr_t>(entry.protocol) == -1;
}

bool IsEmptyEntry(const LegacySecurityOriginData& entry) {
    return !entry.protocol && !entry.host && !entry.hasPort;
}

void DestroyNavigationSet(LegacySecurityOriginData* table) {
    if (!table) return;
    const unsigned size = NavigationSetSize(table);
    for (unsigned index = 0; index < size; ++index) {
        if (IsDeleted(table[index])) continue;
        NKWebKit::WtfAbi::Release(table[index].protocol);
        NKWebKit::WtfAbi::Release(table[index].host);
    }
    _ZN3WTF8fastFreeEPv(NavigationSetMetadata(table));
}

bool NavigationSetContainsOriginOf(LegacySecurityOriginData* table, const URL& url) {
    if (!table) return false;
    const std::u16string protocol = LowerASCII(Units(_ZNK3WTF3URL8protocolEv(&url)));
    const std::u16string host = LowerASCII(Units(_ZNK3WTF3URL4hostEv(&url)));
    const std::optional<std::uint16_t> port = NKWebKit::WtfAbi::PortOf(url);
    const unsigned size = NavigationSetSize(table);
    for (unsigned index = 0; index < size; ++index) {
        const LegacySecurityOriginData& entry = table[index];
        if (IsDeleted(entry) || IsEmptyEntry(entry)) continue;
        const std::optional<std::uint16_t> entryPort = entry.hasPort ? std::optional<std::uint16_t>(entry.port) : std::nullopt;
        if (Units(entry.protocol) == protocol && Units(entry.host) == host && entryPort == port) return true;
    }
    return false;
}

void Destroy(ContentSecurityPolicy* self) {
    Checked(self, "null WebCore::ContentSecurityPolicy");
    for (Csp::DirectiveList* list : self->policies) delete list;
    delete self->selfSource;
    ResetCachedHeaders(*self);
    DestroyNavigationSet(self->insecureNavigationRequestsToUpgrade);
    NKWebKit::WtfAbi::Release(self->protectedURL.impl);
    self->~ContentSecurityPolicy();
}

void DidReceiveHeader(ContentSecurityPolicy& policy, const String& header, bool enforce) {
    if (policy.hasAPIPolicy) return;
    ResetCachedHeaders(policy);
    Host host(policy);
    const std::u16string text = Units(header.impl);
    const char16_t* position = text.data();
    const char16_t* const end = position + text.size();
    const char16_t* begin = position;
    while (position < end) {
        while (position < end && *position != u',') ++position;
        auto list = Csp::ParseDirectiveList(host, std::u16string(begin, static_cast<std::size_t>(position - begin)), enforce ? Csp::HeaderType::Enforce : Csp::HeaderType::Report);
        policy.policies.append(list.get());
        list.release();
        if (position < end && *position == u',') ++position;
        begin = position;
    }
}

class ReportingScope {
public:
    ReportingScope(ContentSecurityPolicy& policy, bool enabled) : policy(policy), previous(policy.isReportingEnabled) { policy.isReportingEnabled = enabled; }
    ReportingScope(const ReportingScope&) = delete;
    ~ReportingScope() { policy.isReportingEnabled = previous; }

private:
    ContentSecurityPolicy& policy;
    bool previous;
};

void SetPort(URL& url, std::uint16_t port) {
    if (!(url.bits & NKWebKit::WebCoreAbi::kURLValidBit)) return;
    const std::u16string text = Units(url.impl);
    const unsigned portLength = (url.bits >> NKWebKit::WebCoreAbi::kURLPortLengthShift) & NKWebKit::WebCoreAbi::kURLPortLengthMask;
    std::u16string rebuilt = text.substr(0, url.hostEnd);
    rebuilt.push_back(u':');
    for (const char digit : std::to_string(static_cast<unsigned>(port))) rebuilt.push_back(static_cast<char16_t>(digit));
    rebuilt += text.substr(url.hostEnd + portLength);
    const String relative = MakeString(rebuilt);
    const URL base{};
    URL parsed{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&parsed, &base, &relative, nullptr);
    NKWebKit::WtfAbi::Release(std::exchange(url.impl, nullptr));
    url = parsed;
}

void UpgradeInsecureRequestIfNeeded(const ContentSecurityPolicy& policy, URL& url, int requestType) {
    if (!ProtocolIs(url, "http") && !ProtocolIs(url, "ws")) return;
    bool upgradeRequest = NavigationSetContainsOriginOf(policy.insecureNavigationRequestsToUpgrade, url);
    if (requestType == NKWebKit::WebCoreAbi::kInsecureRequestTypeLoad || requestType == NKWebKit::WebCoreAbi::kInsecureRequestTypeFormSubmission) upgradeRequest |= policy.upgradeInsecureRequests;
    if (!upgradeRequest) return;
    static constexpr char https[] = "https";
    static constexpr char wss[] = "wss";
    if (ProtocolIs(url, "http"))
        _ZN3WTF3URL11setProtocolENS_10StringViewE(&url, StringView{https, 5, true});
    else
        _ZN3WTF3URL11setProtocolENS_10StringViewE(&url, StringView{wss, 3, true});
    const std::optional<std::uint16_t> port = NKWebKit::WtfAbi::PortOf(url);
    if (port && *port == 80) SetPort(url, 443);
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore21ContentSecurityPolicyC1EON3WTF3URLEPNS_27ContentSecurityPolicyClientE(ContentSecurityPolicy* self, URL* protectedURL, ContentSecurityPolicyClient* client) {
    Construct(self, protectedURL, client);
}

void APS5_VABI _ZN7WebCore21ContentSecurityPolicyC2EON3WTF3URLEPNS_27ContentSecurityPolicyClientE(ContentSecurityPolicy* self, URL* protectedURL, ContentSecurityPolicyClient* client) {
    Construct(self, protectedURL, client);
}

void APS5_VABI _ZN7WebCore21ContentSecurityPolicyD1Ev(ContentSecurityPolicy* self) {
    Destroy(self);
}

void APS5_VABI _ZN7WebCore21ContentSecurityPolicyD2Ev(ContentSecurityPolicy* self) {
    Destroy(self);
}

void APS5_VABI _ZN7WebCore21ContentSecurityPolicy17didReceiveHeadersERKNS_36ContentSecurityPolicyResponseHeadersEON3WTF6StringENS0_19ReportParsingErrorsE(ContentSecurityPolicy* self, const LegacyContentSecurityPolicyResponseHeaders* headers, String* referrer, bool reportParsingErrors) {
    ContentSecurityPolicy& policy = Checked(self, "null WebCore::ContentSecurityPolicy");
    const LegacyContentSecurityPolicyResponseHeaders& received = Checked(headers, "null WebCore::LegacyContentSecurityPolicyResponseHeaders");
    String& givenReferrer = Checked(referrer, "null WTF::String");
    const ReportingScope scope(policy, reportParsingErrors);
    for (const LegacyContentSecurityPolicyHeader& header : received.headers) DidReceiveHeader(policy, header.text, header.enforce);
    policy.referrer = std::move(givenReferrer);
    policy.httpStatusCode = received.httpStatusCode;
}

LegacyContentSecurityPolicyResponseHeaders APS5_VABI _ZNK7WebCore21ContentSecurityPolicy15responseHeadersEv(const ContentSecurityPolicy* self) {
    ContentSecurityPolicy& policy = const_cast<ContentSecurityPolicy&>(Checked(self, "null WebCore::ContentSecurityPolicy"));
    if (!policy.hasCachedResponseHeaders) {
        auto* cached = new (policy.cachedResponseHeaders) LegacyContentSecurityPolicyResponseHeaders{NKWebKit::WtfAbi::Vector<LegacyContentSecurityPolicyHeader>(policy.policies.size), 0, false};
        policy.hasCachedResponseHeaders = true;
        for (const Csp::DirectiveList* list : policy.policies) cached->headers.append(LegacyContentSecurityPolicyHeader{MakeString(list->header), list->headerType == Csp::HeaderType::Enforce});
        cached->httpStatusCode = policy.httpStatusCode;
    }
    return *CachedHeaders(policy);
}

bool APS5_VABI _ZNK7WebCore21ContentSecurityPolicy22overridesXFrameOptionsEv(const ContentSecurityPolicy* self) {
    const ContentSecurityPolicy& policy = Checked(self, "null WebCore::ContentSecurityPolicy");
    for (const Csp::DirectiveList* list : policy.policies) {
        if (!list->reportOnly && list->frameAncestors) return true;
    }
    return false;
}

void APS5_VABI _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERN3WTF3URLENS0_19InsecureRequestTypeE(const ContentSecurityPolicy* self, URL* url, int requestType) {
    UpgradeInsecureRequestIfNeeded(Checked(self, "null WebCore::ContentSecurityPolicy"), Checked(url, "null WTF::URL"), requestType);
}

void APS5_VABI _ZNK7WebCore21ContentSecurityPolicy30upgradeInsecureRequestIfNeededERNS_15ResourceRequestENS0_19InsecureRequestTypeE(const ContentSecurityPolicy* self, LegacyResourceRequestBase* request, int requestType) {
    const ContentSecurityPolicy& policy = Checked(self, "null WebCore::ContentSecurityPolicy");
    LegacyResourceRequestBase& target = Checked(request, "null WebCore::ResourceRequest");
    if (!target.resourceRequestUpdated) throw std::logic_error("WebCore::ResourceRequest::doUpdateResourceRequest is not implemented");
    URL url = target.url;
    if (url.impl) url.impl->refCount += 2;
    UpgradeInsecureRequestIfNeeded(policy, url, requestType);
    NKWebKit::WtfAbi::Release(std::exchange(target.url.impl, nullptr));
    target.url = url;
    target.platformRequestUpdated = false;
}

}

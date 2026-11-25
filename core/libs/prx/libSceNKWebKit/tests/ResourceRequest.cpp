#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/HttpHeaderMapAbi.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceRequestBase check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static StringImpl Make(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

static URL MakeURL(StringImpl* impl, unsigned bits) {
    return URL{impl, bits, 7, 7, 7, 8, 8, 9, 9};
}

static void SetOwnedURL(URL& field, StringImpl& impl, unsigned bits) {
    impl.refCount += 2;
    field = MakeURL(&impl, bits);
}

static void SetOwnedString(NKWebKit::WtfAbi::String& field, StringImpl& impl) {
    impl.refCount += 2;
    field.impl = &impl;
}

static NKWebKit::WebCoreAbi::FormData* CreateForm() {
    auto form = _ZN7WebCore8FormData6createEv();
    return form.m_ptr;
}

static void TestURL() {
    ResourceRequestBase request = ResourceRequestBase();
    request.resourceRequestUpdated = true;
    Require(_ZNK7WebCore19ResourceRequestBase6isNullEv(&request));
    Require(_ZNK7WebCore19ResourceRequestBase7isEmptyEv(&request));

    auto first = Make("http://a/");
    auto second = Make("https://b/");
    auto emptyText = Make("");
    URL a = MakeURL(&first, 1 | (4 << 6));
    URL b = MakeURL(&second, 1 | (5 << 6));
    URL empty = MakeURL(&emptyText, 0);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase6setURLERKN3WTF3URLE(&request, &a);
    Require(!request.platformRequestUpdated);
    Require(first.refCount == 4);
    Require(request.url.impl == &first && request.url.bits == a.bits && request.url.queryEnd == a.queryEnd);
    Require(&_ZNK7WebCore19ResourceRequestBase3urlEv(&request) == &request.url);
    Require(!_ZNK7WebCore19ResourceRequestBase6isNullEv(&request));
    Require(!_ZNK7WebCore19ResourceRequestBase7isEmptyEv(&request));

    _ZN7WebCore19ResourceRequestBase6setURLERKN3WTF3URLE(&request, &b);
    Require(first.refCount == 2 && second.refCount == 4);
    Require(request.url.impl == &second && request.url.bits == b.bits);

    _ZN7WebCore19ResourceRequestBase6setURLERKN3WTF3URLE(&request, &request.url);
    Require(second.refCount == 4);

    _ZN7WebCore19ResourceRequestBase6setURLERKN3WTF3URLE(&request, &empty);
    Require(second.refCount == 2 && emptyText.refCount == 4);
    Require(!_ZNK7WebCore19ResourceRequestBase6isNullEv(&request));
    Require(_ZNK7WebCore19ResourceRequestBase7isEmptyEv(&request));
}

static void TestFirstParty() {
    ResourceRequestBase request = ResourceRequestBase();
    request.resourceRequestUpdated = true;
    auto first = Make("http://a/");
    auto same = Make("http://a/");
    auto other = Make("http://c/");
    URL a = MakeURL(&first, 1);
    URL aCopy = MakeURL(&same, 1);
    URL c = MakeURL(&other, 1);

    _ZN7WebCore19ResourceRequestBase23setFirstPartyForCookiesERKN3WTF3URLE(&request, &a);
    Require(first.refCount == 4);
    Require(&_ZNK7WebCore19ResourceRequestBase20firstPartyForCookiesEv(&request) == &request.firstPartyForCookies);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase23setFirstPartyForCookiesERKN3WTF3URLE(&request, &aCopy);
    Require(request.platformRequestUpdated);
    Require(request.firstPartyForCookies.impl == &first && same.refCount == 2);

    _ZN7WebCore19ResourceRequestBase23setFirstPartyForCookiesERKN3WTF3URLE(&request, &c);
    Require(!request.platformRequestUpdated);
    Require(first.refCount == 2 && other.refCount == 4);
    Require(request.url.impl == nullptr);
}

static void TestMethod() {
    ResourceRequestBase request = ResourceRequestBase();
    request.resourceRequestUpdated = true;
    auto get = Make("GET");
    auto post = Make("POST");
    auto postCopy = Make("POST");
    get.refCount = 4;
    request.httpMethod.impl = &get;

    NKWebKit::WtfAbi::String postString(&post);
    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(&request, &postString);
    Require(!request.platformRequestUpdated);
    Require(request.httpMethod.impl == &post && post.refCount == 4 && get.refCount == 2);
    Require(&_ZNK7WebCore19ResourceRequestBase10httpMethodEv(&request) == &request.httpMethod);

    NKWebKit::WtfAbi::String postCopyString(&postCopy);
    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase13setHTTPMethodERKN3WTF6StringE(&request, &postCopyString);
    Require(request.platformRequestUpdated);
    Require(request.httpMethod.impl == &post && post.refCount == 4 && postCopy.refCount == 2);
    postString.impl = nullptr;
    postCopyString.impl = nullptr;
    request.httpMethod.impl = nullptr;
}

static void TestScalars() {
    ResourceRequestBase request = ResourceRequestBase();
    request.resourceRequestUpdated = true;
    request.timeoutInterval = 12.5;
    Require(_ZNK7WebCore19ResourceRequestBase15timeoutIntervalEv(&request) == 12.5);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase14setCachePolicyENS_26ResourceRequestCachePolicyE(&request, 0);
    Require(request.platformRequestUpdated);
    for (std::uint8_t policy = 1; policy <= 5; ++policy) {
        request.platformRequestUpdated = true;
        _ZN7WebCore19ResourceRequestBase14setCachePolicyENS_26ResourceRequestCachePolicyE(&request, policy);
        Require(!request.platformRequestUpdated);
        Require(_ZNK7WebCore19ResourceRequestBase11cachePolicyEv(&request) == policy);
    }

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase11setPriorityENS_20ResourceLoadPriorityE(&request, 0);
    Require(request.platformRequestUpdated);
    for (std::uint8_t priority = 1; priority <= 4; ++priority) {
        _ZN7WebCore19ResourceRequestBase11setPriorityENS_20ResourceLoadPriorityE(&request, priority);
        Require(_ZNK7WebCore19ResourceRequestBase8priorityEv(&request) == priority);
    }

    Require(!_ZNK7WebCore19ResourceRequestBase10isSameSiteEv(&request));
    _ZN7WebCore19ResourceRequestBase13setIsSameSiteEb(&request, true);
    Require(_ZNK7WebCore19ResourceRequestBase10isSameSiteEv(&request));
    Require(request.sameSiteDisposition == NKWebKit::WebCoreAbi::SameSite);
    _ZN7WebCore19ResourceRequestBase13setIsSameSiteEb(&request, false);
    Require(!_ZNK7WebCore19ResourceRequestBase10isSameSiteEv(&request));
    Require(request.sameSiteDisposition == NKWebKit::WebCoreAbi::CrossSite);

    request.allowCookies = true;
    request.isAppInitiated = true;
    Require(!_ZNK7WebCore19ResourceRequestBase9isTopSiteEv(&request));
    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase12setIsTopSiteEb(&request, true);
    Require(!request.platformRequestUpdated);
    Require(_ZNK7WebCore19ResourceRequestBase9isTopSiteEv(&request));
    Require(request.allowCookies && request.isAppInitiated);
    _ZN7WebCore19ResourceRequestBase12setIsTopSiteEb(&request, false);
    Require(!_ZNK7WebCore19ResourceRequestBase9isTopSiteEv(&request));
    Require(request.allowCookies && request.isAppInitiated);
}

static void TestUpdateFromDelegate() {
    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    request.resourceRequestBodyUpdated = true;
    request.platformRequestUpdated = true;
    request.platformRequestBodyUpdated = true;
    auto oldURLText = Make("http://old/");
    auto oldPartyText = Make("http://old-party/");
    auto oldMethodText = Make("OLD");
    auto oldInitiatorText = Make("old-initiator");
    auto oldPartitionText = Make("old-partition");
    auto oldHeaderText = Make("old-header");
    auto oldFallbackText = Make("old-fallback");
    SetOwnedURL(request.url, oldURLText, 1);
    SetOwnedURL(request.firstPartyForCookies, oldPartyText, 1);
    SetOwnedString(request.httpMethod, oldMethodText);
    SetOwnedString(request.initiatorIdentifier, oldInitiatorText);
    SetOwnedString(request.cachePartition, oldPartitionText);
    request.timeoutInterval = 2.5;
    request.cachePolicy = 1;
    request.sameSiteDisposition = NKWebKit::WebCoreAbi::CrossSite;
    request.priority = 3;
    request.requester = 6;
    request.allowCookies = true;
    request.isAppInitiated = true;
    request.isTopSite = false;
    request.privacyProxyFailClosedForUnreachableNonMainHosts = false;
    request.useAdvancedPrivacyProtections = false;
    request.hiddenFromInspector = true;
    request.inspectorInitiatorNodeIdentifier = 42;
    request.httpBody = CreateForm();
    NKWebKit::WtfAbi::String oldHeader(&oldHeaderText);
    NKWebKit::WtfAbi::String oldFallback(&oldFallbackText);
    NKWebKit::WtfAbi::String noFallback;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(
        &request, NKWebKit::HttpAbi::HTTPHeaderName::UserAgent, &oldHeader);
    _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(
        &request, &oldFallback, &noFallback, &noFallback);

    ResourceRequestBase delegate{};
    delegate.resourceRequestUpdated = true;
    delegate.resourceRequestBodyUpdated = true;
    delegate.platformRequestUpdated = false;
    delegate.platformRequestBodyUpdated = true;
    auto newURLText = Make("https://new/");
    auto newPartyText = Make("https://new-party/");
    auto newMethodText = Make("NEW");
    auto newInitiatorText = Make("new-initiator");
    auto newPartitionText = Make("new-partition");
    auto newHeaderText = Make("new-header");
    auto newFallbackText = Make("new-fallback");
    SetOwnedURL(delegate.url, newURLText, 1 | (5 << 6));
    SetOwnedURL(delegate.firstPartyForCookies, newPartyText, 1 | (4 << 6));
    SetOwnedString(delegate.httpMethod, newMethodText);
    SetOwnedString(delegate.initiatorIdentifier, newInitiatorText);
    SetOwnedString(delegate.cachePartition, newPartitionText);
    delegate.timeoutInterval = 9.5;
    delegate.cachePolicy = 4;
    delegate.sameSiteDisposition = NKWebKit::WebCoreAbi::SameSite;
    delegate.priority = 1;
    delegate.requester = 2;
    delegate.allowCookies = false;
    delegate.isAppInitiated = false;
    delegate.isTopSite = true;
    delegate.privacyProxyFailClosedForUnreachableNonMainHosts = true;
    delegate.useAdvancedPrivacyProtections = true;
    delegate.hiddenFromInspector = false;
    delegate.httpBody = CreateForm();
    NKWebKit::WtfAbi::String newHeader(&newHeaderText);
    NKWebKit::WtfAbi::String newFallback(&newFallbackText);
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(
        &delegate, NKWebKit::HttpAbi::HTTPHeaderName::UserAgent, &newHeader);
    _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(
        &delegate, &newFallback, &noFallback, &noFallback);

    auto* oldBody = static_cast<NKWebKit::WebCoreAbi::FormData*>(request.httpBody);
    auto* newBody = static_cast<NKWebKit::WebCoreAbi::FormData*>(delegate.httpBody);
    request.platformRequestUpdated = true;
    _ZN7WebCore15ResourceRequest41updateFromDelegatePreservingOldPropertiesERKS0_(&request, &delegate);

    Require(request.url.impl == &newURLText && request.url.bits == (1 | (5 << 6)));
    Require(request.firstPartyForCookies.impl == &newPartyText);
    Require(request.httpMethod.impl == &newMethodText);
    Require(request.timeoutInterval == 9.5);
    Require(request.cachePolicy == 4);
    Require(request.sameSiteDisposition == NKWebKit::WebCoreAbi::SameSite);
    Require(request.isTopSite);
    Require(!request.allowCookies);
    Require(!request.isAppInitiated);
    Require(request.privacyProxyFailClosedForUnreachableNonMainHosts);
    Require(request.useAdvancedPrivacyProtections);
    Require(request.cachePartition.impl == &newPartitionText);
    Require(request.priority == 3);
    Require(request.requester == 6);
    Require(request.hiddenFromInspector);
    Require(request.initiatorIdentifier.impl == &oldInitiatorText);
    Require(request.inspectorInitiatorNodeIdentifier && *request.inspectorInitiatorNodeIdentifier == 42);
    Require(request.httpBody == oldBody);
    Require(oldBody->refCount == 1 && newBody->refCount == 1);

    const auto& headers = *reinterpret_cast<const NKWebKit::HttpAbi::HTTPHeaderMap*>(&request.httpHeaderFields);
    Require(headers.common.size == 1);
    Require(headers.common.buffer[0].key == NKWebKit::HttpAbi::HTTPHeaderName::UserAgent);
    Require(headers.common.buffer[0].value.impl == &newHeaderText);
    Require(headers.uncommon.size == 0);
    const auto& fallback = request.responseContentDispositionEncodingFallbackArray;
    Require(fallback.size == 1 && static_cast<NKWebKit::WtfAbi::String*>(fallback.buffer)[0].impl == &newFallbackText);

    Require(oldURLText.refCount == 2 && newURLText.refCount == 6);
    Require(oldPartyText.refCount == 2 && newPartyText.refCount == 6);
    Require(oldMethodText.refCount == 2 && newMethodText.refCount == 6);
    Require(oldInitiatorText.refCount == 4 && newInitiatorText.refCount == 4);
    Require(oldPartitionText.refCount == 2 && newPartitionText.refCount == 6);
    Require(oldHeaderText.refCount == 2 && newHeaderText.refCount == 6);
    Require(oldFallbackText.refCount == 2 && newFallbackText.refCount == 6);

    Require(request.resourceRequestUpdated);
    Require(!request.platformRequestUpdated);
    Require(request.resourceRequestBodyUpdated);
    Require(!request.platformRequestBodyUpdated);

    oldHeader.impl = nullptr;
    oldFallback.impl = nullptr;
    newHeader.impl = nullptr;
    newFallback.impl = nullptr;
}

static void TestUpdateFromDelegateInspectorFallback() {
    ResourceRequestBase request{};
    request.resourceRequestUpdated = true;
    request.resourceRequestBodyUpdated = true;
    request.priority = 2;
    request.requester = 1;
    ResourceRequestBase delegate{};
    delegate.resourceRequestUpdated = true;
    delegate.resourceRequestBodyUpdated = true;
    delegate.priority = 4;
    delegate.requester = 3;
    delegate.inspectorInitiatorNodeIdentifier = 77;

    _ZN7WebCore15ResourceRequest41updateFromDelegatePreservingOldPropertiesERKS0_(&request, &delegate);

    Require(request.priority == 2);
    Require(request.requester == 1);
    Require(request.inspectorInitiatorNodeIdentifier && *request.inspectorInitiatorNodeIdentifier == 77);
    Require(!request.platformRequestUpdated);
    Require(request.resourceRequestBodyUpdated && !request.platformRequestBodyUpdated);
}

static void TestDefaultTimeout() {
    Require(_ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() == 0);
    WKURLRequestSetDefaultTimeoutInterval(45.5);
    Require(_ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() == 45.5);
    WKURLRequestSetDefaultTimeoutInterval(0);
    Require(_ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() == 0);
    _ZN7WebCore19ResourceRequestBase25setDefaultTimeoutIntervalEd(30.25);
    Require(_ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() == 30.25);
    _ZN7WebCore19ResourceRequestBase25setDefaultTimeoutIntervalEd(0);
    Require(_ZN7WebCore19ResourceRequestBase22defaultTimeoutIntervalEv() == 0);
}

int main() {
    TestURL();
    TestFirstParty();
    TestMethod();
    TestScalars();
    TestUpdateFromDelegate();
    TestUpdateFromDelegateInspectorFallback();
    TestDefaultTimeout();
    return 0;
}

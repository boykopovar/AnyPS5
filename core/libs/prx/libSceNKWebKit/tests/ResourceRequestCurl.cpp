#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <map>
#include <string>
#include <utility>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::FormData;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WtfAbi::Vector;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceRequest curl check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<class TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static std::map<void*, std::size_t> guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations[pointer] = size;
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1);
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

static StringImpl Make(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

static String Retain(StringImpl& impl) {
    impl.refCount += 2;
    return String(&impl);
}

static std::string Text(const String& value) {
    if (!value.impl) return "<null>";
    Require((value.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(value.impl->data), value.impl->length);
}

static HTTPHeaderMap& Headers(ResourceRequestBase& request) {
    return *reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
}

static Vector<String>& FallbackArray(ResourceRequestBase& request) {
    return *reinterpret_cast<Vector<String>*>(&request.responseContentDispositionEncodingFallbackArray);
}

static ResourceRequestBase MakeRequest() {
    ResourceRequestBase request{};
    request.priority = 1;
    request.allowCookies = true;
    request.isAppInitiated = true;
    request.resourceRequestUpdated = true;
    request.platformRequestUpdated = true;
    request.resourceRequestBodyUpdated = true;
    request.platformRequestBodyUpdated = true;
    return request;
}

static void SetURLField(URL& field, StringImpl& impl, unsigned pathEnd) {
    impl.refCount += 2;
    field.impl = &impl;
    field.bits = 1;
    field.userStart = 8;
    field.userEnd = 8;
    field.passwordEnd = 8;
    field.hostEnd = 24;
    field.pathAfterLastSlash = 25;
    field.pathEnd = pathEnd;
    field.queryEnd = pathEnd;
}

static FormData* CreateForm(const void* data, std::size_t size) {
    auto ref = _ZN7WebCore8FormData6createEPKvm(data, size);
    return std::exchange(ref.m_ptr, nullptr);
}

static void ReleaseRequest(ResourceRequestBase& request) {
    NKWebKit::WtfAbi::Release(std::exchange(request.url.impl, nullptr));
    NKWebKit::WtfAbi::Release(std::exchange(request.firstPartyForCookies.impl, nullptr));
    auto& map = Headers(request);
    reinterpret_cast<Vector<NKWebKit::HttpAbi::CommonHeader>&>(map.common).clear();
    reinterpret_cast<Vector<NKWebKit::HttpAbi::UncommonHeader>&>(map.uncommon).clear();
    FallbackArray(request).clear();
    auto* body = static_cast<FormData*>(std::exchange(request.httpBody, nullptr));
    if (body) body->deref();
    request.httpMethod = String();
    request.initiatorIdentifier = String();
    request.cachePartition = String();
}

static void Update(ResourceRequestBase* request, const ResourceRequestBase* delegate) {
    _ZN7WebCore15ResourceRequest41updateFromDelegatePreservingOldPropertiesERKS0_(request, delegate);
}

static void TestPreservesOldProperties() {
    auto oldURL = Make("http://old.example/a");
    auto newURL = Make("http://new.example/b");
    auto newParty = Make("http://party.example/");
    auto post = Make("POST");
    auto get = Make("GET");
    auto oldInitiator = Make("old-initiator");
    auto newInitiator = Make("new-initiator");
    auto partition = Make("partition");
    auto accept = Make("text/html");
    auto customName = Make("X-Custom");
    auto customValue = Make("yes");
    auto fallback = Make("utf-8");
    auto staleAccept = Make("stale");

    auto request = MakeRequest();
    SetURLField(request.url, oldURL, 20);
    request.httpMethod = Retain(get);
    request.priority = 2;
    request.requester = 3;
    request.hiddenFromInspector = true;
    request.initiatorIdentifier = Retain(oldInitiator);
    request.inspectorInitiatorNodeIdentifier = 7;
    auto staleValue = Retain(staleAccept);
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Referer, &staleValue);
    auto* oldBody = CreateForm("old", 3);
    auto* body = oldBody;
    _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&request, &body);

    auto delegate = MakeRequest();
    SetURLField(delegate.url, newURL, 21);
    SetURLField(delegate.firstPartyForCookies, newParty, 21);
    delegate.timeoutInterval = 12.5;
    delegate.httpMethod = Retain(post);
    delegate.cachePolicy = 2;
    delegate.sameSiteDisposition = NKWebKit::WebCoreAbi::CrossSite;
    delegate.priority = 4;
    delegate.requester = 1;
    delegate.isTopSite = true;
    delegate.allowCookies = false;
    delegate.isAppInitiated = false;
    delegate.useAdvancedPrivacyProtections = true;
    delegate.hiddenFromInspector = false;
    delegate.initiatorIdentifier = Retain(newInitiator);
    delegate.cachePartition = Retain(partition);
    auto acceptValue = Retain(accept);
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&delegate, HTTPHeaderName::Accept, &acceptValue);
    auto name = Retain(customName);
    auto value = Retain(customValue);
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&delegate, &name, &value);
    auto first = Retain(fallback);
    String none;
    _ZN7WebCore19ResourceRequestBase50setResponseContentDispositionEncodingFallbackArrayERKN3WTF6StringES4_S4_(&delegate, &first, &none, &none);
    auto* newBody = CreateForm("new", 3);
    body = newBody;
    _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&delegate, &body);
    delegate.platformRequestUpdated = true;
    delegate.platformRequestBodyUpdated = true;
    const unsigned acceptRefs = accept.refCount;
    const unsigned customNameRefs = customName.refCount;
    const unsigned fallbackRefs = fallback.refCount;

    Update(&request, &delegate);

    Require(request.url.impl == &newURL && newURL.refCount == 6);
    Require(oldURL.refCount == 2);
    Require(request.url.pathEnd == 21 && request.url.bits == 1);
    Require(request.firstPartyForCookies.impl == &newParty && newParty.refCount == 6);
    Require(request.timeoutInterval == 12.5);
    Require(Text(request.httpMethod) == "POST" && post.refCount == 6);
    Require(get.refCount == 2);
    Require(request.cachePolicy == 2);
    Require(request.sameSiteDisposition == NKWebKit::WebCoreAbi::CrossSite);
    Require(request.isTopSite && !request.allowCookies && !request.isAppInitiated && request.useAdvancedPrivacyProtections);
    Require(request.cachePartition.impl == &partition && partition.refCount == 6);
    Require(Headers(request).common.size == 1 && Headers(request).uncommon.size == 1);
    Require(Headers(request).common.buffer != Headers(delegate).common.buffer);
    Require(Headers(request).common.buffer[0].key == HTTPHeaderName::Accept);
    Require(Headers(request).common.buffer[0].value.impl == &accept && accept.refCount == acceptRefs + 2);
    Require(Headers(request).uncommon.buffer[0].key.impl == &customName && customName.refCount == customNameRefs + 2);
    Require(staleAccept.refCount == 4);
    Require(FallbackArray(request).size == 1 && FallbackArray(request).buffer[0].impl == &fallback);
    Require(FallbackArray(request).buffer != FallbackArray(delegate).buffer && fallback.refCount == fallbackRefs + 2);

    Require(request.priority == 2);
    Require(!request.platformRequestUpdated);
    Require(request.httpBody == oldBody && oldBody->refCount == 1);
    Require(delegate.httpBody == newBody && newBody->refCount == 1);
    Require(!request.platformRequestBodyUpdated && request.resourceRequestBodyUpdated);
    Require(request.hiddenFromInspector);
    Require(request.requester == 3);
    Require(Text(request.initiatorIdentifier) == "old-initiator" && oldInitiator.refCount == 4);
    Require(newInitiator.refCount == 4);
    Require(request.inspectorInitiatorNodeIdentifier == 7);

    Require(delegate.priority == 4 && delegate.requester == 1 && !delegate.hiddenFromInspector);
    Require(delegate.platformRequestUpdated && delegate.platformRequestBodyUpdated);

    ReleaseRequest(request);
    ReleaseRequest(delegate);
    Require(guestAllocations.empty());
    Require(newURL.refCount == 2 && newParty.refCount == 2 && post.refCount == 2 && partition.refCount == 2);
    Require(accept.refCount == 4 && customName.refCount == 4 && customValue.refCount == 4 && fallback.refCount == 4);
}

static void TestNodeIdentifierAndSamePriority() {
    auto request = MakeRequest();
    request.priority = 3;
    request.requester = 2;
    auto delegate = MakeRequest();
    delegate.priority = 3;
    delegate.requester = 5;
    delegate.hiddenFromInspector = true;
    delegate.inspectorInitiatorNodeIdentifier = 9;

    Update(&request, &delegate);
    Require(request.inspectorInitiatorNodeIdentifier == 9);
    Require(request.priority == 3 && request.platformRequestUpdated);
    Require(request.requester == 2 && !request.hiddenFromInspector);
    Require(request.httpBody == nullptr && !request.platformRequestBodyUpdated);
    Require(Headers(request).common.buffer == nullptr && Headers(request).uncommon.buffer == nullptr);
    Require(guestAllocations.empty());
}

static void TestSelfUpdate() {
    auto url = Make("http://self.example/");
    auto method = Make("PUT");
    auto initiator = Make("self");
    auto accept = Make("*/*");
    auto request = MakeRequest();
    SetURLField(request.url, url, 20);
    request.httpMethod = Retain(method);
    request.initiatorIdentifier = Retain(initiator);
    request.priority = 4;
    request.requester = 6;
    request.hiddenFromInspector = true;
    request.inspectorInitiatorNodeIdentifier = 3;
    auto acceptValue = Retain(accept);
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Accept, &acceptValue);
    auto* form = CreateForm("self", 4);
    auto* body = form;
    _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&request, &body);
    request.platformRequestUpdated = true;
    auto* headers = Headers(request).common.buffer;
    const unsigned acceptRefs = accept.refCount;

    Update(&request, &request);
    Require(request.url.impl == &url && url.refCount == 4);
    Require(method.refCount == 4 && initiator.refCount == 4);
    Require(Headers(request).common.buffer == headers && accept.refCount == acceptRefs);
    Require(request.httpBody == form && form->refCount == 1);
    Require(request.priority == 4 && request.platformRequestUpdated);
    Require(request.requester == 6 && request.hiddenFromInspector && request.inspectorInitiatorNodeIdentifier == 3);

    ReleaseRequest(request);
    Require(guestAllocations.empty());
}

static void TestErrors() {
    auto request = MakeRequest();
    auto delegate = MakeRequest();
    Require(Throws([&] { Update(nullptr, &delegate); }));
    Require(Throws([&] { Update(&request, nullptr); }));

    auto url = Make("http://kept.example/");
    SetURLField(request.url, url, 20);
    request.resourceRequestUpdated = false;
    Require(Throws([&] { Update(&request, &delegate); }));
    Require(request.url.impl == &url && url.refCount == 4);
    request.resourceRequestUpdated = true;

    request.resourceRequestBodyUpdated = false;
    Require(Throws([&] { Update(&request, &delegate); }));
    Require(request.url.impl == &url);
    request.resourceRequestBodyUpdated = true;

    auto* form = CreateForm("x", 1);
    auto* body = form;
    _ZN7WebCore19ResourceRequestBase11setHTTPBodyEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&request, &body);
    delegate.resourceRequestUpdated = false;
    Require(Throws([&] { Update(&request, &delegate); }));
    Require(request.url.impl == nullptr && url.refCount == 2);
    Require(request.httpBody == nullptr && guestAllocations.count(form) == 0);

    auto& vector = reinterpret_cast<Vector<NKWebKit::HttpAbi::CommonHeader>&>(Headers(delegate).common);
    request.resourceRequestUpdated = true;
    delegate.resourceRequestUpdated = true;
    vector.size = 1;
    Require(Throws([&] { Update(&request, &delegate); }));
    vector.size = 0;

    ReleaseRequest(request);
    ReleaseRequest(delegate);
    Require(guestAllocations.empty());
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestPreservesOldProperties();
    TestNodeIdentifierAndSamePriority();
    TestSelfUpdate();
    TestErrors();
    Require(guestAllocations.empty());
    return 0;
}

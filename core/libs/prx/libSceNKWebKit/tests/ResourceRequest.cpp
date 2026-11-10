#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

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

static void TestURL() {
    ResourceRequestBase request{};
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
    ResourceRequestBase request{};
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
    ResourceRequestBase request{};
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
    ResourceRequestBase request{};
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

static void TestDefaultTimeout() {
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
    TestDefaultTimeout();
    return 0;
}

#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <string_view>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;
using OriginRef = WebCore::Ref<NKWebKit::WebCoreAbi::SecurityOrigin>;
using WebCore::StoredCredentialsPolicy;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "updateRequestForAccessControl check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<class TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static String Make(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static bool Is(const String& value, std::string_view expected) {
    if (!value.impl || value.impl->length != expected.size()) return false;
    const auto view = NKWebKit::WtfAbi::ViewOf(value.impl);
    for (unsigned i = 0; i < view.length; ++i) {
        if (NKWebKit::WtfAbi::CodeUnitAt(view, i) != static_cast<unsigned char>(expected[i])) return false;
    }
    return true;
}

struct TestURL {
    String storage;
    URL url;
};

static TestURL MakeURL(std::string_view scheme, std::string_view host, std::string_view port, std::string_view path, bool authority = true) {
    std::string text(scheme);
    text += authority ? "://" : ":";
    text += host;
    if (!port.empty()) {
        text += ':';
        text += port;
    }
    text += path;
    TestURL result{Make(text), {}};
    const unsigned schemeEnd = static_cast<unsigned>(scheme.size());
    const unsigned userStart = schemeEnd + (authority ? 3 : 1);
    const unsigned hostEnd = userStart + static_cast<unsigned>(host.size());
    const unsigned portLength = port.empty() ? 0 : static_cast<unsigned>(port.size()) + 1;
    const unsigned pathStart = hostEnd + portLength;
    const auto slash = path.rfind('/');
    const bool httpFamily = scheme == "http" || scheme == "https";
    result.url.impl = result.storage.impl;
    result.url.bits = 1U | (httpFamily ? 2U : 0U) | (portLength << 3) | (schemeEnd << 6);
    result.url.userStart = userStart;
    result.url.userEnd = userStart;
    result.url.passwordEnd = userStart;
    result.url.hostEnd = hostEnd;
    result.url.pathAfterLastSlash = pathStart + (slash == std::string_view::npos ? 0 : static_cast<unsigned>(slash) + 1);
    result.url.pathEnd = pathStart + static_cast<unsigned>(path.size());
    result.url.queryEnd = result.url.pathEnd;
    return result;
}

static OriginRef Create(const TestURL& url) {
    return _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(&url.url);
}

static HTTPHeaderMap& Headers(ResourceRequestBase& request) {
    return *reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
}

static ResourceRequestBase MakeRequest() {
    ResourceRequestBase request = ResourceRequestBase();
    request.resourceRequestUpdated = true;
    request.platformRequestUpdated = true;
    return request;
}

static void Clear(ResourceRequestBase& request) {
    auto& map = Headers(request);
    while (map.common.size) _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&map, map.common.buffer[0].key);
    while (map.uncommon.size) {
        String key(map.uncommon.buffer[0].key);
        _ZN7WebCore13HTTPHeaderMap6removeERKN3WTF6StringE(&map, key);
    }
    _ZN3WTF8fastFreeEPv(map.common.buffer);
    _ZN3WTF8fastFreeEPv(map.uncommon.buffer);
    map = HTTPHeaderMap{};
}

static void Update(ResourceRequestBase* request, const OriginRef& origin, StoredCredentialsPolicy policy) {
    _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(request, origin.get(), policy);
}

static String Origin(const ResourceRequestBase& request) {
    return _ZNK7WebCore19ResourceRequestBase10httpOriginEv(&request);
}

static void TestUseAllowsCookiesAndSetsOrigin() {
    auto origin = Create(MakeURL("http", "example.com", "8080", "/page"));
    auto request = MakeRequest();
    Update(&request, origin, StoredCredentialsPolicy::Use);
    Require(request.allowCookies);
    Require(!request.platformRequestUpdated);
    Require(Is(Origin(request), "http://example.com:8080"));
    Require(Headers(request).common.size == 1 && Headers(request).uncommon.size == 0);
    Clear(request);
}

static void TestDoNotUseReplacesOrigin() {
    auto first = Create(MakeURL("https", "a.test", "", "/"));
    auto second = Create(MakeURL("https", "b.test", "", "/x"));
    auto request = MakeRequest();
    request.allowCookies = true;
    Update(&request, first, StoredCredentialsPolicy::Use);
    Require(request.allowCookies && Is(Origin(request), "https://a.test"));
    request.platformRequestUpdated = true;
    Update(&request, second, StoredCredentialsPolicy::DoNotUse);
    Require(!request.allowCookies);
    Require(!request.platformRequestUpdated);
    Require(Is(Origin(request), "https://b.test"));
    Require(Headers(request).common.size == 1);
    Clear(request);
}

static void TestEphemeralStatelessDisallowsCookies() {
    auto origin = Create(MakeURL("https", "c.test", "", "/"));
    auto request = MakeRequest();
    request.allowCookies = true;
    Update(&request, origin, StoredCredentialsPolicy::EphemeralStateless);
    Require(!request.allowCookies);
    Require(Is(Origin(request), "https://c.test"));
    Clear(request);
}

static void TestOpaqueOriginSerializesAsNull() {
    auto origin = Create(MakeURL("data", "", "", "text/plain,x", false));
    auto request = MakeRequest();
    Update(&request, origin, StoredCredentialsPolicy::DoNotUse);
    Require(!request.allowCookies);
    Require(Is(Origin(request), "null"));
    Clear(request);
}

static void TestInvalidArguments() {
    auto origin = Create(MakeURL("https", "d.test", "", "/"));
    auto request = MakeRequest();
    request.allowCookies = true;
    Require(Throws([&] { _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(nullptr, origin.get(), StoredCredentialsPolicy::Use); }));
    Require(Throws([&] { _ZN7WebCore29updateRequestForAccessControlERNS_15ResourceRequestERNS_14SecurityOriginENS_23StoredCredentialsPolicyE(&request, nullptr, StoredCredentialsPolicy::Use); }));
    Require(Throws([&] { Update(&request, origin, static_cast<StoredCredentialsPolicy>(3)); }));
    Require(request.allowCookies && request.platformRequestUpdated);
    Require(!Origin(request).impl);
    request.resourceRequestUpdated = false;
    Require(Throws([&] { Update(&request, origin, StoredCredentialsPolicy::DoNotUse); }));
    Require(request.allowCookies && request.platformRequestUpdated);
    request.resourceRequestUpdated = true;
    Require(!Origin(request).impl);
}

static void TestCredentialedURLLeavesRequestUntouched() {
    auto origin = Create(MakeURL("https", "e.test", "", "/"));
    auto url = MakeURL("https", "user:pass@e.test", "", "/");
    url.url.userEnd = url.url.userStart + 4;
    url.url.passwordEnd = url.url.userStart + 9;
    auto request = MakeRequest();
    request.url = url.url;
    Require(Throws([&] { Update(&request, origin, StoredCredentialsPolicy::Use); }));
    Require(!request.allowCookies && request.platformRequestUpdated);
    Require(!Origin(request).impl);
    request.url = URL{};
}

int main() {
    TestUseAllowsCookiesAndSetsOrigin();
    TestDoNotUseReplacesOrigin();
    TestEphemeralStatelessDisallowsCookies();
    TestOpaqueOriginSerializesAsNull();
    TestInvalidArguments();
    TestCredentialedURLLeavesRequestUntouched();
    return 0;
}

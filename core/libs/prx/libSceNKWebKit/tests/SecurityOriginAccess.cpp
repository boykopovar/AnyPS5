#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::SecurityOriginData;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::URL;
using OriginRef = WebCore::Ref<SecurityOrigin>;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SecurityOriginAccess check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<typename TFunction>
static bool ThrowsNotImplemented(TFunction&& function) {
    const auto before = NotImplementedCalls_nid_no_patch();
    try {
        function();
    } catch (const std::runtime_error&) {
        return NotImplementedCalls_nid_no_patch() == before + 1;
    }
    return false;
}

static String Make(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
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

static OriginRef Create(std::string_view scheme, std::string_view host, std::string_view port = {}, std::string_view path = "/") {
    return Create(MakeURL(scheme, host, port, path));
}

static OriginRef CreateFromData(std::string_view protocol, std::string_view host) {
    SecurityOriginData data{};
    new (&data.storage.tuple) SecurityOriginTuple{Make(protocol), Make(host), {0, false}};
    data.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
    auto origin = _ZNK7WebCore18SecurityOriginData14securityOriginEv(&data);
    data.storage.tuple.~SecurityOriginTuple();
    return origin;
}

static bool CanAccess(const OriginRef& origin, const OriginRef& other) {
    return _ZNK7WebCore14SecurityOrigin9canAccessERKS0_(origin.get(), other.get());
}

static bool CanRequest(const OriginRef& origin, const TestURL& url) {
    return _ZNK7WebCore14SecurityOrigin10canRequestERKN3WTF3URLE(origin.get(), &url.url);
}

static bool CanDisplay(const OriginRef& origin, const TestURL& url) {
    return _ZNK7WebCore14SecurityOrigin10canDisplayERKN3WTF3URLE(origin.get(), &url.url);
}

static void Allow(const OriginRef& origin, std::string_view protocol, std::string_view domain, bool allowSubdomains) {
    auto destinationProtocol = Make(protocol);
    auto destinationDomain = Make(domain);
    _ZN7WebCore14SecurityPolicy29addOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(origin.get(), &destinationProtocol, &destinationDomain, allowSubdomains);
}

static void Disallow(const OriginRef& origin, std::string_view protocol, std::string_view domain, bool allowSubdomains) {
    auto destinationProtocol = Make(protocol);
    auto destinationDomain = Make(domain);
    _ZN7WebCore14SecurityPolicy32removeOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(origin.get(), &destinationProtocol, &destinationDomain, allowSubdomains);
}

static void Reset() {
    _ZN7WebCore14SecurityPolicy27resetOriginAccessAllowlistsEv();
}

static void RegisterLocal(std::string_view scheme) {
    _ZN7WebCore20LegacySchemeRegistry24registerURLSchemeAsLocalERKN3WTF6StringE(Make(scheme));
}

static void TestCanAccess() {
    auto origin = Create("https", "example.com");
    auto same = Create("https", "example.com", "443", "/other");
    auto opaque = Create("custom", "example.com");
    Require(opaque->data.index == NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex);

    Require(CanAccess(origin, origin));
    Require(CanAccess(opaque, opaque));
    Require(CanAccess(origin, same));
    Require(CanAccess(same, origin));
    Require(!CanAccess(origin, opaque));
    Require(!CanAccess(opaque, origin));
    Require(!CanAccess(origin, Create("http", "example.com")));
    Require(!CanAccess(origin, Create("https", "www.example.com")));
    Require(!CanAccess(origin, Create("https", "example.com", "8443")));

    auto first = Create("https", "a.example.com");
    auto second = Create("https", "b.example.com", "8443");
    Require(!CanAccess(first, second));
    first->domainWasSetInDOM = true;
    first->domain = Make("example.com");
    Require(!CanAccess(first, second));
    Require(!CanAccess(second, first));
    second->domainWasSetInDOM = true;
    second->domain = Make("example.com");
    Require(CanAccess(first, second));
    Require(CanAccess(second, first));
    second->domain = Make("other.com");
    Require(!CanAccess(first, second));
    auto insecure = Create("http", "c.example.com");
    insecure->domainWasSetInDOM = true;
    insecure->domain = Make("example.com");
    Require(!CanAccess(first, insecure));

    auto sameHostWithDomain = Create("https", "example.com");
    sameHostWithDomain->domainWasSetInDOM = true;
    sameHostWithDomain->domain = Make("example.com");
    Require(!CanAccess(origin, sameHostWithDomain));

    auto file = Create("file", "", "", "/tmp/a");
    auto otherFile = Create("file", "", "", "/tmp/b");
    Require(CanAccess(file, otherFile));
    otherFile->enforcesFilePathSeparation = true;
    Require(!CanAccess(file, otherFile));
    Require(!CanAccess(otherFile, file));
    Require(CanAccess(otherFile, otherFile));

    opaque->universalAccess = true;
    Require(CanAccess(opaque, origin));
    Require(!CanAccess(origin, opaque));

    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin9canAccessERKS0_(nullptr, origin.get()); }));
    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin9canAccessERKS0_(origin.get(), nullptr); }));
    Require(origin->refCount.load() == 1);
}

static void TestCanRequest() {
    Reset();
    auto origin = Create("http", "example.com");
    Require(CanRequest(origin, MakeURL("http", "example.com", "", "/path")));
    Require(CanRequest(origin, MakeURL("http", "example.com", "80", "/")));
    Require(!CanRequest(origin, MakeURL("http", "example.com", "8080", "/")));
    Require(!CanRequest(origin, MakeURL("https", "example.com", "", "/")));
    Require(!CanRequest(origin, MakeURL("http", "www.example.com", "", "/")));
    Require(!CanRequest(origin, MakeURL("custom", "example.com", "", "/")));

    origin->domainWasSetInDOM = true;
    origin->domain = Make("other.com");
    Require(CanRequest(origin, MakeURL("http", "example.com", "", "/")));

    auto opaque = Create("custom", "example.com");
    Require(!CanRequest(opaque, MakeURL("custom", "example.com", "", "/")));
    Require(!CanRequest(opaque, MakeURL("http", "example.com", "", "/")));

    auto file = Create("file", "", "", "/tmp/a");
    Require(CanRequest(file, MakeURL("file", "", "", "/tmp/b")));
    file->enforcesFilePathSeparation = true;
    Require(!CanRequest(file, MakeURL("file", "", "", "/tmp/b")));

    auto blob = MakeURL("blob", "", "", "http://example.com/uuid", false);
    Require(ThrowsNotImplemented([&] { CanRequest(origin, blob); }));
    Require(ThrowsNotImplemented([&] { CanRequest(opaque, blob); }));

    auto universal = Create("custom", "example.com");
    universal->universalAccess = true;
    Require(CanRequest(universal, MakeURL("https", "anything.com", "", "/")));
    Require(CanRequest(universal, blob));

    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin10canRequestERKN3WTF3URLE(nullptr, &blob.url); }));
    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin10canRequestERKN3WTF3URLE(origin.get(), nullptr); }));
    Require(origin->refCount.load() == 1);
}

static void TestAllowlist() {
    Reset();
    auto origin = Create("http", "example.com");
    auto exact = MakeURL("https", "b.com", "", "/");
    auto subdomain = MakeURL("https", "sub.b.com", "", "/");
    auto deeper = MakeURL("https", "x.sub.b.com", "", "/");
    auto lookalike = MakeURL("https", "notb.com", "", "/");
    Require(!CanRequest(origin, exact));

    Allow(origin, "HTTPS", "B.com", false);
    Require(CanRequest(origin, exact));
    Require(!CanRequest(origin, subdomain));
    Require(!CanRequest(origin, MakeURL("http", "b.com", "", "/")));
    Require(CanRequest(Create("http", "example.com", "80"), exact));
    Require(!CanRequest(Create("http", "example.com", "8080"), exact));
    Require(!CanRequest(Create("https", "example.com"), exact));

    Allow(origin, "https", "b.com", true);
    Require(CanRequest(origin, subdomain));
    Require(CanRequest(origin, deeper));
    Require(!CanRequest(origin, lookalike));

    Disallow(origin, "https", "b.com", true);
    Require(CanRequest(origin, exact));
    Require(!CanRequest(origin, subdomain));
    Disallow(origin, "https", "b.com", true);
    Disallow(origin, "https", "missing.com", false);
    Disallow(Create("http", "unknown.com"), "https", "b.com", false);
    Require(CanRequest(origin, exact));
    Disallow(origin, "HTTPS", "B.COM", false);
    Require(!CanRequest(origin, exact));

    Allow(origin, "https", "b.com", false);
    Allow(origin, "https", "b.com", false);
    Disallow(origin, "https", "b.com", false);
    Require(CanRequest(origin, exact));
    Disallow(origin, "https", "b.com", false);
    Require(!CanRequest(origin, exact));

    Allow(origin, "http", "2.3.4", true);
    Require(!CanRequest(origin, MakeURL("http", "1.2.3.4", "", "/")));
    Allow(origin, "http", "1.2.3.4", false);
    Require(CanRequest(origin, MakeURL("http", "1.2.3.4", "", "/")));
    Allow(origin, "http", "com", true);
    Require(CanRequest(origin, MakeURL("http", "anything.com", "", "/")));
    Require(!CanRequest(origin, MakeURL("http", "5.6.7.8", "", "/")));
    Allow(origin, "http", "", false);
    Require(!CanRequest(origin, MakeURL("http", "5.6.7.8", "", "/")));
    Allow(origin, "http", "", true);
    Require(CanRequest(origin, MakeURL("http", "5.6.7.8", "", "/")));
    Require(CanRequest(origin, MakeURL("http", "any.where", "", "/")));
    Require(!CanRequest(origin, MakeURL("ftp", "any.where", "", "/")));

    Reset();
    Require(!CanRequest(origin, exact));
    Require(!CanRequest(origin, MakeURL("http", "any.where", "", "/")));

    auto opaque = Create("custom", "example.com");
    Allow(opaque, "https", "b.com", false);
    Disallow(opaque, "https", "b.com", false);
    Require(!CanRequest(opaque, exact));

    auto protocol = Make("https");
    auto domain = Make("b.com");
    Require(Throws([&] { _ZN7WebCore14SecurityPolicy29addOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(nullptr, &protocol, &domain, false); }));
    Require(Throws([&] { _ZN7WebCore14SecurityPolicy29addOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(origin.get(), nullptr, &domain, false); }));
    Require(Throws([&] { _ZN7WebCore14SecurityPolicy29addOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(origin.get(), &protocol, nullptr, false); }));
    Require(Throws([&] { _ZN7WebCore14SecurityPolicy32removeOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(nullptr, &protocol, &domain, false); }));
    Require(Throws([&] { _ZN7WebCore14SecurityPolicy32removeOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(origin.get(), nullptr, &domain, false); }));
    Require(Throws([&] { _ZN7WebCore14SecurityPolicy32removeOriginAccessAllowlistEntryERKNS_14SecurityOriginERKN3WTF6StringES7_b(origin.get(), &protocol, nullptr, false); }));
    Require(!CanRequest(origin, exact));
    Require(origin->refCount.load() == 1);
}

static void TestCanDisplay() {
    Reset();
    auto origin = Create("http", "example.com");
    auto remote = MakeURL("https", "other.com", "", "/");
    Require(CanDisplay(origin, remote));

    auto huge = MakeURL("https", "other.com", "", "/");
    huge.url.pathEnd = 0x04000000;
    Require(CanDisplay(origin, huge));
    huge.url.pathEnd = 0x04000001;
    Require(!CanDisplay(origin, huge));
    auto universal = Create("http", "example.com");
    universal->universalAccess = true;
    Require(CanDisplay(universal, huge));

    auto localFile = MakeURL("file", "", "", "/tmp/a");
    Require(!CanDisplay(origin, localFile));
    origin->filePath = Make("/tmp/a");
    Require(CanDisplay(origin, localFile));
    origin->filePath = String();
    origin->canLoadLocalResources = true;
    Require(CanDisplay(origin, localFile));
    origin->canLoadLocalResources = false;
    Allow(origin, "file", "", true);
    Require(CanDisplay(origin, localFile));
    Reset();
    Require(!CanDisplay(origin, localFile));

    _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(NKWebKit::WebCoreAbi::AllowLocalLoadsForAll);
    Require(CanDisplay(origin, localFile));
    _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(NKWebKit::WebCoreAbi::AllowLocalLoadsForLocalAndSubstituteData);
    Require(!CanDisplay(origin, localFile));
    _ZN7WebCore14SecurityPolicy18setLocalLoadPolicyENS0_15LocalLoadPolicyE(NKWebKit::WebCoreAbi::AllowLocalLoadsForLocalOnly);

    auto file = Create("file", "", "", "/tmp/a");
    Require(ThrowsNotImplemented([&] { CanDisplay(file, localFile); }));
    Require(CanDisplay(file, remote));

    RegisterLocal("feed");
    Require(!CanDisplay(origin, MakeURL("feed", "example.com", "", "/", false)));
    Require(!CanDisplay(origin, MakeURL("feed", "http", "", "//example.com/", false)));
    Require(!CanDisplay(origin, MakeURL("feed", "ftp:", "", "//example.com/", false)));
    Require(CanDisplay(origin, MakeURL("feed", "example.com", "", "/")));
    Require(CanDisplay(origin, MakeURL("feed", "http:", "", "//example.com/", false)));
    Require(CanDisplay(origin, MakeURL("FEED", "HTTPS:", "", "//example.com/", false)));
    Require(CanDisplay(origin, MakeURL("feeds", "https:", "", "//example.com/", false)));
    Require(CanDisplay(origin, MakeURL("feedsearch", "http:", "", "//example.com/", false)));

    auto blob = MakeURL("blob", "", "", "http://example.com/uuid", false);
    Require(ThrowsNotImplemented([&] { CanDisplay(origin, blob); }));
    Require(CanDisplay(universal, blob));

    _ZN7WebCore20LegacySchemeRegistry36registerAsCanDisplayOnlyIfCanRequestERKN3WTF6StringE(Make("ftp"));
    Require(!CanDisplay(origin, MakeURL("ftp", "example.com", "", "/")));
    Require(CanDisplay(Create("ftp", "example.com"), MakeURL("ftp", "example.com", "", "/")));
    Allow(origin, "ftp", "example.com", false);
    Require(CanDisplay(origin, MakeURL("ftp", "example.com", "", "/")));
    Reset();

    _ZN7WebCore20LegacySchemeRegistry34registerURLSchemeAsDisplayIsolatedERKN3WTF6StringE(Make("Isolated"));
    auto isolated = MakeURL("isolated", "host", "", "/");
    Require(!CanDisplay(origin, isolated));
    Require(CanDisplay(CreateFromData("isolated", "elsewhere"), isolated));
    Require(CanDisplay(CreateFromData("ISOLATED", "elsewhere"), isolated));
    Require(!CanDisplay(CreateFromData("isolatedx", "elsewhere"), isolated));

    RegisterLocal("localscheme");
    auto local = MakeURL("localscheme", "host", "", "/");
    Require(!CanDisplay(origin, local));
    origin->canLoadLocalResources = true;
    Require(CanDisplay(origin, local));
    origin->canLoadLocalResources = false;

    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin10canDisplayERKN3WTF3URLE(nullptr, &remote.url); }));
    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin10canDisplayERKN3WTF3URLE(origin.get(), nullptr); }));
    Require(origin->refCount.load() == 1);
}

int main() {
    TestCanAccess();
    TestCanRequest();
    TestAllowlist();
    TestCanDisplay();
    Reset();
    return 0;
}

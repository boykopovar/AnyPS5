#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using NKWebKit::WebCoreAbi::OpaqueOriginIdentifier;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::SecurityOriginData;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::URL;
using OriginRef = WebCore::Ref<SecurityOrigin>;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "SecurityOrigin check failed at line %d\n", line);
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

static bool Local(const char* text) {
    return _ZN7WebCore14SecurityOrigin30isLocalHostOrLoopbackIPAddressEN3WTF10StringViewE(StringView{text, static_cast<unsigned>(std::strlen(text)), true});
}

static bool Local16(const std::u16string& text) {
    return _ZN7WebCore14SecurityOrigin30isLocalHostOrLoopbackIPAddressEN3WTF10StringViewE(StringView{text.data(), static_cast<unsigned>(text.size()), false});
}

static String Make(std::string_view text, bool wide = false) {
    if (wide) {
        std::u16string units(text.begin(), text.end());
        return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
    }
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

static bool Is8Bit(const String& value) {
    return value.impl && (value.impl->hashAndFlags & 4);
}

struct TestURL {
    String storage;
    URL url;
};

static TestURL MakeURL(std::string_view scheme, std::string_view host, std::string_view port, std::string_view path, bool authority = true, bool wide = false) {
    std::string text(scheme);
    text += authority ? "://" : ":";
    text += host;
    if (!port.empty()) {
        text += ':';
        text += port;
    }
    text += path;
    TestURL result{Make(text, wide), {}};
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

static String ToString(const OriginRef& origin) {
    return _ZNK7WebCore14SecurityOrigin8toStringEv(origin.get());
}

static String ToRawString(const OriginRef& origin) {
    return _ZNK7WebCore14SecurityOrigin11toRawStringEv(origin.get());
}

static bool SameOrigin(const OriginRef& a, const OriginRef& b) {
    return _ZNK7WebCore14SecurityOrigin14isSameOriginAsERKS0_(a.get(), b.get());
}

static bool SameSchemeHostPort(const OriginRef& a, const OriginRef& b) {
    return _ZNK7WebCore14SecurityOrigin20isSameSchemeHostPortERKS0_(a.get(), b.get());
}

static bool Equal(const OriginRef& a, const OriginRef& b) {
    return _ZNK7WebCore14SecurityOrigin5equalEPKS0_(a.get(), b.get());
}

static bool Suffix(const OriginRef& origin, std::string_view suffix, bool treatIPAddressAsDomain = false) {
    auto value = Make(suffix);
    return _ZNK7WebCore14SecurityOrigin33isMatchingRegistrableDomainSuffixERKN3WTF6StringEb(origin.get(), &value, treatIPAddressAsDomain);
}

static bool IsOpaque(const OriginRef& origin) {
    return origin->data.index == NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;
}

struct Data {
    SecurityOriginData data{};

    Data(std::string_view protocol, std::string_view host, std::optional<std::uint16_t> port) {
        new (&data.storage.tuple) SecurityOriginTuple{Make(protocol), Make(host), {port.value_or(0), port.has_value()}};
        data.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
    }
    explicit Data(OpaqueOriginIdentifier identifier) {
        data.storage.opaque = identifier;
        data.index = NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;
    }
    Data(const Data&) = delete;
    Data& operator=(const Data&) = delete;
    ~Data() {
        if (data.index == NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex) data.storage.tuple.~SecurityOriginTuple();
    }
};

static void TestLoopback() {
    Require(Local("[::1]"));
    Require(!Local("::1"));
    Require(!Local("[::2]"));
    Require(Local("127.0.0.1"));
    Require(Local("127.255.255.255"));
    Require(Local("127.1.2.3"));
    Require(!Local("127.0.0"));
    Require(!Local("127.0.0.1.1"));
    Require(!Local("127.0.0.a"));
    Require(!Local("128.0.0.1"));
    Require(!Local("127."));
    Require(!Local("127"));
    Require(Local("localhost"));
    Require(Local("LocalHost"));
    Require(Local("LOCALHOST"));
    Require(Local("foo.localhost"));
    Require(Local("a.b.LOCALHOST"));
    Require(Local(".localhost"));
    Require(!Local("localhost.example"));
    Require(!Local("notlocalhost"));
    Require(!Local("localhos"));
    Require(!Local("example.com"));
    Require(!Local(""));

    Require(Local16(u"localhost"));
    Require(Local16(u"sub.LocalHost"));
    Require(Local16(u"127.0.0.1"));
    Require(Local16(u"[::1]"));
    Require(!Local16(u"locаlhost"));
    Require(!Local16(u"127.0.0.١"));

    StringView null{nullptr, 0, true};
    Require(!_ZN7WebCore14SecurityOrigin30isLocalHostOrLoopbackIPAddressEN3WTF10StringViewE(null));
}

static void TestTupleOrigins() {
    auto origin = Create("http", "Example.COM", "8080", "/a/b");
    Require(!IsOpaque(origin));
    Require(origin->refCount.load() == 1);
    Require(Is(origin->data.storage.tuple.protocol, "http"));
    Require(Is(origin->data.storage.tuple.host, "example.com"));
    Require(origin->data.storage.tuple.port.hasValue && origin->data.storage.tuple.port.value == 8080);
    Require(Is(origin->domain, "example.com"));
    Require(!origin->filePath.impl);
    Require(!origin->isLocal && !origin->canLoadLocalResources && !origin->universalAccess && !origin->domainWasSetInDOM);
    Require(!origin->isPotentiallyTrustworthy.hasValue);
    Require(Is(ToString(origin), "http://example.com:8080"));
    Require(Is(ToRawString(origin), "http://example.com:8080"));
    Require(Is8Bit(ToString(origin)));

    Require(Is(ToString(Create("http", "example.com", "80")), "http://example.com"));
    Require(!Create("http", "example.com", "80")->data.storage.tuple.port.hasValue);
    Require(Is(ToString(Create("https", "example.com", "443")), "https://example.com"));
    Require(Is(ToString(Create("https", "example.com", "80")), "https://example.com:80"));
    Require(Is(ToString(Create("ws", "example.com", "80")), "ws://example.com"));
    Require(Is(ToString(Create("wss", "example.com", "443")), "wss://example.com"));
    Require(Is(ToString(Create("ftp", "example.com", "21")), "ftp://example.com"));
    Require(Is(ToString(Create("ftp", "example.com", "22")), "ftp://example.com:22"));
    Require(Is(ToString(Create("http", "example.com", "0")), "http://example.com:0"));
    Require(Is(ToString(Create("http", "example.com", "65535")), "http://example.com:65535"));
    Require(Is(ToString(Create("ws", "", "", "/")), "ws://"));

    auto wide = Create(MakeURL("https", "WIDE.example", "", "/", true, true));
    Require(Is(ToString(wide), "https://wide.example"));
    Require(!Is8Bit(ToString(wide)));
    Require(SameOrigin(wide, Create("https", "wide.example")));
}

static void TestOpaqueOrigins() {
    auto data = Create(MakeURL("data", "", "", "text/plain,x", false));
    auto data2 = Create(MakeURL("data", "", "", "text/plain,x", false));
    auto javascript = Create(MakeURL("javascript", "", "", "alert(1)", false));
    auto about = Create(MakeURL("about", "", "", "blank", false));
    auto custom = Create("custom", "host");
    auto emptyHost = Create("http", "", "", "/path");
    auto emptyFtpHost = Create("ftp", "", "", "/path");
    auto invalidURL = MakeURL("https", "example.com", "", "/");
    invalidURL.url.bits &= ~1U;
    auto invalid = Create(invalidURL);
    URL nullURL{};
    auto null = _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(&nullURL);
    for (const auto* origin : {&data, &data2, &javascript, &about, &custom, &emptyHost, &emptyFtpHost, &invalid, &null}) {
        Require(IsOpaque(*origin));
        Require(Is(ToString(*origin), "null"));
        Require(!ToRawString(*origin).impl);
        Require((*origin)->isPotentiallyTrustworthy.hasValue && !(*origin)->isPotentiallyTrustworthy.value);
        Require(Is((*origin)->domain, ""));
        Require((*origin)->data.storage.opaque.object != 0);
        Require((*origin)->data.storage.opaque.processIdentifier == _ZN7WebCore7Process10identifierEv());
        Require(SameOrigin(*origin, *origin));
        Require(SameSchemeHostPort(*origin, *origin));
        Require(Equal(*origin, *origin));
        Require(Is(_ZNK7WebCore14SecurityOrigin23domainForCachePartitionEv(origin->get()), ""));
        Require(!Suffix(*origin, "example.com"));
    }
    Require(data->data.storage.opaque.object != data2->data.storage.opaque.object);
    Require(!SameOrigin(data, data2));
    Require(!SameSchemeHostPort(data, data2));
    Require(!Equal(data, data2));
    auto http = Create("http", "example.com");
    Require(!SameOrigin(data, http));
    Require(!SameOrigin(http, data));
    Require(!SameSchemeHostPort(data, http));
    Require(!SameSchemeHostPort(http, data));
    Require(!Equal(http, data));

    auto copy = _ZNK7WebCore14SecurityOrigin12isolatedCopyEv(data.get());
    Require(copy.get() != data.get());
    Require(IsOpaque(copy));
    Require(SameOrigin(data, copy));
    Require(SameSchemeHostPort(data, copy));
    Require(Equal(copy, data));
}

static void TestCreateUnique() {
    auto unique = _ZN7WebCore14SecurityOrigin12createUniqueEv();
    Require(unique->refCount.load() == 1);
    Require(IsOpaque(unique));
    Require(unique->data.storage.opaque.object != 0);
    Require(unique->data.storage.opaque.processIdentifier == _ZN7WebCore7Process10identifierEv());
    Require(Is(unique->domain, ""));
    Require(!unique->filePath.impl);
    Require(!unique->universalAccess && !unique->domainWasSetInDOM && !unique->canLoadLocalResources);
    Require(!unique->enforcesFilePathSeparation && !unique->needsStorageAccessFromFileURLsQuirk && !unique->isLocal);
    Require(unique->isPotentiallyTrustworthy.hasValue && !unique->isPotentiallyTrustworthy.value);
    Require(Is(ToString(unique), "null"));
    Require(!ToRawString(unique).impl);
    Require(Is(_ZNK7WebCore14SecurityOrigin23domainForCachePartitionEv(unique.get()), ""));
    Require(SameOrigin(unique, unique));
    Require(Equal(unique, unique));

    auto other = _ZN7WebCore14SecurityOrigin12createUniqueEv();
    Require(other.get() != unique.get());
    Require(other->data.storage.opaque.object != unique->data.storage.opaque.object);
    Require(!SameOrigin(unique, other));
    Require(!Equal(unique, other));
    Require(!SameOrigin(unique, Create(MakeURL("data", "", "", "text/plain,x", false))));
    Require(!SameOrigin(unique, Create("https", "example.com")));

    auto copy = _ZNK7WebCore14SecurityOrigin12isolatedCopyEv(unique.get());
    Require(copy.get() != unique.get());
    Require(IsOpaque(copy));
    Require(SameOrigin(unique, copy));
    Require(Equal(copy, unique));
}

static void TestFileOrigins() {
    auto file = Create("file", "", "", "/tmp/a%20b");
    Require(!IsOpaque(file));
    Require(file->isLocal && file->canLoadLocalResources);
    Require(Is(file->filePath, "/tmp/a b"));
    Require(Is(ToString(file), "file://"));
    Require(Is(ToRawString(file), "file://"));
    Require(Is(_ZNK7WebCore14SecurityOrigin23domainForCachePartitionEv(file.get()), ""));

    auto other = Create("file", "", "", "/other");
    Require(SameOrigin(file, other));
    Require(SameSchemeHostPort(file, other));
    Require(Equal(file, other));
    other->enforcesFilePathSeparation = true;
    Require(Is(ToString(other), "null"));
    Require(Is(ToRawString(other), "file://"));
    Require(!SameOrigin(file, other));
    Require(!SameOrigin(other, file));
    Require(!Equal(file, other));
    Require(SameOrigin(other, other));
    Require(!SameSchemeHostPort(file, other));
    Require(!SameSchemeHostPort(other, file));
    Require(!SameSchemeHostPort(other, other));

    auto server = Create("file", "server", "", "/share");
    Require(Is(ToString(server), "file://"));
    Require(!SameOrigin(file, server));
    Require(!SameSchemeHostPort(file, server));

    auto blob = MakeURL("blob", "", "", "https://example.com/uuid", false);
    Require(ThrowsNotImplemented([&] { _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(&blob.url); }));
}

static void TestComparisons() {
    auto a = Create("https", "example.com");
    auto b = Create("https", "example.com", "443", "/other");
    Require(SameOrigin(a, b));
    Require(SameSchemeHostPort(a, b));
    Require(SameSchemeHostPort(b, a));
    Require(Equal(a, b));
    Require(!SameOrigin(a, Create("http", "example.com")));
    Require(!SameSchemeHostPort(a, Create("http", "example.com")));
    Require(!SameSchemeHostPort(a, Create("https", "www.example.com")));
    Require(!SameSchemeHostPort(a, Create("https", "example.com", "8443")));
    Require(SameSchemeHostPort(Create("http", "example.com", "8080"), Create("http", "example.com", "8080", "/x")));
    Require(!SameOrigin(a, Create("https", "www.example.com")));
    Require(!SameOrigin(a, Create("https", "example.com", "8443")));
    Require(!Equal(a, Create("https", "example.com", "8443")));

    b->domainWasSetInDOM = true;
    b->domain = Make("example.com");
    Require(SameOrigin(a, b));
    Require(SameSchemeHostPort(a, b));
    Require(!Equal(a, b));
    Require(!Equal(b, a));
    a->domainWasSetInDOM = true;
    a->domain = Make("example.com");
    Require(Equal(a, b));
    a->domain = Make("other.com");
    Require(!Equal(a, b));
    Require(SameOrigin(a, b));
    Require(SameSchemeHostPort(a, b));

    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin5equalEPKS0_(a.get(), nullptr); }));
    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin5equalEPKS0_(nullptr, a.get()); }));
    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin14isSameOriginAsERKS0_(a.get(), nullptr); }));
    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin14isSameOriginAsERKS0_(nullptr, a.get()); }));
    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin20isSameSchemeHostPortERKS0_(a.get(), nullptr); }));
    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin20isSameSchemeHostPortERKS0_(nullptr, a.get()); }));
}

static void TestIsolatedCopy() {
    auto origin = Create("https", "example.com", "8443");
    origin->filePath = Make("/path");
    origin->universalAccess = true;
    origin->domainWasSetInDOM = true;
    origin->domain = Make("Domain", true);
    origin->canLoadLocalResources = true;
    origin->enforcesFilePathSeparation = true;
    origin->needsStorageAccessFromFileURLsQuirk = true;
    origin->isPotentiallyTrustworthy = {true, true};
    origin->isLocal = true;
    auto copy = _ZNK7WebCore14SecurityOrigin12isolatedCopyEv(origin.get());
    Require(copy.get() != origin.get());
    Require(copy->refCount.load() == 1);
    Require(origin->refCount.load() == 1);
    Require(copy->data.storage.tuple.protocol.impl != origin->data.storage.tuple.protocol.impl);
    Require(copy->data.storage.tuple.host.impl != origin->data.storage.tuple.host.impl);
    Require(Is(copy->data.storage.tuple.protocol, "https"));
    Require(Is(copy->data.storage.tuple.host, "example.com"));
    Require(copy->data.storage.tuple.port.hasValue && copy->data.storage.tuple.port.value == 8443);
    Require(copy->domain.impl != origin->domain.impl && Is(copy->domain, "Domain") && !Is8Bit(copy->domain));
    Require(copy->filePath.impl != origin->filePath.impl && Is(copy->filePath, "/path"));
    Require(copy->universalAccess && copy->domainWasSetInDOM && copy->canLoadLocalResources);
    Require(copy->enforcesFilePathSeparation && copy->needsStorageAccessFromFileURLsQuirk && copy->isLocal);
    Require(copy->isPotentiallyTrustworthy.hasValue && copy->isPotentiallyTrustworthy.value);

    auto plain = Create("http", "example.com");
    auto plainCopy = _ZNK7WebCore14SecurityOrigin12isolatedCopyEv(plain.get());
    Require(!plainCopy->filePath.impl);
    Require(!plainCopy->isPotentiallyTrustworthy.hasValue);

    OriginRef shared(plain);
    Require(plain->refCount.load() == 2);
    {
        OriginRef third(shared);
        Require(plain->refCount.load() == 3);
    }
    Require(plain->refCount.load() == 2);
    Require(Throws([] { _ZNK7WebCore14SecurityOrigin12isolatedCopyEv(nullptr); }));
}

static void TestCachePartition() {
    auto http = Create("http", "Example.com", "81");
    Require(Is(_ZNK7WebCore14SecurityOrigin23domainForCachePartitionEv(http.get()), "example.com"));
    Require(Is(_ZNK7WebCore14SecurityOrigin23domainForCachePartitionEv(Create("https", "secure.example").get()), "secure.example"));
    auto ws = Create("ws", "socket.example");
    Require(Is(_ZNK7WebCore14SecurityOrigin23domainForCachePartitionEv(ws.get()), ""));
    _ZN7WebCore20LegacySchemeRegistry35registerURLSchemeAsCachePartitionedERKN3WTF6StringE(Make("WS"));
    Require(Is(_ZNK7WebCore14SecurityOrigin23domainForCachePartitionEv(ws.get()), "socket.example"));
    Require(Throws([] { _ZNK7WebCore14SecurityOrigin23domainForCachePartitionEv(nullptr); }));
}

static void TestRegistrableDomainSuffix() {
    auto origin = Create("https", "www.example.com");
    Require(Suffix(origin, "www.example.com"));
    Require(Suffix(origin, "example.com"));
    Require(Suffix(origin, "EXAMPLE.com"));
    Require(!Suffix(origin, "com"));
    Require(!Suffix(origin, "ample.com"));
    Require(!Suffix(origin, "other.com"));
    Require(!Suffix(origin, "www.example.com.evil"));
    Require(!Suffix(origin, ""));
    String null;
    Require(!_ZNK7WebCore14SecurityOrigin33isMatchingRegistrableDomainSuffixERKN3WTF6StringEb(origin.get(), &null, false));

    auto ip = Create("http", "127.0.0.1");
    Require(Suffix(ip, "127.0.0.1"));
    Require(!Suffix(ip, "0.0.1"));
    Require(Suffix(ip, "0.0.1", true));
    Require(!Suffix(ip, "1"));
    auto ipSuffix = Create("http", "a.1.2.3.4");
    Require(!Suffix(ipSuffix, "1.2.3.4"));
    Require(Suffix(ipSuffix, "1.2.3.4", true));

    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin33isMatchingRegistrableDomainSuffixERKN3WTF6StringEb(origin.get(), nullptr, false); }));
    auto value = Make("example.com");
    Require(Throws([&] { _ZNK7WebCore14SecurityOrigin33isMatchingRegistrableDomainSuffixERKN3WTF6StringEb(nullptr, &value, false); }));
}

static void TestOriginData() {
    Data https("https", "example.com", 443);
    Require(Is(_ZNK7WebCore18SecurityOriginData8toStringEv(&https.data), "https://example.com:443"));
    Require(Is(_ZNK7WebCore18SecurityOriginData18databaseIdentifierEv(&https.data), "https_example.com_443"));
    auto origin = _ZNK7WebCore18SecurityOriginData14securityOriginEv(&https.data);
    Require(Is(ToString(origin), "https://example.com"));
    Require(https.data.storage.tuple.port.hasValue);
    Require(origin->data.storage.tuple.host.impl != https.data.storage.tuple.host.impl);
    Require(Is(origin->domain, "example.com"));
    Require(!origin->isLocal && !origin->filePath.impl && !origin->isPotentiallyTrustworthy.hasValue);
    Require(SameOrigin(origin, Create("https", "example.com")));

    Data noPort("http", "a:b/c", std::nullopt);
    Require(Is(_ZNK7WebCore18SecurityOriginData8toStringEv(&noPort.data), "http://a:b/c"));
    Require(Is(_ZNK7WebCore18SecurityOriginData18databaseIdentifierEv(&noPort.data), "http_a%3Ab%2Fc_0"));

    Data mixedCase("HTTP", "Host", 80);
    Require(Is(_ZNK7WebCore18SecurityOriginData8toStringEv(&mixedCase.data), "HTTP://Host:80"));
    Require(Is(_ZNK7WebCore18SecurityOriginData18databaseIdentifierEv(&mixedCase.data), "HTTP_Host_80"));

    Data empty("", "", std::nullopt);
    Require(!_ZNK7WebCore18SecurityOriginData8toStringEv(&empty.data).impl);
    Require(Is(_ZNK7WebCore18SecurityOriginData18databaseIdentifierEv(&empty.data), "__0"));

    Data hostOnly("", "host", std::nullopt);
    Require(Is(_ZNK7WebCore18SecurityOriginData8toStringEv(&hostOnly.data), "://host"));

    Data file("file", "server", 1);
    Require(Is(_ZNK7WebCore18SecurityOriginData8toStringEv(&file.data), "file://"));
    Require(Is(_ZNK7WebCore18SecurityOriginData18databaseIdentifierEv(&file.data), "file__0"));
    auto fileOrigin = _ZNK7WebCore18SecurityOriginData14securityOriginEv(&file.data);
    Require(fileOrigin->isLocal && fileOrigin->canLoadLocalResources);
    Require(Is(fileOrigin->filePath, "/"));
    Require(Is(ToString(fileOrigin), "file://"));

    Data upperFile("FILE", "x", std::nullopt);
    Require(Is(_ZNK7WebCore18SecurityOriginData8toStringEv(&upperFile.data), "FILE://x"));
    Require(Is(_ZNK7WebCore18SecurityOriginData18databaseIdentifierEv(&upperFile.data), "file__0"));
    Require(ThrowsNotImplemented([&] { _ZNK7WebCore18SecurityOriginData14securityOriginEv(&upperFile.data); }));

    _ZN7WebCore20LegacySchemeRegistry24registerURLSchemeAsLocalERKN3WTF6StringE(Make("x-local"));
    Data customLocal("x-local", "h", std::nullopt);
    Require(ThrowsNotImplemented([&] { _ZNK7WebCore18SecurityOriginData14securityOriginEv(&customLocal.data); }));
    auto customLocalURL = Create("x-local", "h");
    Require(IsOpaque(customLocalURL));

    Data opaque(OpaqueOriginIdentifier{42, 7});
    Require(!_ZNK7WebCore18SecurityOriginData8toStringEv(&opaque.data).impl);
    Require(Is(_ZNK7WebCore18SecurityOriginData18databaseIdentifierEv(&opaque.data), "__0"));
    auto opaqueOrigin = _ZNK7WebCore18SecurityOriginData14securityOriginEv(&opaque.data);
    auto opaqueOrigin2 = _ZNK7WebCore18SecurityOriginData14securityOriginEv(&opaque.data);
    Require(IsOpaque(opaqueOrigin));
    Require(opaqueOrigin->data.storage.opaque.object == 42 && opaqueOrigin->data.storage.opaque.processIdentifier == 7);
    Require(Is(opaqueOrigin->domain, ""));
    Require(!opaqueOrigin->isPotentiallyTrustworthy.hasValue);
    Require(SameOrigin(opaqueOrigin, opaqueOrigin2));
    Require(Equal(opaqueOrigin, opaqueOrigin2));
    Data otherProcess(OpaqueOriginIdentifier{42, 8});
    Require(!SameOrigin(opaqueOrigin, _ZNK7WebCore18SecurityOriginData14securityOriginEv(&otherProcess.data)));

    Data valueless(OpaqueOriginIdentifier{1, 1});
    valueless.data.index = std::numeric_limits<std::uint32_t>::max();
    bool threw = false;
    try {
        _ZNK7WebCore18SecurityOriginData8toStringEv(&valueless.data);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw);
    valueless.data.index = NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;

    Require(Throws([] { _ZNK7WebCore18SecurityOriginData8toStringEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore18SecurityOriginData18databaseIdentifierEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore18SecurityOriginData14securityOriginEv(nullptr); }));
    Require(Throws([] { _ZN7WebCore14SecurityOrigin6createERKN3WTF3URLE(nullptr); }));
    Require(Throws([] { _ZNK7WebCore14SecurityOrigin8toStringEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore14SecurityOrigin11toRawStringEv(nullptr); }));
}

int main() {
    TestLoopback();
    TestTupleOrigins();
    TestOpaqueOrigins();
    TestCreateUnique();
    TestFileOrigins();
    TestComparisons();
    TestIsolatedCopy();
    TestCachePartition();
    TestRegistrableDomainSuffix();
    TestOriginData();
    return 0;
}

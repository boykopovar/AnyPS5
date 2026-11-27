#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CORS preflight request check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<typename TException, typename TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

static String Text(std::string_view text, bool wide = false) {
    if (wide) {
        std::u16string units(text.begin(), text.end());
        return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
    }
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static std::string Contents(const StringImpl* impl) {
    std::string text;
    if (!impl) return "<null>";
    const auto view = NKWebKit::WtfAbi::ViewOf(impl);
    for (unsigned index = 0; index < view.length; ++index) {
        const char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, index);
        text.push_back(unit > 0xFF ? '?' : static_cast<char>(unit));
    }
    return text;
}

static URL Parse(std::string_view text) {
    const String input = Text(text);
    const URL base{};
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, nullptr);
    return url;
}

template<class Entry>
static void ReleaseHeaders(NKWebKit::HttpAbi::HeaderVector<Entry>& entries) {
    if (entries.buffer) {
        std::destroy_n(entries.buffer, entries.size);
        _ZN3WTF8fastFreeEPv(entries.buffer);
    }
    entries = {};
}

static HTTPHeaderMap& Map(LegacyResourceRequestBase& request) {
    return *reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
}

struct Request {
    LegacyResourceRequestBase base{};

    explicit Request(std::string_view url = "https://api.example/resource", std::string_view method = "PUT") {
        base.url = Parse(url);
        base.timeoutInterval = 30;
        base.httpMethod = Text(method);
        base.initiatorIdentifier = Text("initiator");
        base.cachePartition = Text("partition");
        base.cachePolicy = 3;
        base.sameSiteDisposition = 2;
        base.priority = 2;
        base.requester = 4;
        base.inspectorInitiatorNodeIdentifier = {true, 7};
        base.allowCookies = true;
        base.resourceRequestUpdated = true;
        base.platformRequestUpdated = true;
        base.platformRequestBodyUpdated = true;
        base.hiddenFromInspector = true;
        base.isTopSite = true;
    }
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    ~Request() {
        ReleaseHeaders(Map(base).common);
        ReleaseHeaders(Map(base).uncommon);
        NKWebKit::WtfAbi::Release(base.url.impl);
    }

    Request& Set(const String& name, std::string_view value) {
        _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&Map(base), name, Text(value));
        return *this;
    }
    Request& Set(std::string_view name, std::string_view value) {
        return Set(Text(name), value);
    }
};

struct Origin {
    SecurityOrigin origin{};

    Origin() {
        new (&origin.data.storage.tuple) SecurityOriginTuple{Text("https"), Text("example.com"), {0, false}};
        origin.data.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
    }
    Origin(const Origin&) = delete;
    Origin& operator=(const Origin&) = delete;
    ~Origin() {
        origin.data.storage.tuple.~SecurityOriginTuple();
    }
};

struct Preflight {
    LegacyResourceRequestBase base;

    Preflight(const Request& request, const String& referrer)
        : base(_ZN7WebCore35createAccessControlPreflightRequestERKNS_15ResourceRequestERNS_14SecurityOriginERKN3WTF6StringE(&request.base, &origin.origin, &referrer)) {
    }
    explicit Preflight(const Request& request) : Preflight(request, String()) {
    }
    Preflight(const Preflight&) = delete;
    Preflight& operator=(const Preflight&) = delete;
    ~Preflight() {
        ReleaseHeaders(Map(base).common);
        ReleaseHeaders(Map(base).uncommon);
        NKWebKit::WtfAbi::Release(base.url.impl);
    }

    std::string Headers() {
        std::string text;
        const auto& map = Map(base);
        for (unsigned index = 0; index < map.common.size; ++index) {
            const auto& header = map.common.buffer[index];
            text += NKWebKit::HttpAbi::headerNames[static_cast<std::size_t>(header.key)];
            text += ": " + Contents(header.value.impl) + "\n";
        }
        Require(map.uncommon.size == 0);
        return text;
    }

    const StringImpl* Value(HTTPHeaderName name) {
        const auto& map = Map(base);
        for (unsigned index = 0; index < map.common.size; ++index) {
            if (map.common.buffer[index].key == name) return map.common.buffer[index].value.impl;
        }
        return nullptr;
    }

    std::string RequestHeaders() {
        const StringImpl* value = Value(HTTPHeaderName::AccessControlRequestHeaders);
        return value ? Contents(value) : std::string("<absent>");
    }

    static inline Origin origin;
};

static constexpr std::string_view kBasic = "Origin: https://example.com\nAccess-Control-Request-Method: PUT\n";

static void TestFields() {
    Request request;
    request.base.resourceRequestUpdated = false;
    const unsigned urlReferences = request.base.url.impl->refCount;
    const unsigned methodReferences = request.base.httpMethod.impl->refCount;
    {
        Preflight preflight(request);
        const auto& base = preflight.base;
        Require(request.base.resourceRequestUpdated);
        Require(base.url.impl == request.base.url.impl && request.base.url.impl->refCount == urlReferences + 2);
        Require(std::memcmp(&base.url, &request.base.url, sizeof(URL)) == 0);
        Require(base.timeoutInterval == 0);
        Require(base.firstPartyForCookies.impl == nullptr && base.firstPartyForCookies.bits == 0);
        Require(Contents(base.httpMethod.impl) == "OPTIONS" && base.httpMethod.impl->refCount == 2 && (base.httpMethod.impl->hashAndFlags & 4));
        Require(base.initiatorIdentifier.impl == nullptr);
        Require(base.cachePartition.impl == NKWebKit::WtfAbi::EmptyStringImpl());
        Require(preflight.Headers() == kBasic);
        Require(preflight.Value(HTTPHeaderName::AccessControlRequestMethod) == request.base.httpMethod.impl);
        Require(request.base.httpMethod.impl->refCount == methodReferences + 2);
        Require(base.responseContentDispositionEncodingFallbackArray.buffer == nullptr && base.responseContentDispositionEncodingFallbackArray.size == 0);
        Require(base.httpBody == nullptr);
        Require(base.cachePolicy == 0 && base.sameSiteDisposition == 0 && base.priority == 2 && base.requester == 0);
        Require(!base.inspectorInitiatorNodeIdentifier.engaged);
        Require(!base.allowCookies && base.resourceRequestUpdated && !base.platformRequestUpdated);
        Require(base.resourceRequestBodyUpdated && !base.platformRequestBodyUpdated && !base.hiddenFromInspector && !base.isTopSite);
    }
    Require(request.base.url.impl->refCount == urlReferences);
    Require(request.base.httpMethod.impl->refCount == methodReferences);
    Require(Contents(request.base.httpMethod.impl) == "PUT" && request.base.timeoutInterval == 30 && request.base.allowCookies);

    for (unsigned priority : {0u, 1u, 3u, 4u, 200u}) {
        Request other;
        other.base.priority = static_cast<std::uint8_t>(priority);
        Preflight preflight(other);
        Require(preflight.base.priority == priority);
    }

    Request nullMethod;
    nullMethod.base.httpMethod = String();
    Preflight preflight(nullMethod);
    Require(preflight.Headers() == "Origin: https://example.com\nAccess-Control-Request-Method: <null>\n");
}

static void TestCredentials() {
    for (const char* url : {"https://user:pass@api.example/x?q#f", "https://user@api.example/x?q#f", "https://:pass@api.example/x?q#f"}) {
        Request request(url);
        const unsigned references = request.base.url.impl->refCount;
        {
            Preflight preflight(request);
            Require(Contents(preflight.base.url.impl) == "https://api.example/x?q#f");
            Require(preflight.base.url.impl != request.base.url.impl);
            Require(preflight.base.url.passwordEnd == preflight.base.url.userStart);
            Require(!preflight.base.platformRequestUpdated);
        }
        Require(Contents(request.base.url.impl) == url && request.base.url.impl->refCount == references);
    }
    Request invalid("");
    Preflight preflight(invalid);
    Require(preflight.base.url.impl == invalid.base.url.impl);
    Require(preflight.Headers() == kBasic);
}

static void TestReferrer() {
    Request request;
    {
        const String referrer = Text("https://example.com/page");
        Preflight preflight(request, referrer);
        Require(preflight.Headers() == std::string(kBasic) + "Referer: https://example.com/page\n");
        Require(preflight.Value(HTTPHeaderName::Referer) == referrer.impl && referrer.impl->refCount == 4);
    }
    {
        const String empty = Text("");
        Preflight preflight(request, empty);
        Require(preflight.Headers() == std::string(kBasic) + "Referer: \n");
    }
    {
        const String longest = Text(std::string(4096, 'a'));
        Preflight preflight(request, longest);
        Require(preflight.Value(HTTPHeaderName::Referer) == longest.impl);
    }
    {
        request.Set("X-Custom", "v");
        const String referrer = Text("r");
        Preflight preflight(request, referrer);
        Require(preflight.Headers() == std::string(kBasic) + "Referer: r\nAccess-Control-Request-Headers: x-custom\n");
    }
    const String tooLong = Text(std::string(4097, 'a'));
    Require(Throws<std::logic_error>([&] { Preflight preflight(request, tooLong); }));
}

static void TestRequestHeaders() {
    {
        Request request;
        request.Set("Accept", "text/html").Set("Accept-Language", "en").Set("Content-Language", "en").Set("Content-Type", "text/plain");
        Preflight preflight(request);
        Require(preflight.Headers() == kBasic);
    }
    {
        Request request;
        request.Set("X-B", "v").Set("x-a", "v").Set("Authorization", "v").Set("Content-Type", "application/json").Set("Accept", "text/html");
        request.Set("X-b2", "v").Set("X", "v").Set("x-A2", "v");
        Preflight preflight(request);
        Require(preflight.RequestHeaders() == "authorization,content-type,x,x-a,x-a2,x-b,x-b2");
        Require(preflight.Headers() == std::string(kBasic) + "Access-Control-Request-Headers: authorization,content-type,x,x-a,x-a2,x-b,x-b2\n");
        const StringImpl* value = preflight.Value(HTTPHeaderName::AccessControlRequestHeaders);
        Require(value->refCount == 2 && (value->hashAndFlags & 4));
    }
    {
        Request request;
        request.Set("Content-Type", "text/plain\t;charset=utf-8").Set("Accept", std::string(129, 'a'));
        Preflight preflight(request);
        Require(preflight.RequestHeaders() == "accept,content-type");
    }
    {
        Request request;
        const String name = Text("x-only");
        request.Set(name, "v");
        const unsigned references = name.impl->refCount;
        Preflight preflight(request);
        Require(preflight.Value(HTTPHeaderName::AccessControlRequestHeaders) == name.impl && name.impl->refCount == references + 2);
    }
    {
        Request request;
        const String name = Text("X-Only");
        request.Set(name, "v");
        const unsigned references = name.impl->refCount;
        Preflight preflight(request);
        Require(preflight.RequestHeaders() == "x-only");
        Require(preflight.Value(HTTPHeaderName::AccessControlRequestHeaders) != name.impl && name.impl->refCount == references);
        Require(preflight.Value(HTTPHeaderName::AccessControlRequestHeaders)->hashAndFlags & 4);
    }
    {
        Request request;
        request.Set(Text("X-Wide", true), "v");
        Preflight single(request);
        Require(single.RequestHeaders() == "x-wide" && !(single.Value(HTTPHeaderName::AccessControlRequestHeaders)->hashAndFlags & 4));
        request.Set("X-Narrow", "v");
        Preflight both(request);
        Require(both.RequestHeaders() == "x-narrow,x-wide" && !(both.Value(HTTPHeaderName::AccessControlRequestHeaders)->hashAndFlags & 4));
    }
    {
        Request request;
        request.Set("X-Empty", "v");
        Map(request.base).uncommon.buffer[0].key = Text("");
        Preflight alone(request);
        Require(alone.RequestHeaders() == "<absent>");
        request.Set("X-A", "v");
        Preflight two(request);
        Require(two.RequestHeaders() == ",x-a");
        Map(request.base).uncommon.buffer[0].key = String();
        Preflight withNull(request);
        Require(withNull.RequestHeaders() == ",x-a");
        Map(request.base).uncommon.buffer[1].key = Text("", true);
        Preflight empties(request);
        Require(empties.RequestHeaders() == "," && (empties.Value(HTTPHeaderName::AccessControlRequestHeaders)->hashAndFlags & 4));
    }
    {
        Request request;
        request.Set("x-b", "v").Set("x-B", "v2").Set("X-a", "v");
        Preflight preflight(request);
        Require(preflight.RequestHeaders() == "x-a,x-b");
    }
    {
        Request request;
        request.Set("b\x7f", "v");
        request.Set(Text("b\xe9"), "v");
        request.Set("B", "v").Set("a", "v").Set("b-", "v").Set("b0", "v").Set("bA", "v");
        Preflight preflight(request);
        Require(preflight.RequestHeaders() == "a,b,b-,b0,ba,b\x7f,b\xe9");
    }
}

static void TestArguments() {
    Request request;
    const String referrer;
    Require(Throws<std::invalid_argument>([&] {
        _ZN7WebCore35createAccessControlPreflightRequestERKNS_15ResourceRequestERNS_14SecurityOriginERKN3WTF6StringE(nullptr, &Preflight::origin.origin, &referrer);
    }));
    Require(Throws<std::invalid_argument>([&] {
        _ZN7WebCore35createAccessControlPreflightRequestERKNS_15ResourceRequestERNS_14SecurityOriginERKN3WTF6StringE(&request.base, nullptr, &referrer);
    }));
    Require(Throws<std::invalid_argument>([&] {
        _ZN7WebCore35createAccessControlPreflightRequestERKNS_15ResourceRequestERNS_14SecurityOriginERKN3WTF6StringE(&request.base, &Preflight::origin.origin, nullptr);
    }));
    request.Set("Authorization", "v").Set("X-Custom", "v");
    auto& map = Map(request.base);
    map.common.buffer[0].key = static_cast<HTTPHeaderName>(NKWebKit::HttpAbi::headerNames.size());
    Require(Throws<std::invalid_argument>([&] { Preflight preflight(request); }));
    map.common.buffer[0].key = HTTPHeaderName::Authorization;
    const auto commonCapacity = map.common.capacity;
    map.common.capacity = 0;
    Require(Throws<std::invalid_argument>([&] { Preflight preflight(request); }));
    map.common.capacity = commonCapacity;
    const auto uncommonCapacity = map.uncommon.capacity;
    map.uncommon.capacity = 0;
    Require(Throws<std::invalid_argument>([&] { Preflight preflight(request); }));
    map.uncommon.capacity = uncommonCapacity;
    Preflight preflight(request);
    Require(preflight.RequestHeaders() == "authorization,x-custom");
}

int main() {
    TestFields();
    TestCredentials();
    TestReferrer();
    TestRequestHeaders();
    TestArguments();
    std::puts("libSceNKWebKit CORS preflight request tests passed");
    return 0;
}

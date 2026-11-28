#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::FormData;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceResponseStatusLayout;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "redirected request check failed at line %d\n", line);
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

static String Text(std::string_view text) {
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

static void ReleaseRequest(LegacyResourceRequestBase& request) {
    ReleaseHeaders(Map(request).common);
    ReleaseHeaders(Map(request).uncommon);
    auto& fallback = request.responseContentDispositionEncodingFallbackArray;
    if (fallback.buffer) {
        std::destroy_n(static_cast<String*>(fallback.buffer), fallback.size);
        _ZN3WTF8fastFreeEPv(fallback.buffer);
    }
    fallback = {};
    NKWebKit::WtfAbi::Release(request.url.impl);
    NKWebKit::WtfAbi::Release(request.firstPartyForCookies.impl);
    request.url = {};
    request.firstPartyForCookies = {};
    if (auto* body = static_cast<FormData*>(std::exchange(request.httpBody, nullptr))) body->deref();
}

struct Request {
    LegacyResourceRequestBase base{};
    FormData body{};

    explicit Request(std::string_view method = "GET", std::string_view url = "https://a.example/old") {
        base.url = Parse(url);
        base.httpMethod = Text(method);
        base.resourceRequestUpdated = true;
        body.refCount = 1;
    }
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    ~Request() {
        base.httpBody = nullptr;
        ReleaseRequest(base);
        Check(body.refCount == 1, __LINE__);
    }

    Request& Set(std::string_view name, std::string_view value) {
        _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&Map(base), Text(name), Text(value));
        return *this;
    }
    Request& WithBody() {
        base.httpBody = &body;
        return *this;
    }
};

struct Response {
    ResourceResponseStatusLayout layout{};

    explicit Response(int status, std::optional<std::string_view> location = "/new", std::string_view url = "https://a.example/old") {
        layout.head.url = Parse(url);
        layout.httpStatusCode = static_cast<short>(status);
        if (location)
            _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&layout.head, HTTPHeaderName::Location, Text(*location));
    }
    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;
    ~Response() {
        ReleaseHeaders(layout.head.httpHeaderFields.common);
        ReleaseHeaders(layout.head.httpHeaderFields.uncommon);
        NKWebKit::WtfAbi::Release(layout.head.url.impl);
    }
};

struct Redirected {
    LegacyResourceRequestBase base;

    Redirected(const Request& request, const Response& response, bool clearReferrer = false)
        : base(_ZNK7WebCore19ResourceRequestBase17redirectedRequestERKNS_16ResourceResponseEb(&request.base, &response.layout.head, clearReferrer)) {
    }
    Redirected(const Redirected&) = delete;
    Redirected& operator=(const Redirected&) = delete;
    ~Redirected() {
        ReleaseRequest(base);
    }

    std::string Headers() {
        std::string text;
        const auto& map = Map(base);
        for (unsigned index = 0; index < map.common.size; ++index) {
            const auto& header = map.common.buffer[index];
            text += NKWebKit::HttpAbi::headerNames[static_cast<std::size_t>(header.key)];
            text += "=" + Contents(header.value.impl) + ";";
        }
        for (unsigned index = 0; index < map.uncommon.size; ++index)
            text += Contents(map.uncommon.buffer[index].key.impl) + "=" + Contents(map.uncommon.buffer[index].value.impl) + ";";
        return text;
    }
};

static void TestCopy() {
    Request request("PUT");
    request.WithBody().Set("Accept", "a").Set("X-Custom", "c").Set("Content-Type", "t");
    auto& source = request.base;
    source.timeoutInterval = 12.5;
    source.firstPartyForCookies = Parse("https://first.example/");
    source.initiatorIdentifier = Text("initiator");
    source.cachePartition = Text("partition");
    auto* fallback = static_cast<String*>(_ZN3WTF10fastMallocEm(4 * sizeof(String)));
    new (&fallback[0]) String(Text("utf-8"));
    new (&fallback[1]) String(Text("latin1"));
    source.responseContentDispositionEncodingFallbackArray = {fallback, 4, 2};
    source.cachePolicy = 3;
    source.sameSiteDisposition = 2;
    source.priority = 4;
    source.requester = 2;
    source.inspectorInitiatorNodeIdentifier = {true, 9};
    source.allowCookies = true;
    source.resourceRequestUpdated = false;
    source.platformRequestUpdated = true;
    source.resourceRequestBodyUpdated = true;
    source.platformRequestBodyUpdated = true;
    source.hiddenFromInspector = true;
    source.isTopSite = true;
    const unsigned urlReferences = source.url.impl->refCount;
    const unsigned firstPartyReferences = source.firstPartyForCookies.impl->refCount;
    const unsigned methodReferences = source.httpMethod.impl->refCount;
    const unsigned valueReferences = Map(source).common.buffer[0].value.impl->refCount;
    const unsigned keyReferences = Map(source).uncommon.buffer[0].key.impl->refCount;
    const unsigned fallbackReferences = fallback[0].impl->refCount;
    Response response(307);
    {
        Redirected redirected(request, response);
        auto& copy = redirected.base;
        Require(source.resourceRequestUpdated && copy.resourceRequestUpdated);
        Require(Contents(copy.url.impl) == "https://a.example/new" && source.url.impl->refCount == urlReferences);
        Require(Contents(source.url.impl) == "https://a.example/old");
        Require(copy.timeoutInterval == 12.5);
        Require(copy.firstPartyForCookies.impl == source.firstPartyForCookies.impl);
        Require(std::memcmp(&copy.firstPartyForCookies, &source.firstPartyForCookies, sizeof(URL)) == 0);
        Require(source.firstPartyForCookies.impl->refCount == firstPartyReferences + 2);
        Require(copy.httpMethod.impl == source.httpMethod.impl && source.httpMethod.impl->refCount == methodReferences + 2);
        Require(copy.initiatorIdentifier.impl == source.initiatorIdentifier.impl);
        Require(copy.cachePartition.impl == source.cachePartition.impl);
        Require(redirected.Headers() == "Accept=a;Content-Type=t;X-Custom=c;");
        Require(Map(copy).common.buffer != Map(source).common.buffer && Map(copy).common.capacity == Map(source).common.capacity);
        Require(Map(copy).uncommon.buffer != Map(source).uncommon.buffer && Map(copy).uncommon.capacity == Map(source).uncommon.capacity);
        Require(Map(copy).common.buffer[0].value.impl == Map(source).common.buffer[0].value.impl);
        Require(Map(source).common.buffer[0].value.impl->refCount == valueReferences + 2);
        Require(Map(source).uncommon.buffer[0].key.impl->refCount == keyReferences + 2);
        const auto& copiedFallback = copy.responseContentDispositionEncodingFallbackArray;
        Require(copiedFallback.buffer != fallback && copiedFallback.capacity == 4 && copiedFallback.size == 2);
        Require(static_cast<String*>(copiedFallback.buffer)[0].impl == fallback[0].impl && fallback[0].impl->refCount == fallbackReferences + 2);
        Require(static_cast<String*>(copiedFallback.buffer)[1].impl == fallback[1].impl);
        Require(copy.httpBody == &request.body && request.body.refCount == 2);
        Require(copy.cachePolicy == 3 && copy.sameSiteDisposition == 2 && copy.priority == 4 && copy.requester == 2);
        Require(copy.inspectorInitiatorNodeIdentifier.engaged && copy.inspectorInitiatorNodeIdentifier.value == 9);
        Require(copy.allowCookies && !copy.platformRequestUpdated && copy.resourceRequestBodyUpdated);
        Require(copy.platformRequestBodyUpdated && copy.hiddenFromInspector && copy.isTopSite);
    }
    Require(source.url.impl->refCount == urlReferences && source.firstPartyForCookies.impl->refCount == firstPartyReferences);
    Require(source.httpMethod.impl->refCount == methodReferences && request.body.refCount == 1);
    Require(Map(source).common.buffer[0].value.impl->refCount == valueReferences);
    Require(Map(source).uncommon.buffer[0].key.impl->refCount == keyReferences && fallback[0].impl->refCount == fallbackReferences);

    Request empty;
    Redirected plain(empty, response);
    Require(Map(plain.base).common.buffer == nullptr && Map(plain.base).uncommon.buffer == nullptr);
    Require(plain.base.responseContentDispositionEncodingFallbackArray.buffer == nullptr && plain.base.httpBody == nullptr);
    Require(plain.base.firstPartyForCookies.impl == nullptr && !plain.base.allowCookies);
}

static std::string TargetOf(std::optional<std::string_view> location, std::string_view responseURL = "https://a.example/dir/old?q") {
    Request request;
    Response response(307, location, responseURL);
    Redirected redirected(request, response);
    return Contents(redirected.base.url.impl);
}

static void TestLocation() {
    Require(TargetOf("/new") == "https://a.example/new");
    Require(TargetOf("next?x#f") == "https://a.example/dir/next?x#f");
    Require(TargetOf("?only") == "https://a.example/dir/old?only");
    Require(TargetOf("//b.example/z") == "https://b.example/z");
    Require(TargetOf("http://c.example:8080/p") == "http://c.example:8080/p");
    Require(TargetOf(std::nullopt) == "<null>");
    Require(TargetOf("") == "<null>");
    Request request;
    Response response(307, std::nullopt);
    Redirected redirected(request, response);
    Require(redirected.base.url.bits == 0 && redirected.base.url.hostEnd == 0 && !redirected.base.platformRequestUpdated);
}

static void TestMethod() {
    for (int status : {200, 300, 301, 302, 303, 304, 307, 308}) {
        for (const char* method : {"GET", "get", "HEAD", "Head", "POST", "post", "pOsT", "PUT", "DELETE", "GETS", "POSTS", "", static_cast<const char*>(nullptr)}) {
            Request request(method ? method : "");
            if (!method) request.base.httpMethod = String();
            request.WithBody();
            request.base.resourceRequestBodyUpdated = false;
            request.base.platformRequestBodyUpdated = true;
            request.Set("Content-Length", "1").Set("Content-Language", "en").Set("Content-Encoding", "gzip").Set("Content-Location", "/l");
            request.Set("Content-Type", "t").Set("Accept", "a").Set("X-Custom", "c");
            const std::string lower = [&] {
                std::string text = method ? method : "?";
                for (char& c : text) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
                return text;
            }();
            const bool usesGet = lower != "get" && lower != "head" && (status == 303 || ((status == 301 || status == 302) && lower == "post"));
            Response response(status);
            Redirected redirected(request, response);
            const auto& copy = redirected.base;
            if (usesGet) {
                Require(Contents(copy.httpMethod.impl) == "GET" && copy.httpMethod.impl != request.base.httpMethod.impl);
                Require(copy.httpBody == nullptr && request.body.refCount == 1);
                Require(copy.resourceRequestBodyUpdated && !copy.platformRequestBodyUpdated);
                Require(redirected.Headers() == "Accept=a;X-Custom=c;");
            } else {
                Require(copy.httpMethod.impl == request.base.httpMethod.impl);
                Require(copy.httpBody == &request.body && request.body.refCount == 2);
                Require(!copy.resourceRequestBodyUpdated && copy.platformRequestBodyUpdated);
                Require(redirected.Headers() == "Content-Length=1;Content-Language=en;Content-Encoding=gzip;Content-Location=/l;Content-Type=t;Accept=a;X-Custom=c;");
            }
        }
    }
    Request withoutBody("POST");
    Response response(303);
    Redirected redirected(withoutBody, response);
    Require(Contents(redirected.base.httpMethod.impl) == "GET" && redirected.base.httpBody == nullptr);
}

static bool KeepsReferrer(std::string_view referrer, std::string_view location, bool clearReferrer) {
    Request request;
    request.Set("Referer", referrer);
    Response response(307, location);
    Redirected redirected(request, response, clearReferrer);
    const std::string headers = redirected.Headers();
    Require(headers.empty() || headers == "Referer=" + std::string(referrer) + ";");
    return !headers.empty();
}

static void TestReferrer() {
    Require(!KeepsReferrer("https://r.example/p", "http://b.example/", true));
    Require(KeepsReferrer("https://r.example/p", "http://b.example/", false));
    Require(KeepsReferrer("https://r.example/p", "https://b.example/", true));
    Require(KeepsReferrer("https://r.example/p", "HTTPS://b.example/", true));
    Require(KeepsReferrer("http://r.example/p", "http://b.example/", true));
    Require(!KeepsReferrer("HTTPS://r.example/p", "http://b.example/", true));
    Require(!KeepsReferrer(" \t\x01https://r.example/p", "http://b.example/", true));
    Require(!KeepsReferrer("ht\ttp\rs\n://r.example/p", "http://b.example/", true));
    Require(KeepsReferrer("ht tps://r.example/p", "http://b.example/", true));
    Require(KeepsReferrer("https", "http://b.example/", true));
    Require(KeepsReferrer("https/", "http://b.example/", true));
    Require(KeepsReferrer("httpsx://r.example/", "http://b.example/", true));
    Require(KeepsReferrer("http", "http://b.example/", true));
    Require(!KeepsReferrer("https:", "http://b.example/", true));
    Require(!KeepsReferrer("https://r.example/p", "wss://b.example/", true));
    Require(!KeepsReferrer("https://r.example/p", "", true));
    Require(KeepsReferrer("https://r.example/p", "", false));

    Request request;
    Response response(307, "http://b.example/");
    Redirected redirected(request, response, true);
    Require(redirected.Headers().empty());
}

static bool KeepsOrigin(std::string_view location, std::string_view responseURL = "https://a.example/old") {
    Request request;
    request.Set("Origin", "https://o.example");
    Response response(307, location, responseURL);
    Redirected redirected(request, response);
    const std::string headers = redirected.Headers();
    Require(headers.empty() || headers == "Origin=https://o.example;");
    return !headers.empty();
}

static void TestOriginAndAuthorization() {
    Require(KeepsOrigin("/new"));
    Require(KeepsOrigin("https://a.example/other?q"));
    Require(KeepsOrigin("https://A.EXAMPLE:443/other"));
    Require(!KeepsOrigin("https://b.example/"));
    Require(!KeepsOrigin("https://a.example:8443/"));
    Require(!KeepsOrigin("http://a.example/"));
    Require(!KeepsOrigin("https://sub.a.example/"));
    Require(!KeepsOrigin(""));
    Require(KeepsOrigin("/new", "https://a.example:8443/old"));
    Require(!KeepsOrigin("https://a.example/new", "https://a.example:8443/old"));

    Request request;
    request.Set("Authorization", "Basic x").Set("Proxy-Authorization", "Basic y").Set("X-Authorization", "kept").Set("Accept", "a");
    Response response(307);
    Redirected redirected(request, response);
    Require(redirected.Headers() == "Accept=a;X-Authorization=kept;");
}

static void TestArguments() {
    Request request;
    request.Set("Accept", "a").Set("X-Custom", "c");
    Response response(307);
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase17redirectedRequestERKNS_16ResourceResponseEb(nullptr, &response.layout.head, false); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase17redirectedRequestERKNS_16ResourceResponseEb(&request.base, nullptr, false); }));
    auto& map = Map(request.base);
    const auto commonCapacity = map.common.capacity;
    map.common.capacity = 0;
    Require(Throws([&] { Redirected redirected(request, response); }));
    map.common.capacity = commonCapacity;
    const auto uncommonCapacity = map.uncommon.capacity;
    map.uncommon.capacity = 0;
    Require(Throws([&] { Redirected redirected(request, response); }));
    map.uncommon.capacity = uncommonCapacity;
    request.base.responseContentDispositionEncodingFallbackArray = {nullptr, 1, 1};
    Require(Throws([&] { Redirected redirected(request, response); }));
    request.base.responseContentDispositionEncodingFallbackArray = {nullptr, 0, 1};
    Require(Throws([&] { Redirected redirected(request, response); }));
    request.base.responseContentDispositionEncodingFallbackArray = {};
    Redirected redirected(request, response);
    Require(redirected.Headers() == "Accept=a;X-Custom=c;");
}

int main() {
    TestCopy();
    TestLocation();
    TestMethod();
    TestReferrer();
    TestOriginAndAuthorization();
    TestArguments();
    std::puts("libSceNKWebKit redirected request tests passed");
    return 0;
}

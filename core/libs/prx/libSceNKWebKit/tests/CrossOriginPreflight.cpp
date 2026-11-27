#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisabler;
using NKWebKit::WebCoreAbi::CrossOriginAccessControlCheckDisablerVtable;
using NKWebKit::WebCoreAbi::CrossOriginPreflightResultCache;
using NKWebKit::WebCoreAbi::CrossOriginPreflightResultCacheItem;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceResponseStatusLayout;
using NKWebKit::WebCoreAbi::SecurityOrigin;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WebCoreAbi::StoredCredentialsPolicy;
using NKWebKit::WtfAbi::ExpectedVoidString;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CORS preflight check failed at line %d\n", line);
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

static constexpr auto doNotUse = StoredCredentialsPolicy::DoNotUse;
static constexpr auto use = StoredCredentialsPolicy::Use;
static constexpr auto ephemeralStateless = StoredCredentialsPolicy::EphemeralStateless;

static constexpr std::string_view kOrigin = "https://example.com";
static constexpr std::string_view kURL = "https://api.example/resource";

static String Text(std::string_view text, bool wide = false) {
    if (wide) {
        std::u16string units(text.begin(), text.end());
        return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
    }
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

static String Wide(std::u16string_view units) {
    return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
}

static std::string Contents(const StringImpl* impl) {
    std::string text;
    if (!impl) return text;
    const auto view = NKWebKit::WtfAbi::ViewOf(impl);
    for (unsigned index = 0; index < view.length; ++index) {
        const char16_t unit = NKWebKit::WtfAbi::CodeUnitAt(view, index);
        text.push_back(unit > 0xFF ? '?' : static_cast<char>(unit));
    }
    return text;
}

template<class Entry>
static void ReleaseHeaders(NKWebKit::HttpAbi::HeaderVector<Entry>& entries) {
    if (entries.buffer) {
        std::destroy_n(entries.buffer, entries.size);
        _ZN3WTF8fastFreeEPv(entries.buffer);
    }
    entries = {};
}

struct Headers {
    HTTPHeaderMap map{};

    Headers() = default;
    Headers(const Headers&) = delete;
    Headers& operator=(const Headers&) = delete;
    ~Headers() {
        ReleaseHeaders(map.common);
        ReleaseHeaders(map.uncommon);
    }

    Headers& Set(std::string_view name, std::string_view value, bool wideName = false) {
        _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&map, Text(name, wideName), Text(value));
        return *this;
    }
};

struct Request {
    LegacyResourceRequestBase base{};
    String urlText;

    explicit Request(std::string_view method = "GET", std::string_view url = kURL) : urlText(Text(url)) {
        base.url.impl = urlText.impl;
        base.httpMethod = Text(method);
        base.resourceRequestUpdated = true;
    }
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    ~Request() {
        ReleaseHeaders(Map().common);
        ReleaseHeaders(Map().uncommon);
    }

    HTTPHeaderMap& Map() {
        return *reinterpret_cast<HTTPHeaderMap*>(&base.httpHeaderFields);
    }

    Request& Set(std::string_view name, std::string_view value, bool wideName = false) {
        _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&Map(), Text(name, wideName), Text(value));
        return *this;
    }
};

struct Response {
    ResourceResponseStatusLayout layout{};

    explicit Response(std::string_view allowOrigin = kOrigin, int status = 200) {
        layout.httpStatusCode = static_cast<short>(status);
        Set(HTTPHeaderName::AccessControlAllowOrigin, allowOrigin);
    }
    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;
    ~Response() {
        ReleaseHeaders(layout.head.httpHeaderFields.common);
        ReleaseHeaders(layout.head.httpHeaderFields.uncommon);
    }

    Response& Set(HTTPHeaderName name, std::string_view value) {
        _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&layout.head, name, Text(value));
        return *this;
    }
    Response& Methods(std::string_view value) {
        return Set(HTTPHeaderName::AccessControlAllowMethods, value);
    }
    Response& AllowHeaders(std::string_view value) {
        return Set(HTTPHeaderName::AccessControlAllowHeaders, value);
    }
    Response& MaxAge(std::string_view value) {
        return Set(HTTPHeaderName::AccessControlMaxAge, value);
    }
    Response& Credentials() {
        return Set(HTTPHeaderName::AccessControlAllowCredentials, "true");
    }
};

struct Origin {
    SecurityOrigin origin{};

    Origin(std::string_view protocol = "https", std::string_view host = "example.com") {
        new (&origin.data.storage.tuple) SecurityOriginTuple{Text(protocol), Text(host), {0, false}};
        origin.data.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
    }
    Origin(const Origin&) = delete;
    Origin& operator=(const Origin&) = delete;
    ~Origin() {
        origin.data.storage.tuple.~SecurityOriginTuple();
    }
};

static bool enabledAnswer = true;

static void APS5_VABI IgnoreDestructor(CrossOriginAccessControlCheckDisabler*) {
}

static bool APS5_VABI Enabled(const CrossOriginAccessControlCheckDisabler*) {
    return enabledAnswer;
}

static const CrossOriginAccessControlCheckDisablerVtable disablerVtable{&IgnoreDestructor, &IgnoreDestructor, &Enabled};

static CrossOriginPreflightResultCache* Cache() {
    return &_ZN7WebCore31CrossOriginPreflightResultCache9singletonEv();
}

static void Clear() {
    _ZN7WebCore31CrossOriginPreflightResultCache5clearEv(Cache());
}

static ExpectedVoidString Validate(const Request& request, const Response& response, StoredCredentialsPolicy policy = doNotUse,
                                   const CrossOriginAccessControlCheckDisabler* disabler = nullptr) {
    Origin origin;
    return _ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
        &request.base, &response.layout.head, policy, &origin.origin, disabler);
}

static bool Passes(const ExpectedVoidString& result) {
    return result.hasValue;
}

static bool Fails(const ExpectedVoidString& result, std::string_view message) {
    return !result.hasValue && result.error && result.error->refCount == 2 && Contents(result.error) == message;
}

static bool CanSkip(const String& origin, const String& urlText, StoredCredentialsPolicy policy, const String& method, const HTTPHeaderMap& headers) {
    URL url{};
    url.impl = urlText.impl;
    return _ZN7WebCore31CrossOriginPreflightResultCache16canSkipPreflightERKN3WTF6StringERKNS1_3URLENS_23StoredCredentialsPolicyES4_RKNS_13HTTPHeaderMapE(
        Cache(), &origin, &url, policy, &method, &headers);
}

static bool CanSkip(std::string_view method = "GET", StoredCredentialsPolicy policy = doNotUse, const HTTPHeaderMap& headers = HTTPHeaderMap{},
                    std::string_view origin = kOrigin, std::string_view url = kURL) {
    return CanSkip(Text(origin), Text(url), policy, Text(method), headers);
}

static StringImpl* const deletedBucket = reinterpret_cast<StringImpl*>(static_cast<std::intptr_t>(-1));

static StringImpl** MakeTable(std::vector<String>& watched, std::initializer_list<std::string_view> entries, bool wide = false) {
    constexpr unsigned size = 8;
    auto* metadata = static_cast<unsigned*>(_ZN3WTF10fastMallocEm(4 * sizeof(unsigned) + size * sizeof(StringImpl*)));
    metadata[0] = 1;
    metadata[1] = static_cast<unsigned>(entries.size());
    metadata[2] = size - 1;
    metadata[3] = size;
    auto** table = reinterpret_cast<StringImpl**>(metadata + 4);
    std::fill_n(table, size, nullptr);
    table[1] = deletedBucket;
    unsigned index = 2;
    for (const auto entry : entries) {
        String text = Text(entry, wide);
        watched.push_back(text);
        table[index++] = std::exchange(text.impl, nullptr);
    }
    return table;
}

static double Now() {
    return _ZN3WTF13MonotonicTime3nowEv().m_value;
}

static void AppendGuest(double expiry, StoredCredentialsPolicy policy, StringImpl** methods, StringImpl** headers, const String& origin, const String& urlText) {
    auto* item = static_cast<CrossOriginPreflightResultCacheItem*>(_ZN3WTF10fastMallocEm(sizeof(CrossOriginPreflightResultCacheItem)));
    *item = CrossOriginPreflightResultCacheItem{expiry, policy, methods, headers};
    URL url{};
    url.impl = urlText.impl;
    _ZN7WebCore31CrossOriginPreflightResultCache11appendEntryERKN3WTF6StringERKNS1_3URLESt10unique_ptrINS_35CrossOriginPreflightResultCacheItemESt14default_deleteIS9_EE(
        Cache(), &origin, &url, &item);
    Require(item == nullptr);
}

static void AppendGuest(double expiry, StoredCredentialsPolicy policy, StringImpl** methods = nullptr, StringImpl** headers = nullptr) {
    AppendGuest(expiry, policy, methods, headers, Text(kOrigin), Text(kURL));
}

static void TestSingleton() {
    Require(Cache() == &_ZN7WebCore31CrossOriginPreflightResultCache9singletonEv());
    CrossOriginPreflightResultCache other{};
    const String origin = Text(kOrigin);
    const String method = Text("GET");
    URL url{};
    url.impl = origin.impl;
    const HTTPHeaderMap headers{};
    CrossOriginPreflightResultCacheItem* none = nullptr;
    Require(Throws([&] { _ZN7WebCore31CrossOriginPreflightResultCache5clearEv(&other); }));
    Require(Throws([&] { _ZN7WebCore31CrossOriginPreflightResultCache5clearEv(nullptr); }));
    Require(Throws([&] {
        _ZN7WebCore31CrossOriginPreflightResultCache11appendEntryERKN3WTF6StringERKNS1_3URLESt10unique_ptrINS_35CrossOriginPreflightResultCacheItemESt14default_deleteIS9_EE(
            &other, &origin, &url, &none);
    }));
    Require(Throws([&] {
        _ZN7WebCore31CrossOriginPreflightResultCache16canSkipPreflightERKN3WTF6StringERKNS1_3URLENS_23StoredCredentialsPolicyES4_RKNS_13HTTPHeaderMapE(
            &other, &origin, &url, doNotUse, &method, &headers);
    }));
}

static void TestLifetime() {
    using NKWebKit::PreflightCache::LifetimeSeconds;
    Require(LifetimeSeconds(String()) == 5);
    for (const char* invalid : {"", " ", "-12", "+ 12", "+-12", "12 3", "12a", "a12", "1.5", "0x10", "+", "4294967296", "4294967300", "42949672950", "99999999999999999999"})
        Require(LifetimeSeconds(Text(invalid)) == 5);
    Require(LifetimeSeconds(Text("0")) == 0);
    Require(LifetimeSeconds(Text("1")) == 1);
    Require(LifetimeSeconds(Text("599")) == 599);
    Require(LifetimeSeconds(Text("600")) == 600);
    Require(LifetimeSeconds(Text("601")) == 600);
    Require(LifetimeSeconds(Text("4294967295")) == 600);
    Require(LifetimeSeconds(Text("4294967289")) == 600);
    Require(LifetimeSeconds(Text(" 12 ")) == 12);
    Require(LifetimeSeconds(Text("\t\n\v\f\r12\t\n\v\f\r")) == 12);
    Require(LifetimeSeconds(Text("+12")) == 12);
    Require(LifetimeSeconds(Text("0012")) == 12);
    Require(LifetimeSeconds(Text("12", true)) == 12);
    Require(LifetimeSeconds(Wide(u"　 12　")) == 12);
    Require(LifetimeSeconds(Wide(u" 12")) == 5);
    Require(LifetimeSeconds(Wide(u"\u008512")) == 5);
    Require(LifetimeSeconds(Wide(u"12١")) == 5);
    Require(LifetimeSeconds(Text("\x1f" "12")) == 5);
}

static void TestStatusAndAccessCheck() {
    Clear();
    for (int status : {0, 100, 199, 300, 304, 404, 500, -1}) {
        Request request;
        request.base.resourceRequestUpdated = false;
        Response response(kOrigin, status);
        Require(Fails(Validate(request, response), "Preflight response is not successful"));
        Require(!request.base.resourceRequestUpdated);
    }
    Require(!CanSkip());
    for (int status : {200, 204, 299}) {
        Clear();
        Request request;
        Response response(kOrigin, status);
        Require(Passes(Validate(request, response)));
        Require(CanSkip());
    }

    Clear();
    {
        Request request;
        Response response("https://other.example", 204);
        Require(Fails(Validate(request, response), "Origin https://example.com is not allowed by Access-Control-Allow-Origin."));
        Response several("https://example.com, https://other.example");
        Require(Fails(Validate(request, several), "Access-Control-Allow-Origin cannot contain more than one origin."));
        Response star("*");
        Require(Passes(Validate(request, star)));
        Clear();
        Require(Fails(Validate(request, star, use), "Cannot use wildcard in Access-Control-Allow-Origin when credentials flag is true."));
        Response exact;
        Require(Fails(Validate(request, exact, use), "Credentials flag is true, but Access-Control-Allow-Credentials is not \"true\"."));
        Require(!CanSkip());
        exact.Credentials();
        Require(Passes(Validate(request, exact, use)));
        Require(CanSkip("GET", use));
    }

    Clear();
    {
        Request request;
        Response star("*");
        enabledAnswer = false;
        const CrossOriginAccessControlCheckDisabler disabled{&disablerVtable, true};
        Require(Passes(Validate(request, star, use, &disabled)));
        Require(CanSkip("GET", use));
        enabledAnswer = true;
        Require(Fails(Validate(request, star, use, &disabled), "Cannot use wildcard in Access-Control-Allow-Origin when credentials flag is true."));
    }
}

static void TestAllowLists() {
    Clear();
    {
        Request request;
        Response response;
        response.Methods("GET, P UT").AllowHeaders("X A");
        Require(Fails(Validate(request, response), "Header Access-Control-Allow-Methods has an invalid value: GET, P UT"));
        response.Methods("PUT").AllowHeaders("X-A, X B");
        Require(Fails(Validate(request, response), "Header Access-Control-Allow-Headers has an invalid value: X-A, X B"));
        Require(!CanSkip());
        request.base.resourceRequestUpdated = false;
        response.AllowHeaders("X-A");
        Require(Passes(Validate(request, response)));
        Require(request.base.resourceRequestUpdated);
    }
    for (const char* invalid : {"PU(T", "PUT;", "GET,\"PUT\"", "PUT\x7f", "P\x01T", "PUT, ,D:E"}) {
        Request request;
        Response response;
        response.Methods(invalid);
        Require(Fails(Validate(request, response), std::string("Header Access-Control-Allow-Methods has an invalid value: ") + invalid));
    }
    for (const char* valid : {"PUT, ", "PUT,\t", "PUT,  \t ", " ,PUT, ", "PUT, ,", "\t", "", " ", ",", ",,", " , ,", "PUT", " PUT ", "\tPUT\r\n", "PUT,", ",PUT", "DELETE , PUT", "DELETE,,PUT", "!#$%&'*+-.^_`|~,PUT", "a, PUT"}) {
        Clear();
        Request request("PUT");
        Response response;
        response.Methods(valid);
        const bool listed = std::string_view(valid).find("PUT") != std::string_view::npos;
        if (listed) Require(Passes(Validate(request, response)));
        else Require(Fails(Validate(request, response), "Method PUT is not allowed by Access-Control-Allow-Methods."));
        Require(CanSkip("GET"));
        Require(CanSkip("PUT") == listed);
    }
}

static void TestMethods() {
    Clear();
    {
        Request request("put");
        Response response;
        response.Methods("PUT");
        Require(Fails(Validate(request, response), "Method put is not allowed by Access-Control-Allow-Methods."));
        Require(CanSkip("PUT"));
        Require(!CanSkip("put"));
        Require(!CanSkip("PUT"));
    }
    for (const char* simple : {"GET", "HEAD", "POST"}) {
        Clear();
        Request request(simple);
        Response response;
        Require(Passes(Validate(request, response)));
        Require(CanSkip(simple));
    }
    for (const char* other : {"get", "Post", "OPTIONS", "DELETE", ""}) {
        Clear();
        Request request(other);
        Response response;
        Require(Fails(Validate(request, response), std::string("Method ") + other + " is not allowed by Access-Control-Allow-Methods."));
        Require(!CanSkip(other));
        Require(!CanSkip("GET"));
    }

    Clear();
    {
        Request request("DELETE");
        Response star;
        star.Methods("*");
        Require(Passes(Validate(request, star)));
        Require(CanSkip("PATCH"));
        Require(!CanSkip("PATCH", use));
        Clear();
        Require(Passes(Validate(request, star, ephemeralStateless)));
        Require(CanSkip("PATCH", ephemeralStateless));
        Clear();
        star.Credentials();
        Require(Fails(Validate(request, star, use), "Method DELETE is not allowed by Access-Control-Allow-Methods."));
        Require(CanSkip("PATCH"));
        star.Methods("*, DELETE");
        Require(Passes(Validate(request, star, use)));
        Require(CanSkip("DELETE", use));
        Require(!CanSkip("PATCH", use));
    }

    Clear();
    {
        Request request;
        request.base.httpMethod = String();
        Response response;
        Require(Fails(Validate(request, response), "Method  is not allowed by Access-Control-Allow-Methods."));
        response.Methods("PUT");
        Require(Throws([&] { Validate(request, response); }));
    }
}

static void TestHeaders() {
    Clear();
    {
        Request request;
        request.Set("X-Custom", "v");
        Response response;
        Require(Fails(Validate(request, response), "Request header field X-Custom is not allowed by Access-Control-Allow-Headers."));
        Headers headers;
        headers.Set("x-custom", "v");
        Require(CanSkip("GET"));
        Require(!CanSkip("GET", doNotUse, headers.map));
        Require(!CanSkip("GET"));
        response.AllowHeaders("x-other, X-CUSTOM");
        Require(Passes(Validate(request, response)));
        Require(CanSkip("GET", doNotUse, headers.map));
        headers.Set("X-Other", "v");
        Require(CanSkip("GET", doNotUse, headers.map));
        headers.Set("X-Third", "v");
        Require(!CanSkip("GET", doNotUse, headers.map));
    }

    Clear();
    {
        Request request;
        request.Set("X-Wide", "v", true);
        Response response;
        const ExpectedVoidString narrow = Validate(Request("PUT"), response);
        Require(!narrow.hasValue && (narrow.error->hashAndFlags & 4));
        const ExpectedVoidString wide = Validate(request, response);
        Require(Fails(wide, "Request header field X-Wide is not allowed by Access-Control-Allow-Headers."));
        Require(!(wide.error->hashAndFlags & 4));
    }

    Clear();
    {
        Request request;
        request.Set("X-First", "v").Set("authorization", "v").Set("X-Second", "v");
        Response response;
        Require(Fails(Validate(request, response), "Request header field Authorization is not allowed by Access-Control-Allow-Headers."));
        const ExpectedVoidString named = Validate(request, response);
        Require(named.error->hashAndFlags & 4);
        response.AllowHeaders("AUTHORIZATION");
        Require(Fails(Validate(request, response), "Request header field X-First is not allowed by Access-Control-Allow-Headers."));
        response.AllowHeaders("authorization, x-first");
        Require(Fails(Validate(request, response), "Request header field X-Second is not allowed by Access-Control-Allow-Headers."));
        response.AllowHeaders("authorization, x-first, x-second");
        Require(Passes(Validate(request, response)));
    }

    Clear();
    {
        Request request;
        request.Set("X-Custom", "v").Set("Authorization", "v");
        Response star;
        star.AllowHeaders("*");
        Require(Passes(Validate(request, star)));
        Require(CanSkip("GET", doNotUse, request.Map()));
        Require(!CanSkip("GET", use, request.Map()));
        Clear();
        star.Credentials();
        Require(Fails(Validate(request, star, use), "Request header field Authorization is not allowed by Access-Control-Allow-Headers."));
        Require(CanSkip("GET", doNotUse, request.Map()));
        star.AllowHeaders("*, authorization, x-custom");
        Require(Passes(Validate(request, star, use)));
        Require(CanSkip("GET", use, request.Map()));
    }

    Clear();
    {
        Request request;
        request.Set("X-Custom", "v");
        request.Map().uncommon.buffer[0].key = String();
        Response response;
        Require(Fails(Validate(request, response), "Request header field  is not allowed by Access-Control-Allow-Headers."));
        response.AllowHeaders("x-custom");
        Require(Throws([&] { Validate(request, response); }));
    }
}

static bool IsSafe(std::string_view name, std::string_view value) {
    Clear();
    Request request;
    request.Set(name, value);
    Response response;
    const ExpectedVoidString result = Validate(request, response);
    Require(result.hasValue || Contents(result.error).starts_with("Request header field "));
    return result.hasValue;
}

static void TestSafeRequestHeaders() {
    for (const char* value : {"", "text/html", "text/html, */*;q=0.8", "a b\tc", "a*b!#$%&'+-.^_`|~", "\x80\xff"}) Require(IsSafe("Accept", value));
    for (const char* value : {"a\"b", "(a)", "a:b", "a<b", "a>b", "a?b", "a@b", "a[b", "a\\b", "a]b", "a{b", "a}b", "a\x7f", "a\x01", "a\x1f", "a\nb"})
        Require(!IsSafe("Accept", value));
    Require(IsSafe("Accept", std::string(128, 'a')));
    Require(!IsSafe("Accept", std::string(129, 'a')));

    for (const char* name : {"Accept-Language", "Content-Language"}) {
        for (const char* value : {"", "en-US,en;q=0.9", "EN *,-.;=09"}) Require(IsSafe(name, value));
        for (const char* value : {"en/US", "en_US", "en\t", "en:", "en+", "en\x80"}) Require(!IsSafe(name, value));
        Require(IsSafe(name, std::string(128, 'a')));
        Require(!IsSafe(name, std::string(129, 'a')));
    }

    for (const char* value : {"text/plain", "TEXT/PLAIN", " text/plain ", "\ttext/plain\t","text/plain;charset=utf-8", "text/plain ; charset=utf-8", "text/plain  ;",
                              "text/plain;", "text/plain;;a", "text/plain; a=b; c", "multipart/form-data; boundary=x", "Multipart/Form-Data",
                              "application/x-www-form-urlencoded", "APPLICATION/X-WWW-FORM-URLENCODED;a=b"})
        Require(IsSafe("Content-Type", value));
    for (const char* value : {"", " ", "text", "text/", "/plain", "text/ plain", "text /plain", "text/plain\t;charset=utf-8", "text/plain\t;", "text/plain\r\n", "text/plain\n;a","text/plain x",
                              "application/json", "text/html", "text/plainx", "xtext/plain", "text/plain; a=\"b\"", "text/plain; a=(b)", "text/plain; a=b:c",
                              "text/plain,text/html", "text//plain", "text/pl ain", "text/plain\x01", "text/plain;\x7f"})
        Require(!IsSafe("Content-Type", value));
    for (const char unsafe : std::string_view("\"():<>?@[\\]{}\x7f\x01\x1f")) Require(!IsSafe("Content-Type", std::string("text/plain; a=") + unsafe));
    for (const char* value : {"text/plain;\ta=b", "text/plain; a=b,c/d*e", "text/plain; a=\x80"}) Require(IsSafe("Content-Type", value));
    Require(IsSafe("Content-Type", "text/plain;" + std::string(117, 'a')));
    Require(!IsSafe("Content-Type", "text/plain;" + std::string(118, 'a')));

    for (const char* name : {"Range", "Authorization", "Cache-Control", "Content-Length", "Accept-Encoding", "Origin"}) Require(!IsSafe(name, "a"));
}

static void TestCacheKeys() {
    Clear();
    Request request("PUT");
    Response response;
    response.Methods("PUT");
    Require(Passes(Validate(request, response)));
    Require(!CanSkip("PUT", doNotUse, HTTPHeaderMap{}, "https://example.com:443"));
    Require(!CanSkip("PUT", doNotUse, HTTPHeaderMap{}, "http://example.com"));
    Require(!CanSkip("PUT", doNotUse, HTTPHeaderMap{}, kOrigin, "https://api.example/resource/"));
    Require(!CanSkip("PUT", doNotUse, HTTPHeaderMap{}, kOrigin, "https://api.example/Resource"));
    Require(CanSkip(Text(kOrigin, true), Text(kURL, true), doNotUse, Text("PUT", true), HTTPHeaderMap{}));
    Require(CanSkip("PUT"));

    Request other("PATCH", "https://api.example/other");
    Response patch;
    patch.Methods("PATCH");
    Require(Passes(Validate(other, patch)));
    Require(CanSkip("PUT"));
    Require(CanSkip("PATCH", doNotUse, HTTPHeaderMap{}, kOrigin, "https://api.example/other"));
    Require(!CanSkip("PATCH"));
    Require(!CanSkip("PUT"));
    Require(CanSkip("PATCH", doNotUse, HTTPHeaderMap{}, kOrigin, "https://api.example/other"));

    Request again("PUT", "https://api.example/other");
    Response replaced;
    replaced.Methods("PUT");
    Require(Passes(Validate(again, replaced)));
    Require(CanSkip("PUT", doNotUse, HTTPHeaderMap{}, kOrigin, "https://api.example/other"));
    Require(!CanSkip("PATCH", doNotUse, HTTPHeaderMap{}, kOrigin, "https://api.example/other"));

    Require(Passes(Validate(request, response)));
    Clear();
    Require(!CanSkip("PUT"));
    Require(!CanSkip("GET"));
    Require(Passes(Validate(other, patch)));
    Require(!CanSkip("PUT"));
    Require(CanSkip("PATCH", doNotUse, HTTPHeaderMap{}, kOrigin, "https://api.example/other"));
}

static void TestExpiry() {
    Clear();
    Request request;
    Response response;
    response.MaxAge("0");
    Require(Passes(Validate(request, response)));
    unsigned attempts = 0;
    while (CanSkip() && attempts < 2000000) ++attempts;
    Require(attempts < 2000000);
    Require(!CanSkip());

    response.MaxAge("600");
    Require(Passes(Validate(request, response)));
    for (unsigned index = 0; index < 1000; ++index) Require(CanSkip());

    Clear();
    AppendGuest(Now() + 4, doNotUse);
    Require(CanSkip());
    AppendGuest(Now() - 0.001, doNotUse);
    Require(!CanSkip());
    AppendGuest(std::numeric_limits<double>::quiet_NaN(), doNotUse);
    Require(CanSkip());
    AppendGuest(std::numeric_limits<double>::infinity(), doNotUse);
    Require(CanSkip());
    AppendGuest(-std::numeric_limits<double>::infinity(), doNotUse);
    Require(!CanSkip());
    Require(!CanSkip());
}

static void TestGuestItems() {
    Clear();
    {
        std::vector<String> watched;
        StringImpl** methods = MakeTable(watched, {"PUT", "delete"});
        StringImpl** headers = MakeTable(watched, {"X-Custom", "authorization"});
        for (const auto& text : watched) Require(text.impl->refCount == 4);
        AppendGuest(Now() + 100, doNotUse, methods, headers);
        for (const auto& text : watched) Require(text.impl->refCount == 2);
        Headers allowed;
        allowed.Set("x-CUSTOM", "v").Set("Authorization", "v");
        Require(CanSkip("PUT", doNotUse, allowed.map));
        Require(CanSkip("delete", doNotUse, allowed.map));
        Require(CanSkip("POST", doNotUse, allowed.map));
        Require(!CanSkip("DELETE", doNotUse, allowed.map));
        Require(!CanSkip("PUT", doNotUse, allowed.map));
    }

    Clear();
    {
        std::vector<String> watched;
        AppendGuest(Now() + 100, doNotUse, MakeTable(watched, {"PUT"}, true), MakeTable(watched, {"X-Custom"}, true));
        Headers allowed;
        allowed.Set("x-custom", "v");
        Require(CanSkip("PUT", doNotUse, allowed.map));
        Require(CanSkip("PUT", ephemeralStateless, allowed.map));
        Require(!CanSkip("PUT", use, allowed.map));
        Require(!CanSkip("PUT", doNotUse, allowed.map));
    }

    Clear();
    {
        std::vector<String> watched;
        AppendGuest(Now() + 100, use, MakeTable(watched, {"PUT"}), nullptr);
        Require(CanSkip("PUT", use));
        Require(CanSkip("PUT", doNotUse));
        AppendGuest(Now() + 100, ephemeralStateless, MakeTable(watched, {"PUT"}), nullptr);
        Require(CanSkip("PUT", use));
        AppendGuest(Now() + 100, use, MakeTable(watched, {"*"}), MakeTable(watched, {"*"}));
        Headers custom;
        custom.Set("X-Custom", "v");
        Require(CanSkip("PATCH", doNotUse, custom.map));
        Require(!CanSkip("PATCH", use));
        AppendGuest(Now() + 100, use, MakeTable(watched, {"*"}), MakeTable(watched, {"*"}));
        Require(!CanSkip("GET", use, custom.map));
        Require(!CanSkip("GET"));
    }

    Clear();
    {
        const String origin = Text(kOrigin);
        const String urlText = Text(kURL);
        const String method = Text("GET");
        const String none;
        const HTTPHeaderMap headers{};
        Require(!CanSkip(none, urlText, doNotUse, method, headers));
        Require(!CanSkip(origin, none, doNotUse, method, headers));
        CrossOriginPreflightResultCacheItem* empty = nullptr;
        URL url{};
        url.impl = urlText.impl;
        URL nullURL{};
        Require(Throws([&] {
            _ZN7WebCore31CrossOriginPreflightResultCache11appendEntryERKN3WTF6StringERKNS1_3URLESt10unique_ptrINS_35CrossOriginPreflightResultCacheItemESt14default_deleteIS9_EE(
                Cache(), &none, &url, &empty);
        }));
        Require(Throws([&] {
            _ZN7WebCore31CrossOriginPreflightResultCache11appendEntryERKN3WTF6StringERKNS1_3URLESt10unique_ptrINS_35CrossOriginPreflightResultCacheItemESt14default_deleteIS9_EE(
                Cache(), &origin, &nullURL, &empty);
        }));
        Require(Throws([&] {
            _ZN7WebCore31CrossOriginPreflightResultCache11appendEntryERKN3WTF6StringERKNS1_3URLESt10unique_ptrINS_35CrossOriginPreflightResultCacheItemESt14default_deleteIS9_EE(
                Cache(), nullptr, &url, &empty);
        }));
        Require(Throws([&] {
            _ZN7WebCore31CrossOriginPreflightResultCache11appendEntryERKN3WTF6StringERKNS1_3URLESt10unique_ptrINS_35CrossOriginPreflightResultCacheItemESt14default_deleteIS9_EE(
                Cache(), &origin, nullptr, &empty);
        }));
        Require(Throws([&] {
            _ZN7WebCore31CrossOriginPreflightResultCache11appendEntryERKN3WTF6StringERKNS1_3URLESt10unique_ptrINS_35CrossOriginPreflightResultCacheItemESt14default_deleteIS9_EE(
                Cache(), &origin, &url, nullptr);
        }));
        Require(!CanSkip(none, urlText, doNotUse, method, headers));
        _ZN7WebCore31CrossOriginPreflightResultCache11appendEntryERKN3WTF6StringERKNS1_3URLESt10unique_ptrINS_35CrossOriginPreflightResultCacheItemESt14default_deleteIS9_EE(
            Cache(), &origin, &url, &empty);
        Require(Throws([&] { CanSkip(none, urlText, doNotUse, method, headers); }));
        Require(Throws([&] { CanSkip(origin, none, doNotUse, method, headers); }));
        Require(Throws([&] { CanSkip(origin, urlText, doNotUse, method, headers); }));
        Require(!CanSkip(origin, Text("https://api.example/absent"), doNotUse, method, headers));
        Require(Throws([&] {
            _ZN7WebCore31CrossOriginPreflightResultCache16canSkipPreflightERKN3WTF6StringERKNS1_3URLENS_23StoredCredentialsPolicyES4_RKNS_13HTTPHeaderMapE(
                Cache(), nullptr, &url, doNotUse, &method, &headers);
        }));
        Require(Throws([&] {
            _ZN7WebCore31CrossOriginPreflightResultCache16canSkipPreflightERKN3WTF6StringERKNS1_3URLENS_23StoredCredentialsPolicyES4_RKNS_13HTTPHeaderMapE(
                Cache(), &origin, nullptr, doNotUse, &method, &headers);
        }));
        Require(Throws([&] {
            _ZN7WebCore31CrossOriginPreflightResultCache16canSkipPreflightERKN3WTF6StringERKNS1_3URLENS_23StoredCredentialsPolicyES4_RKNS_13HTTPHeaderMapE(
                Cache(), &origin, &url, doNotUse, nullptr, &headers);
        }));
        Require(Throws([&] {
            _ZN7WebCore31CrossOriginPreflightResultCache16canSkipPreflightERKN3WTF6StringERKNS1_3URLENS_23StoredCredentialsPolicyES4_RKNS_13HTTPHeaderMapE(
                Cache(), &origin, &url, doNotUse, &method, nullptr);
        }));
        Clear();
        Require(!CanSkip(none, urlText, doNotUse, method, headers));
    }

    Clear();
    {
        std::vector<String> watched;
        const String origin = Text(kOrigin);
        const String urlText = Text(kURL);
        const String none;
        const HTTPHeaderMap headers{};
        AppendGuest(Now() + 100, doNotUse);
        Require(!CanSkip(origin, urlText, doNotUse, none, headers));
        Require(!CanSkip());
        AppendGuest(Now() + 100, doNotUse, MakeTable(watched, {"PUT"}), nullptr);
        Require(Throws([&] { CanSkip(origin, urlText, doNotUse, none, headers); }));
        Require(CanSkip("PUT"));

        Headers custom;
        custom.Set("X-Custom", "v");
        custom.map.uncommon.buffer[0].key = String();
        Require(!CanSkip("PUT", doNotUse, custom.map));
        AppendGuest(Now() + 100, doNotUse, nullptr, MakeTable(watched, {"x-custom"}));
        Require(Throws([&] { CanSkip("GET", doNotUse, custom.map); }));
        Require(CanSkip("GET"));

        Headers unknown;
        unknown.Set("Authorization", "v");
        unknown.map.common.buffer[0].key = static_cast<HTTPHeaderName>(NKWebKit::HttpAbi::headerNames.size());
        Require(Throws([&] { CanSkip("GET", doNotUse, unknown.map); }));
        unknown.map.common.buffer[0].key = static_cast<HTTPHeaderName>(-1);
        Require(Throws([&] { CanSkip("GET", doNotUse, unknown.map); }));
        unknown.map.common.buffer[0].key = HTTPHeaderName::Authorization;

        HTTPHeaderMap broken = unknown.map;
        broken.common.size = broken.common.capacity + 1;
        Require(Throws([&] { CanSkip("GET", doNotUse, broken); }));
        broken = unknown.map;
        broken.common.buffer = nullptr;
        Require(Throws([&] { CanSkip("GET", doNotUse, broken); }));
        broken = HTTPHeaderMap{};
        broken.uncommon.size = 1;
        broken.uncommon.capacity = 1;
        Require(Throws([&] { CanSkip("GET", doNotUse, broken); }));
        Require(CanSkip("GET"));
    }
    Clear();
}

static void TestNullArguments() {
    Clear();
    Request request;
    Response response;
    Origin origin;
    Require(Throws([&] {
        _ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            nullptr, &response.layout.head, doNotUse, &origin.origin, nullptr);
    }));
    Require(Throws([&] {
        _ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            &request.base, nullptr, doNotUse, &origin.origin, nullptr);
    }));
    Require(Throws([&] {
        _ZN7WebCore25validatePreflightResponseERKNS_15ResourceRequestERKNS_16ResourceResponseENS_23StoredCredentialsPolicyERKNS_14SecurityOriginEPKNS_37CrossOriginAccessControlCheckDisablerE(
            &request.base, &response.layout.head, doNotUse, nullptr, nullptr);
    }));
    request.base.url.impl = nullptr;
    Require(Throws([&] { Validate(request, response); }));
    request.base.url.impl = request.urlText.impl;
    Require(!CanSkip());
}

int main() {
    TestSingleton();
    TestLifetime();
    TestStatusAndAccessCheck();
    TestAllowLists();
    TestMethods();
    TestHeaders();
    TestSafeRequestHeaders();
    TestCacheKeys();
    TestExpiry();
    TestGuestItems();
    TestNullArguments();
    std::puts("libSceNKWebKit CORS preflight tests passed");
    return 0;
}

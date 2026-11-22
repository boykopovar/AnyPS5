#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::LegacyCookieJar;
using NKWebKit::WebCoreAbi::LegacyCookieJarVtable;
using NKWebKit::WebCoreAbi::LegacyCookieRequestHeaderFieldValue;
using NKWebKit::WebCoreAbi::LegacyOptionalIdentifier;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::LegacySameSiteInfo;
using NKWebKit::WebCoreAbi::LegacyVaryingRequestHeader;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WtfAbi::Vector;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "varying request headers check failed at line %d\n", line);
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

static String Wide(std::u16string_view units) {
    return _ZN3WTF10StringImpl6createEPKDsj(units.data(), static_cast<unsigned>(units.size()));
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

struct Request {
    LegacyResourceRequestBase base{};

    explicit Request(std::string_view url = "https://a.example/page", std::string_view method = "GET") {
        base.url = Parse(url);
        base.firstPartyForCookies = Parse("https://first.example/");
        base.httpMethod = Text(method);
        base.resourceRequestUpdated = true;
    }
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
    ~Request() {
        ReleaseHeaders(Map().common);
        ReleaseHeaders(Map().uncommon);
        NKWebKit::WtfAbi::Release(base.url.impl);
        NKWebKit::WtfAbi::Release(base.firstPartyForCookies.impl);
    }

    HTTPHeaderMap& Map() {
        return *reinterpret_cast<HTTPHeaderMap*>(&base.httpHeaderFields);
    }
    Request& Set(std::string_view name, std::string_view value) {
        _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&Map(), Text(name), Text(value));
        return *this;
    }
};

struct Response {
    alignas(8) unsigned char storage[512]{};

    Response() = default;
    explicit Response(const String& vary) {
        _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(Base(), HTTPHeaderName::Vary, vary);
    }
    explicit Response(std::string_view vary) : Response(Text(vary)) {
    }
    Response(const Response&) = delete;
    Response& operator=(const Response&) = delete;
    ~Response() {
        ReleaseHeaders(Base()->httpHeaderFields.common);
        ReleaseHeaders(Base()->httpHeaderFields.uncommon);
    }

    ResourceResponseBase* Base() {
        return reinterpret_cast<ResourceResponseBase*>(storage);
    }
};

struct JarCall {
    std::string firstParty;
    std::string url;
    LegacySameSiteInfo sameSiteInfo{};
    bool frameEngaged = true;
    bool pageEngaged = true;
    bool distinctOptionals = false;
    bool includeSecureCookies = false;
};

static int jarCalls = 0;
static JarCall lastCall;
static const LegacyCookieJar* lastJar = nullptr;
static StringImpl* jarAnswer = nullptr;

static LegacyCookieRequestHeaderFieldValue APS5_VABI JarCookieRequestHeaderFieldValue(
    const LegacyCookieJar* jar, const URL* firstParty, const LegacySameSiteInfo* sameSiteInfo, const URL* url, const LegacyOptionalIdentifier* frame,
    const LegacyOptionalIdentifier* page, bool includeSecureCookies) {
    ++jarCalls;
    lastJar = jar;
    lastCall = {Contents(firstParty->impl), Contents(url->impl), *sameSiteInfo, frame->engaged, page->engaged, frame != page, includeSecureCookies};
    LegacyCookieRequestHeaderFieldValue result{String(jarAnswer), true};
    if (jarAnswer) jarAnswer->refCount += 2;
    return result;
}

static const LegacyCookieJarVtable jarVtable{nullptr, nullptr, nullptr, &JarCookieRequestHeaderFieldValue, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};

static std::string Collect(const LegacyCookieJar* jar, Request& request, Response& response, unsigned* capacity = nullptr) {
    const Vector<LegacyVaryingRequestHeader> headers =
        _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(jar, &request.base, response.Base());
    if (capacity) *capacity = headers.capacity;
    Require(headers.size <= headers.capacity && (headers.buffer != nullptr) == (headers.capacity != 0));
    std::string text;
    for (unsigned index = 0; index < headers.size; ++index)
        text += "[" + Contents(headers.buffer[index].name.impl) + "=" + Contents(headers.buffer[index].value.impl) + "]";
    return text;
}

static std::string Collect(std::string_view vary, Request& request, const LegacyCookieJar* jar = nullptr) {
    Response response(vary);
    return Collect(jar, request, response);
}

static void TestNames() {
    Request request;
    request.Set("Accept", "text/html").Set("X-Custom", "c").Set("Accept-Language", "en").Set("Cookie", "from-request=1");
    Response none;
    unsigned capacity = 7;
    Require(Collect(nullptr, request, none, &capacity).empty() && capacity == 0);
    Response empty("");
    Require(Collect(nullptr, request, empty, &capacity).empty() && capacity == 0);
    Response commas(",,");
    Require(Collect(nullptr, request, commas, &capacity).empty() && capacity == 0);

    Require(Collect("Accept", request) == "[Accept=text/html]");
    Require(Collect("accept", request) == "[accept=text/html]");
    Require(Collect("Accept, X-Custom ,Accept-Language", request) == "[Accept=text/html][X-Custom=c][Accept-Language=en]");
    Require(Collect("Accept,,X-Custom,", request) == "[Accept=text/html][X-Custom=c]");
    Require(Collect("X-Absent", request) == "[X-Absent=<null>]");
    Require(Collect("*", request) == "[*=<null>]");
    Require(Collect(" , Accept", request) == "[=<null>][Accept=text/html]");
    Require(Collect("\t\n\v\f\r Accept \r\n", request) == "[Accept=text/html]");
    Require(Collect("Acc ept", request) == "[Acc ept=<null>]");
    Require(Collect("Accept;q=1", request) == "[Accept;q=1=<null>]");
    Require(Collect("cookie", request) == "[cookie=from-request=1]");
    Require(Collect("COOKIE", request) == "[COOKIE=from-request=1]");
    Require(Collect("Cookie", request) == "[Cookie=<null>]");

    Response three("Accept, X-Custom ,Accept-Language");
    Require(Collect(nullptr, request, three, &capacity) == "[Accept=text/html][X-Custom=c][Accept-Language=en]" && capacity == 3);
    Response spaced(" , Accept");
    Require(Collect(nullptr, request, spaced, &capacity) == "[=<null>][Accept=text/html]" && capacity == 2);

    Response wide(Wide(u"　Accept , X-Custom"));
    Require(Collect(nullptr, request, wide) == "[Accept=text/html][\xa0X-Custom=<null>]");
}

static void TestStrings() {
    Request request;
    request.Set("Accept", "text/html");
    const StringImpl* acceptValue = request.Map().common.buffer[0].value.impl;
    const unsigned valueReferences = acceptValue->refCount;
    {
        const String vary = Text("Accept");
        Response response(vary);
        const unsigned varyReferences = vary.impl->refCount;
        {
            const Vector<LegacyVaryingRequestHeader> headers =
                _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(nullptr, &request.base, response.Base());
            Require(headers.size == 1 && headers.capacity == 1);
            Require(headers.buffer[0].name.impl == vary.impl && vary.impl->refCount == varyReferences + 2);
            Require(headers.buffer[0].value.impl == acceptValue && acceptValue->refCount == valueReferences + 2);
        }
        Require(vary.impl->refCount == varyReferences && acceptValue->refCount == valueReferences);
    }
    {
        const String vary = Text(" Accept");
        Response response(vary);
        const unsigned varyReferences = vary.impl->refCount;
        const Vector<LegacyVaryingRequestHeader> headers =
            _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(nullptr, &request.base, response.Base());
        Require(headers.size == 1 && headers.buffer[0].name.impl != vary.impl && vary.impl->refCount == varyReferences);
        Require(headers.buffer[0].name.impl->refCount == 2 && (headers.buffer[0].name.impl->hashAndFlags & 4));
    }
    {
        Response response(Wide(u" Accept"));
        const Vector<LegacyVaryingRequestHeader> headers =
            _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(nullptr, &request.base, response.Base());
        Require(headers.size == 1 && Contents(headers.buffer[0].name.impl) == "Accept" && !(headers.buffer[0].name.impl->hashAndFlags & 4));
    }
    {
        Response response(" ,x");
        const Vector<LegacyVaryingRequestHeader> headers =
            _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(nullptr, &request.base, response.Base());
        Require(headers.size == 2 && headers.buffer[0].name.impl == NKWebKit::WtfAbi::EmptyStringImpl());
    }
}

static void TestCookieJar() {
    const LegacyCookieJar jar{&jarVtable, 1, nullptr};
    const String answer = Text("jar=1");
    jarAnswer = answer.impl;
    {
        Request request;
        request.Set("Cookie", "from-request=1").Set("Accept", "a");
        jarCalls = 0;
        Require(Collect("Accept, Cookie", request, &jar) == "[Accept=a][Cookie=jar=1]");
        Require(jarCalls == 1 && lastJar == &jar);
        Require(lastCall.firstParty == "https://first.example/" && lastCall.url == "https://a.example/page");
        Require(!lastCall.frameEngaged && !lastCall.pageEngaged && lastCall.distinctOptionals && lastCall.includeSecureCookies);
        Require(!lastCall.sameSiteInfo.isSameSite && !lastCall.sameSiteInfo.isTopSite && lastCall.sameSiteInfo.isSafeHTTPMethod);
        Require(answer.impl->refCount == 2);
        Require(Collect("Cookie,Cookie", request, &jar) == "[Cookie=jar=1][Cookie=jar=1]" && jarCalls == 3);
        Require(Collect("cookie, Accept", request, &jar) == "[cookie=from-request=1][Accept=a]" && jarCalls == 3);
        Require(Collect(" Cookie ", request, &jar) == "[Cookie=jar=1]" && jarCalls == 4);
        Require(Collect("Accept", request, &jar) == "[Accept=a]" && jarCalls == 4);
        Require(Collect("Cookies, Cooki", request, &jar) == "[Cookies=<null>][Cooki=<null>]" && jarCalls == 4);
        Require(Collect("Cookie", request, nullptr) == "[Cookie=<null>]" && jarCalls == 4);
    }
    {
        Request plain("http://a.example/page");
        Require(Collect("Cookie", plain, &jar) == "[Cookie=jar=1]" && !lastCall.includeSecureCookies && lastCall.url == "http://a.example/page");
        Request secureUpper("HTTPS://a.example/page");
        Require(Collect("Cookie", secureUpper, &jar) == "[Cookie=jar=1]" && lastCall.includeSecureCookies);
        Request socket("wss://a.example/page");
        Require(Collect("Cookie", socket, &jar) == "[Cookie=jar=1]" && !lastCall.includeSecureCookies);
    }
    for (unsigned disposition = 0; disposition < 4; ++disposition) {
        for (bool topSite : {false, true}) {
            Request request;
            request.base.sameSiteDisposition = static_cast<std::uint8_t>(disposition);
            request.base.isTopSite = topSite;
            Require(Collect("Cookie", request, &jar) == "[Cookie=jar=1]");
            Require(lastCall.sameSiteInfo.isSameSite == (disposition == 1) && lastCall.sameSiteInfo.isTopSite == topSite);
        }
    }
    for (const char* method : {"GET", "get", "HEAD", "Head", "OPTIONS", "options", "TRACE", "trace"}) {
        Request request("https://a.example/page", method);
        Require(Collect("Cookie", request, &jar) == "[Cookie=jar=1]" && lastCall.sameSiteInfo.isSafeHTTPMethod);
    }
    for (const char* method : {"POST", "PUT", "DELETE", "GETS", "GE", "", "CONNECT"}) {
        Request request("https://a.example/page", method);
        Require(Collect("Cookie", request, &jar) == "[Cookie=jar=1]" && !lastCall.sameSiteInfo.isSafeHTTPMethod);
    }
    {
        Request request;
        request.base.httpMethod = String();
        Require(Collect("Cookie", request, &jar) == "[Cookie=jar=1]" && !lastCall.sameSiteInfo.isSafeHTTPMethod);
        jarAnswer = nullptr;
        Require(Collect("Cookie", request, &jar) == "[Cookie=<null>]");
        jarAnswer = answer.impl;
    }
    Require(answer.impl->refCount == 2);

    Request request;
    const LegacyCookieJar noTable{nullptr, 1, nullptr};
    const LegacyCookieJarVtable emptyTable{};
    const LegacyCookieJar noFunction{&emptyTable, 1, nullptr};
    Require(Throws([&] { Collect("Cookie", request, &noTable); }));
    Require(Throws([&] { Collect("Cookie", request, &noFunction); }));
    Require(Collect("Accept", request, &noTable) == "[Accept=<null>]");
}

static void TestUpdatedFlagAndArguments() {
    Request request;
    request.Set("Accept", "a");
    request.base.resourceRequestUpdated = false;
    Response none;
    Require(Collect(nullptr, request, none).empty() && !request.base.resourceRequestUpdated);
    Require(Collect("Accept", request) == "[Accept=a]" && request.base.resourceRequestUpdated);
    request.base.resourceRequestUpdated = false;
    Require(Collect("Cookie", request) == "[Cookie=<null>]" && !request.base.resourceRequestUpdated);
    const LegacyCookieJar jar{&jarVtable, 1, nullptr};
    jarAnswer = nullptr;
    Require(Collect("Cookie", request, &jar) == "[Cookie=<null>]" && request.base.resourceRequestUpdated);

    Response response("Accept");
    Require(Throws([&] {
        _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(nullptr, nullptr, response.Base());
    }));
    Require(Throws([&] {
        _ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE(nullptr, &request.base, nullptr);
    }));
}

int main() {
    TestNames();
    TestStrings();
    TestCookieJar();
    TestUpdatedFlagAndArguments();
    std::puts("libSceNKWebKit varying request headers tests passed");
    return 0;
}

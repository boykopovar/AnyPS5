#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

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

struct Expected {
    std::optional<std::string_view> name;
    std::optional<std::string_view> value;
    bool wideValue = false;
};

static bool Verify(const LegacyCookieJar* jar, Request& request, std::initializer_list<Expected> expected) {
    Vector<LegacyVaryingRequestHeader> headers(static_cast<unsigned>(expected.size()));
    for (const Expected& pair : expected) {
        String value;
        if (pair.value) value = pair.wideValue ? Wide(std::u16string(pair.value->begin(), pair.value->end())) : Text(*pair.value);
        headers.append(LegacyVaryingRequestHeader{pair.name ? Text(*pair.name) : String(), value});
    }
    return _ZN7WebCore27verifyVaryingRequestHeadersEPKNS_9CookieJarERKN3WTF6VectorISt4pairINS3_6StringES6_ELm0ENS3_15CrashOnOverflowELm16ENS3_10FastMallocEEERKNS_15ResourceRequestE(
        jar, &headers, &request.base);
}

static void TestVerify() {
    Request request;
    request.Set("Accept", "text/html").Set("X-Custom", "c").Set("X-Empty", "").Set("Cookie", "from-request=1");
    request.base.resourceRequestUpdated = false;
    Require(Verify(nullptr, request, {}));
    Require(!Verify(nullptr, request, {{"*", std::nullopt}}));
    Require(!Verify(nullptr, request, {{"*", "text/html"}}));
    Require(!request.base.resourceRequestUpdated);
    Require(Verify(nullptr, request, {{"Accept", "text/html"}}));
    Require(request.base.resourceRequestUpdated);
    Require(Verify(nullptr, request, {{"accept", "text/html"}, {"X-CUSTOM", "c"}}));
    Require(Verify(nullptr, request, {{"Accept", "text/html", true}}));
    Require(!Verify(nullptr, request, {{"Accept", "text/HTML"}}));
    Require(!Verify(nullptr, request, {{"Accept", "text/htm"}}));
    Require(!Verify(nullptr, request, {{"Accept", "text/html "}}));
    Require(!Verify(nullptr, request, {{" Accept", "text/html"}}));
    Require(!Verify(nullptr, request, {{"Accept", "text/html"}, {"X-Custom", "other"}}));
    Require(!Verify(nullptr, request, {{"X-Custom", "other"}, {"Accept", "text/html"}}));
    Require(!Verify(nullptr, request, {{"Accept", "text/html"}, {"*", std::nullopt}}));
    Require(Verify(nullptr, request, {{"X-Absent", std::nullopt}}));
    Require(!Verify(nullptr, request, {{"X-Absent", ""}}));
    Require(Verify(nullptr, request, {{"X-Empty", ""}}));
    Require(!Verify(nullptr, request, {{"X-Empty", std::nullopt}}));
    Require(!Verify(nullptr, request, {{"Accept", std::nullopt}}));
    Require(Verify(nullptr, request, {{std::nullopt, std::nullopt}}));
    Require(Verify(nullptr, request, {{"", std::nullopt}}));
    Require(Verify(nullptr, request, {{"**", std::nullopt}}));

    const LegacyCookieJar jar{&jarVtable, 1, nullptr};
    const String answer = Text("jar=1");
    jarAnswer = answer.impl;
    jarCalls = 0;
    Require(Verify(&jar, request, {{"Cookie", "jar=1"}}) && jarCalls == 1);
    Require(lastCall.url == "https://a.example/page" && lastCall.includeSecureCookies);
    Require(!Verify(&jar, request, {{"Cookie", "from-request=1"}}) && jarCalls == 2);
    Require(Verify(&jar, request, {{"cookie", "from-request=1"}}) && jarCalls == 2);
    Require(Verify(&jar, request, {{"Accept", "text/html"}, {"Cookie", "jar=1"}, {"Cookie", "jar=1"}}) && jarCalls == 4);
    Require(!Verify(&jar, request, {{"Accept", "other"}, {"Cookie", "jar=1"}}) && jarCalls == 4);
    Require(!Verify(&jar, request, {{"*", std::nullopt}, {"Cookie", "jar=1"}}) && jarCalls == 4);
    Require(Verify(nullptr, request, {{"Cookie", std::nullopt}}));
    Require(!Verify(nullptr, request, {{"Cookie", "jar=1"}}));
    jarAnswer = nullptr;
    Require(Verify(&jar, request, {{"Cookie", std::nullopt}}) && jarCalls == 5);
    Require(!Verify(&jar, request, {{"Cookie", ""}}) && jarCalls == 6);
    Require(answer.impl->refCount == 2);

    Vector<LegacyVaryingRequestHeader> headers(1);
    headers.append(LegacyVaryingRequestHeader{Text("Accept"), Text("text/html")});
    constexpr auto verify = &_ZN7WebCore27verifyVaryingRequestHeadersEPKNS_9CookieJarERKN3WTF6VectorISt4pairINS3_6StringES6_ELm0ENS3_15CrashOnOverflowELm16ENS3_10FastMallocEEERKNS_15ResourceRequestE;
    Require(verify(nullptr, &headers, &request.base));
    Require(headers.buffer[0].name.impl->refCount == 2 && headers.buffer[0].value.impl->refCount == 2);
    Require(Throws([&] { verify(nullptr, nullptr, &request.base); }));
    Require(Throws([&] { verify(nullptr, &headers, nullptr); }));
    headers.capacity = 0;
    Require(Throws([&] { verify(nullptr, &headers, &request.base); }));
    headers.capacity = 1;
    LegacyVaryingRequestHeader* storage = std::exchange(headers.buffer, nullptr);
    Require(Throws([&] { verify(nullptr, &headers, &request.base); }));
    headers.buffer = storage;
}

static void TestSameSiteInfo() {
    for (unsigned disposition = 0; disposition < 4; ++disposition) {
        for (bool topSite : {false, true}) {
            for (const char* method : {"GET", "head", "Options", "TRACE", "POST", "GETS", ""}) {
                Request request("https://a.example/page", method);
                request.base.sameSiteDisposition = static_cast<std::uint8_t>(disposition);
                request.base.isTopSite = topSite;
                request.base.resourceRequestUpdated = false;
                const LegacySameSiteInfo info = _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(&request.base);
                const std::string_view name(method);
                const bool safe = name == "GET" || name == "head" || name == "Options" || name == "TRACE";
                Require(info.isSameSite == (disposition == 1) && info.isTopSite == topSite && info.isSafeHTTPMethod == safe);
                Require(request.base.resourceRequestUpdated);
            }
        }
    }
    Request request;
    request.base.httpMethod = String();
    Require(!_ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(&request.base).isSafeHTTPMethod);
    Require(Throws([] { _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(nullptr); }));
}

template<typename TFunction>
static bool ThrowsNotImplemented(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return false;
    } catch (const std::runtime_error&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr);
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize, pageSize, PAGE_READONLY, &previous) != 0);
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED);
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize, pageSize, PROT_READ) == 0);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0);
#endif
    }

    GuardedPages(const GuardedPages&) = delete;
    GuardedPages& operator=(const GuardedPages&) = delete;

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    unsigned char* Writable() const { return base; }
    unsigned char* ReadOnly() const { return base + pageSize; }
    unsigned char* Guard() const { return base + pageSize * 2; }

    template<typename T>
    T* SplitReadOnlyToGuard() const {
        return reinterpret_cast<T*>(base + pageSize * 2 - sizeof(T) / 2);
    }
};

static void TestGuestPointers() {
    GuardedPages pages;
    constexpr auto collect = &_ZN7WebCore28collectVaryingRequestHeadersEPKNS_9CookieJarERKNS_15ResourceRequestERKNS_16ResourceResponseE;
    constexpr auto verify = &_ZN7WebCore27verifyVaryingRequestHeadersEPKNS_9CookieJarERKN3WTF6VectorISt4pairINS3_6StringES6_ELm0ENS3_15CrashOnOverflowELm16ENS3_10FastMallocEEERKNS_15ResourceRequestE;
    constexpr auto sameSite = &_ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE;

    Request request;
    request.Set("Accept", "text/html").Set("X-Custom", "c");
    Response response("Accept");
    Response cookieResponse("Cookie");

    const auto* unmappedRequest = reinterpret_cast<const LegacyResourceRequestBase*>(pages.Guard());
    const auto* splitRequest = pages.SplitReadOnlyToGuard<const LegacyResourceRequestBase>();
    const auto* readOnlyRequest = reinterpret_cast<const LegacyResourceRequestBase*>(pages.ReadOnly());
    const auto* unmappedResponse = reinterpret_cast<const ResourceResponseBase*>(pages.Guard());
    const auto* splitResponse = pages.SplitReadOnlyToGuard<const ResourceResponseBase>();
    const auto* unmappedJar = reinterpret_cast<const LegacyCookieJar*>(pages.Guard());
    const auto* splitJar = pages.SplitReadOnlyToGuard<const LegacyCookieJar>();
    const auto* unmappedHeaders = reinterpret_cast<const Vector<LegacyVaryingRequestHeader>*>(pages.Guard());
    const auto* splitHeaders = pages.SplitReadOnlyToGuard<const Vector<LegacyVaryingRequestHeader>>();

    Require(ThrowsNotImplemented([&] { sameSite(unmappedRequest); }));
    Require(ThrowsNotImplemented([&] { sameSite(splitRequest); }));
    Require(ThrowsNotImplemented([&] { sameSite(readOnlyRequest); }));

    {
        Request badMethod;
        StringImpl* const orig = std::exchange(badMethod.base.httpMethod.impl, reinterpret_cast<StringImpl*>(pages.Guard()));
        badMethod.base.resourceRequestUpdated = false;
        Require(ThrowsNotImplemented([&] { sameSite(&badMethod.base); }));
        Require(!badMethod.base.resourceRequestUpdated);
        StringImpl badMethodData{2, 8, pages.Guard(), 4};
        badMethod.base.httpMethod.impl = &badMethodData;
        Require(ThrowsNotImplemented([&] { sameSite(&badMethod.base); }));
        Require(!badMethod.base.resourceRequestUpdated);
        badMethod.base.httpMethod.impl = orig;
    }

    Require(ThrowsNotImplemented([&] { collect(nullptr, unmappedRequest, response.Base()); }));
    Require(ThrowsNotImplemented([&] { collect(nullptr, splitRequest, response.Base()); }));
    Require(ThrowsNotImplemented([&] { collect(nullptr, readOnlyRequest, response.Base()); }));
    Require(ThrowsNotImplemented([&] { collect(nullptr, &request.base, unmappedResponse); }));
    Require(ThrowsNotImplemented([&] { collect(nullptr, &request.base, splitResponse); }));
    Require(ThrowsNotImplemented([&] { collect(unmappedJar, &request.base, cookieResponse.Base()); }));
    Require(ThrowsNotImplemented([&] { collect(splitJar, &request.base, cookieResponse.Base()); }));

    const LegacyCookieJar badVtableJar{reinterpret_cast<const LegacyCookieJarVtable*>(pages.Guard()), 1, nullptr};
    const LegacyCookieJar splitVtableJar{pages.SplitReadOnlyToGuard<const LegacyCookieJarVtable>(), 1, nullptr};
    Require(ThrowsNotImplemented([&] { collect(&badVtableJar, &request.base, cookieResponse.Base()); }));
    Require(ThrowsNotImplemented([&] { collect(&splitVtableJar, &request.base, cookieResponse.Base()); }));

    {
        Response badResponse;
        badResponse.Base()->httpHeaderFields.common.buffer = reinterpret_cast<NKWebKit::HttpAbi::CommonHeader*>(pages.Guard());
        badResponse.Base()->httpHeaderFields.common.capacity = 1;
        badResponse.Base()->httpHeaderFields.common.size = 1;
        Require(ThrowsNotImplemented([&] { collect(nullptr, &request.base, badResponse.Base()); }));
        badResponse.Base()->httpHeaderFields.common = {};
    }

    {
        Request badReqHeaders;
        auto* const origCommon = std::exchange(badReqHeaders.Map().common.buffer, reinterpret_cast<NKWebKit::HttpAbi::CommonHeader*>(pages.Guard()));
        const auto origCap = std::exchange(badReqHeaders.Map().common.capacity, 1u);
        const auto origSize = std::exchange(badReqHeaders.Map().common.size, 1u);
        badReqHeaders.base.resourceRequestUpdated = false;
        Require(ThrowsNotImplemented([&] { collect(nullptr, &badReqHeaders.base, response.Base()); }));
        Require(!badReqHeaders.base.resourceRequestUpdated);
        badReqHeaders.Map().common.buffer = origCommon;
        badReqHeaders.Map().common.capacity = origCap;
        badReqHeaders.Map().common.size = origSize;
    }

    Vector<LegacyVaryingRequestHeader> validHeaders(1);
    validHeaders.append(LegacyVaryingRequestHeader{Text("Accept"), Text("text/html")});
    Require(ThrowsNotImplemented([&] { verify(nullptr, unmappedHeaders, &request.base); }));
    Require(ThrowsNotImplemented([&] { verify(nullptr, splitHeaders, &request.base); }));
    Require(ThrowsNotImplemented([&] { verify(nullptr, &validHeaders, unmappedRequest); }));
    Require(ThrowsNotImplemented([&] { verify(nullptr, &validHeaders, splitRequest); }));
    Require(ThrowsNotImplemented([&] { verify(nullptr, &validHeaders, readOnlyRequest); }));

    {
        Vector<LegacyVaryingRequestHeader> badStorage(0);
        badStorage.buffer = reinterpret_cast<LegacyVaryingRequestHeader*>(pages.Guard());
        badStorage.capacity = 1;
        badStorage.size = 1;
        Require(ThrowsNotImplemented([&] { verify(nullptr, &badStorage, &request.base); }));
        badStorage.buffer = nullptr;
        badStorage.capacity = 0;
        badStorage.size = 0;
    }

    {
        Vector<LegacyVaryingRequestHeader> badEntry(1);
        badEntry.append(LegacyVaryingRequestHeader{Text("Accept"), Text("text/html")});
        StringImpl* const origName = std::exchange(badEntry.buffer[0].name.impl, reinterpret_cast<StringImpl*>(pages.Guard()));
        Require(ThrowsNotImplemented([&] { verify(nullptr, &badEntry, &request.base); }));
        StringImpl badData{2, 8, pages.Guard(), 4};
        badEntry.buffer[0].name.impl = &badData;
        Require(ThrowsNotImplemented([&] { verify(nullptr, &badEntry, &request.base); }));
        badEntry.buffer[0].name.impl = origName;
        StringImpl* const origVal = std::exchange(badEntry.buffer[0].value.impl, reinterpret_cast<StringImpl*>(pages.Guard()));
        Require(ThrowsNotImplemented([&] { verify(nullptr, &badEntry, &request.base); }));
        badEntry.buffer[0].value.impl = &badData;
        Require(ThrowsNotImplemented([&] { verify(nullptr, &badEntry, &request.base); }));
        badEntry.buffer[0].value.impl = origVal;
    }

    Vector<LegacyVaryingRequestHeader> cookieHeaders(1);
    cookieHeaders.append(LegacyVaryingRequestHeader{Text("Cookie"), Text("jar=1")});
    Require(ThrowsNotImplemented([&] { verify(unmappedJar, &cookieHeaders, &request.base); }));
    Require(ThrowsNotImplemented([&] { verify(&badVtableJar, &cookieHeaders, &request.base); }));
}

int main() {
    TestSameSiteInfo();
    TestVerify();
    TestNames();
    TestStrings();
    TestCookieJar();
    TestUpdatedFlagAndArguments();
    TestGuestPointers();
    std::puts("libSceNKWebKit varying request headers tests passed");
    return 0;
}

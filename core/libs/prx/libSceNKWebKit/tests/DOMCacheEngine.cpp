#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::CacheQueryOptions;
using NKWebKit::WebCoreAbi::LegacyResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringHashMap;
using NKWebKit::WtfAbi::URL;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "DOM cache engine check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

constexpr auto matchCached = &_ZN7WebCore14DOMCacheEngine15queryCacheMatchERKNS_15ResourceRequestES3_RKNS_16ResourceResponseERKNS_17CacheQueryOptionsE;
constexpr auto matchStored =
    &_ZN7WebCore14DOMCacheEngine15queryCacheMatchERKNS_15ResourceRequestERKN3WTF3URLEbRKNS4_7HashMapINS4_6StringES9_NS4_11DefaultHashIS9_EENS4_10HashTraitsIS9_EESD_EERKNS_17CacheQueryOptionsE;

template<typename TException, typename TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const TException&) {
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

template<typename TBase>
struct RequestOf {
    TBase base{};
    unsigned urlCount = 0;

    explicit RequestOf(std::string_view url, std::initializer_list<std::pair<std::string_view, std::string_view>> headers = {}) {
        base.url = Parse(url);
        base.httpMethod = Text("GET");
        base.resourceRequestUpdated = true;
        for (const auto& header : headers) _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&Map(), Text(header.first), Text(header.second));
        urlCount = base.url.impl ? base.url.impl->refCount : 0;
    }
    RequestOf(const RequestOf&) = delete;
    RequestOf& operator=(const RequestOf&) = delete;
    ~RequestOf() {
        ReleaseHeaders(Map().common);
        ReleaseHeaders(Map().uncommon);
        NKWebKit::WtfAbi::Release(base.url.impl);
    }

    HTTPHeaderMap& Map() {
        return *reinterpret_cast<HTTPHeaderMap*>(&base.httpHeaderFields);
    }

    void Snapshot() {
        urlCount = base.url.impl ? base.url.impl->refCount : 0;
    }

    bool Untouched() const {
        return !base.url.impl || base.url.impl->refCount == urlCount;
    }
};

using Request = RequestOf<ResourceRequestBase>;
using LegacyRequest = RequestOf<LegacyResourceRequestBase>;

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

struct Stored {
    StringHashMap map{nullptr};

    Stored(std::initializer_list<std::pair<std::string_view, std::string_view>> headers) {
        for (const auto& header : headers) {
            const String key = Text(header.first);
            const String value = Text(header.second);
            NKWebKit::WtfAbi::StringHashMapAdd(map, key.impl, value.impl);
        }
    }
    Stored(const Stored&) = delete;
    Stored& operator=(const Stored&) = delete;
    ~Stored() {
        NKWebKit::WtfAbi::StringHashMapClear(map);
    }
};

constexpr CacheQueryOptions kDefault{false, false, false};
constexpr CacheQueryOptions kIgnoreSearch{true, false, false};
constexpr CacheQueryOptions kIgnoreMethod{false, true, false};
constexpr CacheQueryOptions kIgnoreVary{false, false, true};
constexpr CacheQueryOptions kIgnoreSearchAndVary{true, false, true};

static bool Cached(std::string_view requestURL, std::string_view cachedURL, const CacheQueryOptions& options) {
    Request request(requestURL);
    Request cached(cachedURL);
    Response response;
    request.Snapshot();
    cached.Snapshot();
    const bool result = matchCached(&request.base, &cached.base, response.Base(), &options);
    Require(request.Untouched() && cached.Untouched());
    LegacyRequest legacy(requestURL);
    URL url = Parse(cachedURL);
    legacy.Snapshot();
    const unsigned urlCount = url.impl ? url.impl->refCount : 0;
    Stored none{};
    Require(matchStored(&legacy.base, &url, false, &none.map, &options) == result);
    Require(legacy.Untouched() && (!url.impl || url.impl->refCount == urlCount));
    NKWebKit::WtfAbi::Release(url.impl);
    return result;
}

static void TestTheURLs() {
    Require(Cached("https://a.example/page", "https://a.example/page", kDefault));
    Require(!Cached("https://a.example/page", "https://a.example/other", kDefault));
    Require(!Cached("https://a.example/page", "https://b.example/page", kDefault));
    Require(!Cached("https://a.example/page", "http://a.example/page", kDefault));
    Require(!Cached("https://a.example/page", "https://a.example:8443/page", kDefault));
    Require(Cached("https://a.example/page#top", "https://a.example/page#bottom", kDefault));
    Require(Cached("https://a.example/page#top", "https://a.example/page", kDefault));
    Require(Cached("https://a.example/page?x=1", "https://a.example/page?x=1", kDefault));
    Require(!Cached("https://a.example/page?x=1", "https://a.example/page?x=2", kDefault));
    Require(!Cached("https://a.example/page?x=1", "https://a.example/page", kDefault));
    Require(!Cached("https://a.example/page", "https://a.example/page?", kDefault));
    Require(Cached("https://a.example/page?x=1#a", "https://a.example/page?x=1#b", kDefault));

    Require(Cached("https://a.example/page?x=1", "https://a.example/page?x=2", kIgnoreSearch));
    Require(Cached("https://a.example/page?x=1", "https://a.example/page", kIgnoreSearch));
    Require(Cached("https://a.example/page", "https://a.example/page?y", kIgnoreSearch));
    Require(Cached("https://a.example/page?x=1#a", "https://a.example/page?y=2#b", kIgnoreSearch));
    Require(Cached("https://a.example/page?", "https://a.example/page", kIgnoreSearch));
    Require(!Cached("https://a.example/page?x=1", "https://a.example/other?x=1", kIgnoreSearch));
    Require(!Cached("https://a.example/page?x=1", "https://b.example/page?x=1", kIgnoreSearch));
    Require(Cached("https://a.example/page?x=1", "https://a.example/page?x=2", kIgnoreSearchAndVary));
    Require(!Cached("https://a.example/page?x=1", "https://a.example/page?x=2", kIgnoreVary));
    Require(!Cached("https://a.example/page?x=1", "https://a.example/page?x=2", kIgnoreMethod));
    Require(Cached("https://a.example/page", "https://a.example/page", kIgnoreMethod));
    Require(Cached("", "", kDefault));
    Require(!Cached("", "https://a.example/", kDefault));
    Require(Cached("not a url", "not a url", kIgnoreSearch));
}

using Headers = std::initializer_list<std::pair<std::string_view, std::string_view>>;

static bool Vary(const String& vary, Headers requestHeaders, Headers cachedHeaders, const CacheQueryOptions& options = kDefault) {
    Request request("https://a.example/page", requestHeaders);
    Request cached("https://a.example/page", cachedHeaders);
    Response response(vary);
    return matchCached(&request.base, &cached.base, response.Base(), &options);
}

static bool Vary(std::string_view vary, Headers requestHeaders, Headers cachedHeaders, const CacheQueryOptions& options = kDefault) {
    return Vary(Text(vary), requestHeaders, cachedHeaders, options);
}

static void TestTheVaryHeader() {
    Require(Vary("Accept", {{"Accept", "text/html"}}, {{"Accept", "text/html"}}));
    Require(!Vary("Accept", {{"Accept", "text/html"}}, {{"Accept", "image/png"}}));
    Require(!Vary("Accept", {{"Accept", "text/html"}}, {}));
    Require(!Vary("Accept", {}, {{"Accept", "text/html"}}));
    Require(Vary("Accept", {}, {}));
    Require(!Vary("Accept", {{"Accept", ""}}, {}));
    Require(Vary("accept", {{"Accept", "a"}}, {{"ACCEPT", "a"}}));
    Require(!Vary("Accept", {{"Accept", "a"}}, {{"Accept", "A"}}));
    Require(Vary("Accept", {{"Accept", "a"}, {"X-Other", "1"}}, {{"Accept", "a"}, {"X-Other", "2"}}));

    Require(Vary("Accept, X-Custom", {{"Accept", "a"}, {"X-Custom", "1"}}, {{"Accept", "a"}, {"X-Custom", "1"}}));
    Require(!Vary("Accept, X-Custom", {{"Accept", "a"}, {"X-Custom", "1"}}, {{"Accept", "a"}, {"X-Custom", "2"}}));
    Require(!Vary("Accept, X-Custom", {{"Accept", "b"}, {"X-Custom", "1"}}, {{"Accept", "a"}, {"X-Custom", "1"}}));
    Require(!Vary("Accept,X-Custom,Accept-Language", {{"Accept-Language", "fr"}}, {{"Accept-Language", "en"}}));
    Require(Vary(" Accept \t,\r\n X-Custom ", {{"Accept", "a"}, {"X-Custom", "1"}}, {{"Accept", "a"}, {"X-Custom", "1"}}));
    Require(!Vary(" Accept \t,\r\n X-Custom ", {{"Accept", "a"}, {"X-Custom", "1"}}, {{"Accept", "a"}, {"X-Custom", "2"}}));
    Require(!Vary("\tAccept", {{"Accept", "a"}}, {{"Accept", "b"}}));
    Require(!Vary("Accept\n", {{"Accept", "a"}}, {{"Accept", "b"}}));
    Require(Vary(",,Accept,,", {{"Accept", "a"}}, {{"Accept", "a"}}));
    Require(!Vary(",,Accept,,", {{"Accept", "a"}}, {{"Accept", "b"}}));
    Require(Vary("", {{"Accept", "a"}}, {{"Accept", "b"}}));
    Require(Vary(",", {{"Accept", "a"}}, {{"Accept", "b"}}));
    Require(Vary(" , ", {{"Accept", "a"}}, {{"Accept", "b"}}));
    Require(Vary("Accept;q=1", {{"Accept", "a"}}, {{"Accept", "b"}}));
    Require(Vary("Acc ept", {{"Accept", "a"}}, {{"Accept", "b"}}));
    Require(Vary("Accept\x0b", {{"Accept", "a"}}, {{"Accept", "b"}}));

    Require(Vary("A", {{"A", "1"}}, {{"A", "1"}}));
    Require(!Vary("A", {{"A", "1"}}, {{"A", "2"}}));
    Require(Vary("A,B", {{"A", "1"}, {"B", "2"}}, {{"A", "1"}, {"B", "2"}}));
    Require(Vary(",", {{"", "x"}}, {}));
    Require(Vary(",,", {{"", "x"}}, {{"", "y"}}));
    Require(!Vary(" , ", {{"", "x"}}, {}));
    Require(!Vary(" ", {{"", "x"}}, {{"", "y"}}));
    Require(Vary(" ", {{"", "x"}}, {{"", "x"}}));

    Require(!Vary("*", {}, {}));
    Require(!Vary(" * ", {{"Accept", "a"}}, {{"Accept", "a"}}));
    Require(!Vary("Accept, *", {{"Accept", "a"}}, {{"Accept", "a"}}));
    Require(!Vary("*, Accept", {{"Accept", "a"}}, {{"Accept", "a"}}));
    Require(Vary("**", {}, {}));
    Require(Vary("* *", {}, {}));
    Require(Vary("*", {}, {}, kIgnoreVary));
    Require(Vary("Accept", {{"Accept", "a"}}, {{"Accept", "b"}}, kIgnoreVary));
    Require(!Vary("Accept", {{"Accept", "a"}}, {{"Accept", "b"}}, kIgnoreSearch));
    Require(!Vary("Accept", {{"Accept", "a"}}, {{"Accept", "b"}}, kIgnoreMethod));

    Require(Vary(Wide(u"　Accept"), {{"Accept", "a"}}, {{"Accept", "b"}}));
    Require(Vary(Wide(u" Accept , X-Custom"), {{"Accept", "a"}, {"X-Custom", "1"}}, {{"Accept", "a"}, {"X-Custom", "1"}}));
    Require(!Vary(Wide(u" Accept , X-Custom"), {{"Accept", "a"}, {"X-Custom", "1"}}, {{"Accept", "a"}, {"X-Custom", "2"}}));
    Require(!Vary(Wide(u"*"), {}, {}));

    Request request("https://a.example/page", {{"Accept", "a"}});
    Request cached("https://a.example/page", {{"Accept", "b"}});
    Response none;
    Require(matchCached(&request.base, &cached.base, none.Base(), &kDefault));
    Request other("https://a.example/other", {{"Accept", "a"}});
    Response star("*");
    Require(!matchCached(&other.base, &cached.base, star.Base(), &kIgnoreVary));
}

static bool StoredMatch(Headers requestHeaders, bool hasVaryStar, Headers stored, const CacheQueryOptions& options = kDefault,
                        std::string_view requestURL = "https://a.example/page") {
    LegacyRequest request(requestURL, requestHeaders);
    URL url = Parse("https://a.example/page");
    Stored headers(stored);
    const bool result = matchStored(&request.base, &url, hasVaryStar, &headers.map, &options);
    NKWebKit::WtfAbi::Release(url.impl);
    return result;
}

static void TestTheStoredHeaders() {
    Require(StoredMatch({}, false, {}));
    Require(StoredMatch({{"Accept", "a"}}, false, {}));
    Require(StoredMatch({{"Accept", "a"}}, false, {{"Accept", "a"}}));
    Require(StoredMatch({{"accept", "a"}}, false, {{"ACCEPT", "a"}}));
    Require(!StoredMatch({{"Accept", "a"}}, false, {{"Accept", "b"}}));
    Require(!StoredMatch({}, false, {{"Accept", "a"}}));
    Require(!StoredMatch({}, false, {{"Accept", ""}}));
    Require(StoredMatch({{"Accept", ""}}, false, {{"Accept", ""}}));
    Require(StoredMatch({{"Accept", "a"}, {"X-Custom", "1"}, {"X-Other", "9"}}, false, {{"Accept", "a"}, {"X-Custom", "1"}}));
    Require(!StoredMatch({{"Accept", "a"}, {"X-Custom", "1"}}, false, {{"Accept", "a"}, {"X-Custom", "2"}}));
    Require(!StoredMatch({{"Accept", "b"}, {"X-Custom", "1"}}, false, {{"Accept", "a"}, {"X-Custom", "1"}}));
    Require(!StoredMatch({{"Accept", "a"}}, true, {{"Accept", "a"}}));
    Require(!StoredMatch({}, true, {}));
    Require(StoredMatch({}, true, {}, kIgnoreVary));
    Require(StoredMatch({{"Accept", "a"}}, false, {{"Accept", "b"}}, kIgnoreVary));
    Require(!StoredMatch({{"Accept", "a"}}, false, {{"Accept", "b"}}, kIgnoreSearch));
    Require(!StoredMatch({{"Accept", "a"}}, false, {{"Accept", "a"}}, kIgnoreVary, "https://a.example/other"));
    Require(!StoredMatch({{"Accept", "a"}}, false, {{"Accept", "a"}}, kDefault, "https://a.example/page?x"));
    Require(StoredMatch({{"Accept", "a"}}, false, {{"Accept", "a"}}, kIgnoreSearch, "https://a.example/page?x"));

    LegacyRequest request("https://a.example/page", {{"H3", "v3"}, {"H17", "v17"}, {"H29", "v29"}});
    URL url = Parse("https://a.example/page");
    StringHashMap map{nullptr};
    for (int index = 0; index < 40; ++index) {
        const String key = Text("H" + std::to_string(index));
        const String value = Text("v" + std::to_string(index));
        NKWebKit::WtfAbi::StringHashMapAdd(map, key.impl, value.impl);
        _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&request.Map(), key, value);
    }
    const unsigned keyCount = NKWebKit::WtfAbi::StringHashMapKeyCount(map);
    Require(keyCount == 40 && matchStored(&request.base, &url, false, &map, &kDefault));
    for (const int removed : {0, 7, 21, 39}) {
        const String key = Text("H" + std::to_string(removed));
        NKWebKit::WtfAbi::StringHashMapRemove(map, NKWebKit::WtfAbi::StringHashMapFind(map, key.impl));
    }
    const auto counts = [&] {
        std::string text;
        for (NKWebKit::WtfAbi::StringHashMapIterator iterator = NKWebKit::WtfAbi::StringHashMapBegin(map);
             iterator.position != NKWebKit::WtfAbi::StringHashMapEnd(map).position; NKWebKit::WtfAbi::StringHashMapAdvance(iterator))
            text += std::to_string(iterator.position->key->refCount) + "/" + std::to_string(iterator.position->value->refCount) + " ";
        return text;
    };
    const std::string countsBefore = counts();
    Require(NKWebKit::WtfAbi::StringHashMapKeyCount(map) == 36 && matchStored(&request.base, &url, false, &map, &kDefault));
    Require(counts() == countsBefore);
    for (const int changed : {1, 20, 38}) {
        const String key = Text("H" + std::to_string(changed));
        const String original = Text("v" + std::to_string(changed));
        _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&request.Map(), key, Text("other"));
        const std::string changedCounts = counts();
        Require(!matchStored(&request.base, &url, false, &map, &kDefault));
        Require(counts() == changedCounts);
        _ZN7WebCore13HTTPHeaderMap3setERKN3WTF6StringES4_(&request.Map(), key, original);
        const std::string restoredCounts = counts();
        Require(matchStored(&request.base, &url, false, &map, &kDefault));
        Require(counts() == restoredCounts);
    }
    NKWebKit::WtfAbi::StringHashMapClear(map);
    NKWebKit::WtfAbi::Release(url.impl);
}

static void TestInvalidArguments() {
    Request request("https://a.example/page");
    Request cached("https://a.example/page");
    Response response("Accept");
    LegacyRequest legacy("https://a.example/page");
    URL url = Parse("https://a.example/page");
    Stored headers{{"Accept", "a"}};
    using std::invalid_argument;
    Require(Throws<invalid_argument>([&] { matchCached(nullptr, &cached.base, response.Base(), &kDefault); }));
    Require(Throws<invalid_argument>([&] { matchCached(&request.base, nullptr, response.Base(), &kDefault); }));
    Require(Throws<invalid_argument>([&] { matchCached(&request.base, &cached.base, nullptr, &kDefault); }));
    Require(Throws<invalid_argument>([&] { matchCached(&request.base, &cached.base, response.Base(), nullptr); }));
    Require(Throws<invalid_argument>([&] { matchStored(nullptr, &url, false, &headers.map, &kDefault); }));
    Require(Throws<invalid_argument>([&] { matchStored(&legacy.base, nullptr, false, &headers.map, &kDefault); }));
    Require(Throws<invalid_argument>([&] { matchStored(&legacy.base, &url, false, nullptr, &kDefault); }));
    Require(Throws<invalid_argument>([&] { matchStored(&legacy.base, &url, false, &headers.map, nullptr); }));

    Response none;
    Stored empty{};
    request.base.resourceRequestUpdated = false;
    Require(Throws<std::logic_error>([&] { matchCached(&request.base, &cached.base, response.Base(), &kDefault); }));
    Require(Throws<std::logic_error>([&] { matchCached(&request.base, &cached.base, none.Base(), &kDefault); }));
    Require(Throws<std::logic_error>([&] { matchCached(&request.base, &cached.base, response.Base(), &kIgnoreVary); }));
    request.base.resourceRequestUpdated = true;
    cached.base.resourceRequestUpdated = false;
    Require(Throws<std::logic_error>([&] { matchCached(&request.base, &cached.base, response.Base(), &kDefault); }));
    Require(Throws<std::logic_error>([&] { matchCached(&request.base, &cached.base, none.Base(), &kDefault); }));
    Require(Throws<std::logic_error>([&] { matchCached(&request.base, &cached.base, response.Base(), &kIgnoreVary); }));
    cached.base.resourceRequestUpdated = true;
    Require(matchCached(&request.base, &cached.base, response.Base(), &kDefault));
    legacy.base.resourceRequestUpdated = false;
    Require(Throws<std::logic_error>([&] { matchStored(&legacy.base, &url, false, &headers.map, &kDefault); }));
    Require(Throws<std::logic_error>([&] { matchStored(&legacy.base, &url, false, &empty.map, &kIgnoreVary); }));
    Require(Throws<std::logic_error>([&] { matchStored(&legacy.base, &url, true, &empty.map, &kDefault); }));
    legacy.base.resourceRequestUpdated = true;
    Require(matchStored(&legacy.base, &url, false, &empty.map, &kDefault));
    Require(request.Untouched() && cached.Untouched() && legacy.Untouched());
    NKWebKit::WtfAbi::Release(url.impl);
}

int main() {
    TestTheURLs();
    TestTheVaryHeader();
    TestTheStoredHeaders();
    TestInvalidArguments();
    std::puts("libSceNKWebKit DOM cache engine tests passed");
    return 0;
}

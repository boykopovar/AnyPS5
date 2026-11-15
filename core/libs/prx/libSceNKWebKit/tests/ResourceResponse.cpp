#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceResponseBase check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static StringImpl Make(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

static void TestFields() {
    auto urlText = Make("http://a/");
    auto mime = Make("text/html");
    auto encoding = Make("utf-8");
    auto status = Make("OK");
    auto version = Make("HTTP/1.1");
    ResourceResponseBase response{};
    response.url.impl = &urlText;
    response.url.bits = 1 | (4 << 6);
    response.mimeType.impl = &mime;
    response.expectedContentLength = 1234567890123LL;
    response.textEncodingName.impl = &encoding;
    response.httpStatusText.impl = &status;
    response.httpVersion.impl = &version;
    response.httpHeaderFields.common.capacity = 9;

    Require(&_ZNK7WebCore20ResourceResponseBase3urlEv(&response) == &response.url);
    Require(&_ZNK7WebCore20ResourceResponseBase8mimeTypeEv(&response) == &response.mimeType);
    Require(_ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(&response) == 1234567890123LL);
    Require(&_ZNK7WebCore20ResourceResponseBase16textEncodingNameEv(&response) == &response.textEncodingName);
    Require(&_ZNK7WebCore20ResourceResponseBase14httpStatusTextEv(&response) == &response.httpStatusText);
    Require(&_ZNK7WebCore20ResourceResponseBase11httpVersionEv(&response) == &response.httpVersion);
    Require(&_ZNK7WebCore20ResourceResponseBase16httpHeaderFieldsEv(&response) == &response.httpHeaderFields);
    Require(_ZNK7WebCore20ResourceResponseBase16httpHeaderFieldsEv(&response).common.capacity == 9);
    Require(urlText.refCount == 2 && mime.refCount == 2);
    response.expectedContentLength = -1;
    Require(_ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(&response) == -1);
}

static void TestHTTPFamily() {
    auto http = Make("http://a/");
    auto https = Make("HTTPS://a/");
    auto file = Make("file:///a");
    auto ftp = Make("ftp://a/");
    ResourceResponseBase response{};
    Require(!_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &http;
    response.url.bits = 1 | (4 << 6);
    Require(_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &https;
    Require(_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &file;
    Require(!_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &ftp;
    Require(!_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &http;
    response.url.bits = 0;
    Require(!_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
}

static void TestNull() {
    bool thrown = false;
    try {
        _ZNK7WebCore20ResourceResponseBase3urlEv(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
}

using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WtfAbi::String;

static String Text(std::string_view value) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(value.data()), static_cast<unsigned>(value.size()));
}

static String Text(std::u16string_view value) {
    return _ZN3WTF10StringImpl6createEPKDsj(value.data(), static_cast<unsigned>(value.size()));
}

static std::u16string Contents(const String& value) {
    Require(value.impl != nullptr);
    if (value.impl->hashAndFlags & 4) {
        const auto* bytes = static_cast<const unsigned char*>(value.impl->data);
        return {bytes, bytes + value.impl->length};
    }
    return {static_cast<const char16_t*>(value.impl->data), value.impl->length};
}

template<class Entry>
static void ReleaseHeaders(NKWebKit::HttpAbi::HeaderVector<Entry>& entries) {
    if (entries.buffer) {
        std::destroy_n(entries.buffer, entries.size);
        _ZN3WTF8fastFreeEPv(entries.buffer);
    }
}

struct ResponseFixture {
    ResourceResponseBase response{};

    ~ResponseFixture() {
        ReleaseHeaders(response.httpHeaderFields.common);
        ReleaseHeaders(response.httpHeaderFields.uncommon);
    }

    void Set(HTTPHeaderName header, const String& value) {
        _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&response, header, value);
    }

    String Get(HTTPHeaderName header) const {
        return _ZNK7WebCore20ResourceResponseBase15httpHeaderFieldENS_14HTTPHeaderNameE(&response, header);
    }
};

static void TestHeaderMutation() {
    ResponseFixture fixture;
    const auto* borrowed = &_ZNK7WebCore20ResourceResponseBase16httpHeaderFieldsEv(&fixture.response);
    Require(!fixture.Get(HTTPHeaderName::ContentType).impl);

    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldERKN3WTF6StringES4_(
        &fixture.response, Text(u"cOnTeNt-tYpE"), Text("text/plain"));
    _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(
        &fixture.response, HTTPHeaderName::ContentType, Text(u"text/html"));
    Require(Contents(fixture.Get(HTTPHeaderName::ContentType)) == u"text/plain, text/html");
    Require(borrowed->common.size == 1);

    _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldERKN3WTF6StringES4_(
        &fixture.response, Text("X-Example"), Text("one"));
    _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldERKN3WTF6StringES4_(
        &fixture.response, Text(u"x-example"), Text("two"));
    Require(borrowed->uncommon.size == 1);
    Require(Contents(borrowed->uncommon.buffer[0].value) == u"one, two");
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldERKN3WTF6StringES4_(
        &fixture.response, Text("X-EXAMPLE"), Text("replacement"));
    Require(borrowed->uncommon.size == 1);
    Require(Contents(borrowed->uncommon.buffer[0].value) == u"replacement");

    const auto oldValue = fixture.Get(HTTPHeaderName::ContentType);
    fixture.Set(HTTPHeaderName::ContentType, Text("text/xml"));
    Require(Contents(oldValue) == u"text/plain, text/html");
    Require(Contents(fixture.Get(HTTPHeaderName::ContentType)) == u"text/xml");
}

static void TestCacheControl() {
    ResponseFixture fixture;
    const auto* response = &fixture.response;
    Require(!_ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoCacheEv(response));
    Require(!_ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoStoreEv(response));
    Require(!_ZNK7WebCore20ResourceResponseBase29cacheControlContainsImmutableEv(response));
    Require(!_ZNK7WebCore20ResourceResponseBase34cacheControlContainsMustRevalidateEv(response));
    Require(!_ZNK7WebCore20ResourceResponseBase18cacheControlMaxAgeEv(response).hasValue);
    Require(!_ZNK7WebCore20ResourceResponseBase32cacheControlStaleWhileRevalidateEv(response).hasValue);
    Require(!_ZNK7WebCore20ResourceResponseBase23hasCacheValidatorFieldsEv(response));

    fixture.Set(HTTPHeaderName::Pragma, Text("no-cache"));
    Require(_ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoCacheEv(response));
    fixture.Set(HTTPHeaderName::CacheControl, Text(u"max-age=60, stale-while-revalidate=0, no-store, immutable, must-revalidate"));
    const auto maxAge = _ZNK7WebCore20ResourceResponseBase18cacheControlMaxAgeEv(response);
    const auto stale = _ZNK7WebCore20ResourceResponseBase32cacheControlStaleWhileRevalidateEv(response);
    Require(maxAge.hasValue && maxAge.seconds == 60);
    Require(stale.hasValue && stale.seconds == 0);
    Require(_ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoStoreEv(response));
    Require(_ZNK7WebCore20ResourceResponseBase29cacheControlContainsImmutableEv(response));
    Require(_ZNK7WebCore20ResourceResponseBase34cacheControlContainsMustRevalidateEv(response));

    fixture.Set(HTTPHeaderName::Pragma, Text(""));
    fixture.Set(HTTPHeaderName::CacheControl, Text("max-age=broken"));
    Require(!_ZNK7WebCore20ResourceResponseBase18cacheControlMaxAgeEv(response).hasValue);
    Require(!_ZNK7WebCore20ResourceResponseBase27cacheControlContainsNoCacheEv(response));
    fixture.Set(HTTPHeaderName::ETag, Text("\"abc\""));
    Require(_ZNK7WebCore20ResourceResponseBase23hasCacheValidatorFieldsEv(response));
    fixture.Set(HTTPHeaderName::ETag, Text(""));
    fixture.Set(HTTPHeaderName::LastModified, Text("invalid date"));
    Require(_ZNK7WebCore20ResourceResponseBase23hasCacheValidatorFieldsEv(response));
}

static void TestTimes() {
    ResponseFixture fixture;
    const auto* response = &fixture.response;
    Require(!_ZNK7WebCore20ResourceResponseBase4dateEv(response).hasValue);
    Require(!_ZNK7WebCore20ResourceResponseBase7expiresEv(response).hasValue);
    Require(!_ZNK7WebCore20ResourceResponseBase12lastModifiedEv(response).hasValue);
    Require(!_ZNK7WebCore20ResourceResponseBase3ageEv(response).hasValue);

    fixture.Set(HTTPHeaderName::Date, Text("Sun, 06 Nov 1994 08:49:37 GMT"));
    fixture.Set(HTTPHeaderName::Expires, Text(u"Sunday, 06-Nov-94 08:49:37 GMT"));
    fixture.Set(HTTPHeaderName::LastModified, Text("Thu, 01 Jan 1970 00:00:00 GMT"));
    fixture.Set(HTTPHeaderName::Age, Text(u"  -2.5"));
    const auto date = _ZNK7WebCore20ResourceResponseBase4dateEv(response);
    const auto expires = _ZNK7WebCore20ResourceResponseBase7expiresEv(response);
    const auto modified = _ZNK7WebCore20ResourceResponseBase12lastModifiedEv(response);
    const auto age = _ZNK7WebCore20ResourceResponseBase3ageEv(response);
    Require(date.hasValue && date.seconds == 784111777);
    Require(expires.hasValue && expires.seconds == date.seconds);
    Require(modified.hasValue && modified.seconds == 0);
    Require(age.hasValue && age.seconds == -2.5);

    fixture.Set(HTTPHeaderName::Date, Text("invalid"));
    fixture.Set(HTTPHeaderName::Age, Text("10 "));
    Require(!_ZNK7WebCore20ResourceResponseBase4dateEv(response).hasValue);
    Require(!_ZNK7WebCore20ResourceResponseBase3ageEv(response).hasValue);
    fixture.Set(HTTPHeaderName::Age, Text("10x"));
    Require(!_ZNK7WebCore20ResourceResponseBase3ageEv(response).hasValue);
}

static void TestDisposition() {
    ResponseFixture fixture;
    const auto* response = &fixture.response;
    Require(!_ZNK7WebCore20ResourceResponseBase12isAttachmentEv(response));
    Require(!_ZNK7WebCore20ResourceResponseBase24isAttachmentWithFilenameEv(response));
    Require(!_ZNK7WebCore20ResourceResponseBase17suggestedFilenameEv(response).impl);

    fixture.Set(HTTPHeaderName::ContentDisposition, Text(u"  AtTaChMeNt ; filename=\"caf\u00e9.txt\""));
    Require(_ZNK7WebCore20ResourceResponseBase12isAttachmentEv(response));
    Require(_ZNK7WebCore20ResourceResponseBase24isAttachmentWithFilenameEv(response));
    Require(Contents(_ZNK7WebCore20ResourceResponseBase17suggestedFilenameEv(response)) == u"caf\u00e9.txt");
    fixture.Set(HTTPHeaderName::ContentDisposition, Text("inline; filename=plain.txt"));
    Require(!_ZNK7WebCore20ResourceResponseBase24isAttachmentWithFilenameEv(response));
    Require(Contents(_ZNK7WebCore20ResourceResponseBase17suggestedFilenameEv(response)) == u"plain.txt");
    fixture.Set(HTTPHeaderName::ContentDisposition, Text("attachment; Filename=ignored"));
    Require(_ZNK7WebCore20ResourceResponseBase12isAttachmentEv(response));
    Require(!_ZNK7WebCore20ResourceResponseBase24isAttachmentWithFilenameEv(response));
    fixture.Set(HTTPHeaderName::ContentDisposition, Text("attachment; filename=\"\""));
    Require(_ZNK7WebCore20ResourceResponseBase24isAttachmentWithFilenameEv(response));
    Require(Contents(_ZNK7WebCore20ResourceResponseBase17suggestedFilenameEv(response)).empty());
    fixture.Set(HTTPHeaderName::ContentDisposition, Text("attachment; filename=\""));
    Require(Contents(_ZNK7WebCore20ResourceResponseBase17suggestedFilenameEv(response)) == u"\"");
    fixture.Set(HTTPHeaderName::ContentDisposition, Text(u"\u2003attachment; filename=x"));
    Require(!_ZNK7WebCore20ResourceResponseBase12isAttachmentEv(response));
    fixture.Set(HTTPHeaderName::ContentDisposition, Text("attachment; filename=caf\xc3\xa9.txt"));
    Require(Contents(_ZNK7WebCore20ResourceResponseBase17suggestedFilenameEv(response)) == u"caf\u00e9.txt");
    fixture.Set(HTTPHeaderName::ContentDisposition, Text("attachment; filename=caf\xe9.txt"));
    Require(Contents(_ZNK7WebCore20ResourceResponseBase17suggestedFilenameEv(response)) == u"caf\u00e9.txt");

    auto sanitize = _ZN7WebCore20ResourceResponseBase25sanitizeSuggestedFilenameERKN3WTF6StringE;
    Require(!sanitize(String()).impl);
    Require(Contents(sanitize(Text(u"caf\u00e9.txt"))) == u"caf\u00e9.txt");
    Require(Contents(sanitize(Text("a\\b\"c"))) == u"a\\\\b\\\"c");
    Require(Contents(sanitize(Text("name;rest"))) == u"nam");
}

static void TestHTTP09() {
    auto version = Make("HTTP/0.9");
    auto suffix = Make("HTTP/0.9 extra");
    auto other = Make("HTTP/1.0");
    ResourceResponseBase response{};
    Require(!_ZNK7WebCore20ResourceResponseBase8isHTTP09Ev(&response));
    response.httpVersion.impl = &version;
    Require(_ZNK7WebCore20ResourceResponseBase8isHTTP09Ev(&response));
    response.httpVersion.impl = &suffix;
    Require(_ZNK7WebCore20ResourceResponseBase8isHTTP09Ev(&response));
    response.httpVersion.impl = &other;
    Require(!_ZNK7WebCore20ResourceResponseBase8isHTTP09Ev(&response));
}

static double Freshness(const ResourceResponseBase& response, double responseTime) {
    return _ZN7WebCore37computeFreshnessLifetimeForHTTPFamilyERKNS_16ResourceResponseEN3WTF8WallTimeE(response, WTF::WallTime{responseTime}).m_value;
}

static void TestFreshnessLifetime() {
    auto http = Make("http://a/");
    auto file = Make("file:///a");
    ResponseFixture fixture;
    auto& response = fixture.response;
    response.url.impl = &file;
    response.url.bits = 1 | (4 << 6);
    fixture.Set(HTTPHeaderName::CacheControl, Text("max-age=60"));
    Require(Freshness(response, 1000) == 0);

    response.url.impl = &http;
    fixture.Set(HTTPHeaderName::Date, Text("Sun, 06 Nov 1994 08:49:07 GMT"));
    fixture.Set(HTTPHeaderName::Expires, Text("Sun, 06 Nov 1994 08:49:37 GMT"));
    Require(Freshness(response, 0) == 60);
    fixture.Set(HTTPHeaderName::CacheControl, Text("no-cache"));
    Require(Freshness(response, 0) == 30);
    fixture.Set(HTTPHeaderName::Date, Text(""));
    Require(Freshness(response, 784111777 + 100) == -100);
    Require(Freshness(response, 784111777 - 15) == 15);

    fixture.Set(HTTPHeaderName::Expires, Text(""));
    Require(Freshness(response, 784111777) == 0);
    fixture.Set(HTTPHeaderName::LastModified, Text("Sun, 06 Nov 1994 07:49:37 GMT"));
    Require(Freshness(response, 784111777) == 360);
    fixture.Set(HTTPHeaderName::Date, Text("Sun, 06 Nov 1994 08:49:37 GMT"));
    Require(Freshness(response, 0) == 360);
    response.httpStatusCode = 301;
    Require(Freshness(response, 0) == 31536000);
    response.httpStatusCode = 410;
    Require(Freshness(response, 0) == 31536000);
    response.httpStatusCode = 404;
    Require(Freshness(response, 0) == 360);
    fixture.Set(HTTPHeaderName::Expires, Text("Sun, 06 Nov 1994 08:50:37 GMT"));
    response.httpStatusCode = 301;
    Require(Freshness(response, 0) == 60);
    fixture.Set(HTTPHeaderName::CacheControl, Text("max-age=5"));
    Require(Freshness(response, 0) == 5);
    response.url.bits = 0;
    Require(Freshness(response, 0) == 0);
}

static void SetUncommon(ResponseFixture& fixture, std::u16string_view name, std::string_view value) {
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&fixture.response, Text(name), Text(value));
}

static String Uncommon(const ResponseFixture& fixture, std::string_view name) {
    return _ZNK7WebCore13HTTPHeaderMap17getUncommonHeaderERKN3WTF6StringE(&fixture.response.httpHeaderFields, Text(name));
}

static void TestRevalidationHeaders() {
    ResponseFixture cached;
    cached.Set(HTTPHeaderName::ContentType, Text("text/html"));
    cached.Set(HTTPHeaderName::ETag, Text("\"old\""));
    cached.Set(HTTPHeaderName::CacheControl, Text("max-age=10"));
    SetUncommon(cached, u"X-Custom", "old");

    ResponseFixture validating;
    validating.Set(HTTPHeaderName::ContentType, Text("text/plain"));
    validating.Set(HTTPHeaderName::ETag, Text("\"new\""));
    validating.Set(HTTPHeaderName::LastModified, Text("Sun, 06 Nov 1994 08:49:37 GMT"));
    validating.Set(HTTPHeaderName::KeepAlive, Text("timeout=5"));
    validating.Set(HTTPHeaderName::XFrameOptions, Text("DENY"));
    validating.Set(HTTPHeaderName::XContentTypeOptions, Text("nosniff"));
    validating.Set(HTTPHeaderName::CacheControl, Text("max-age=60"));
    validating.Set(HTTPHeaderName::ContentSecurityPolicy, Text("default-src 'self'"));
    validating.Set(HTTPHeaderName::Vary, Text("Accept"));
    SetUncommon(validating, u"x-CUSTOM", "new");
    SetUncommon(validating, u"X-WebKit-Foo", "ignored");
    SetUncommon(validating, u"X-WEBKIT-Bar", "ignored");
    SetUncommon(validating, u"X-Content-Foo", "ignored");
    SetUncommon(validating, u"Content-Foo", "ignored");
    SetUncommon(validating, u"CONTENT-SECURITY-Extra", "kept");
    SetUncommon(validating, u"X-Other", "1");

    _ZN7WebCore38updateResponseHeadersAfterRevalidationERNS_16ResourceResponseERKS0_(cached.response, validating.response);

    Require(Contents(cached.Get(HTTPHeaderName::ContentType)) == u"text/html");
    Require(Contents(cached.Get(HTTPHeaderName::ETag)) == u"\"old\"");
    Require(!cached.Get(HTTPHeaderName::LastModified).impl);
    Require(!cached.Get(HTTPHeaderName::KeepAlive).impl);
    Require(!cached.Get(HTTPHeaderName::XFrameOptions).impl);
    Require(!cached.Get(HTTPHeaderName::XContentTypeOptions).impl);
    Require(Contents(cached.Get(HTTPHeaderName::CacheControl)) == u"max-age=60");
    Require(Contents(cached.Get(HTTPHeaderName::ContentSecurityPolicy)) == u"default-src 'self'");
    Require(Contents(cached.Get(HTTPHeaderName::Vary)) == u"Accept");
    Require(cached.response.httpHeaderFields.common.size == 5);

    const auto& uncommon = cached.response.httpHeaderFields.uncommon;
    Require(uncommon.size == 3);
    Require(Contents(uncommon.buffer[0].key) == u"X-Custom");
    Require(Contents(uncommon.buffer[0].value) == u"new");
    Require(Contents(uncommon.buffer[1].key) == u"CONTENT-SECURITY-Extra");
    Require(Contents(uncommon.buffer[1].value) == u"kept");
    Require(Contents(uncommon.buffer[2].key) == u"X-Other");
    Require(Contents(Uncommon(cached, "x-other")) == u"1");
    Require(!Uncommon(cached, "X-WebKit-Foo").impl);
    Require(!Uncommon(cached, "Content-Foo").impl);
    Require(validating.response.httpHeaderFields.uncommon.size == 7);
    Require(Contents(validating.Get(HTTPHeaderName::ContentType)) == u"text/plain");

    ResponseFixture empty;
    _ZN7WebCore38updateResponseHeadersAfterRevalidationERNS_16ResourceResponseERKS0_(cached.response, empty.response);
    Require(cached.response.httpHeaderFields.common.size == 5);
    Require(uncommon.size == 3);
}

using namespace NKWebKit::WebCoreAbi;

static bool IsNull(const ResourceResponseBase& response) {
    return response.stateBits & responseNullMask;
}

template<class Call>
static bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void TestDefaultConstruction() {
    for (auto construct : {_ZN7WebCore20ResourceResponseBaseC1Ev, _ZN7WebCore20ResourceResponseBaseC2Ev}) {
        alignas(ResourceResponseBase) unsigned char storage[sizeof(ResourceResponseBase)];
        std::memset(storage, 0xAB, sizeof(storage));
        auto& response = *reinterpret_cast<ResourceResponseBase*>(storage);
        construct(&response);

        Require(!response.url.impl && response.url.bits == 0 && response.url.queryEnd == 0);
        Require(!response.mimeType.impl && !response.textEncodingName.impl);
        Require(!response.httpStatusText.impl && !response.httpVersion.impl);
        Require(_ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(&response) == 0);
        Require(!response.httpHeaderFields.common.buffer && !response.httpHeaderFields.common.capacity);
        Require(!response.httpHeaderFields.common.size && !response.httpHeaderFields.uncommon.size);
        Require(!response.httpHeaderFields.uncommon.buffer && !response.httpHeaderFields.uncommon.capacity);
        Require(!response.networkLoadMetrics);
        Require(!response.certificateInfo.hasValue);
        Require(response.httpStatusCode == 0);
        Require(std::isnan(response.age) && std::isnan(response.date));
        Require(std::isnan(response.expires) && std::isnan(response.lastModified));
        Require(response.contentRange.firstBytePosition == 0 && response.contentRange.lastBytePosition == 0);
        Require(response.contentRange.instanceLength == ParsedContentRange::invalidLength);
        const auto& directives = response.cacheControlDirectives;
        Require(std::isnan(directives.maxAge) && std::isnan(directives.maxStale) && std::isnan(directives.staleWhileRevalidate));
        Require(!directives.noCache && !directives.noStore && !directives.mustRevalidate && !directives.immutable);
        Require(response.stateBits == responseNullMask && response.parseBits == 0 && response.taintingSourceBits == 0);
        Require(response.typeBits == static_cast<std::uint8_t>(ResponseType::Default));
        Require(IsNull(response));
        Require(_ZNK7WebCore20ResourceResponseBase14httpStatusCodeEv(&response) == 0);
        Require(_ZNK7WebCore20ResourceResponseBase6sourceEv(&response) == ResponseSource::Unknown);
        Require(!_ZNK7WebCore20ResourceResponseBase12isSuccessfulEv(&response));
        Require(!_ZNK7WebCore20ResourceResponseBase13isRedirectionEv(&response));
    }
    Require(Throws([] { _ZN7WebCore20ResourceResponseBaseC1Ev(nullptr); }));
    Require(Throws([] { _ZN7WebCore20ResourceResponseBaseC2Ev(nullptr); }));
}

static void TestStatusCode() {
    ResourceResponseBase response{};
    Require(IsNull(response));
    _ZN7WebCore20ResourceResponseBase17setHTTPStatusCodeEi(&response, 200);
    Require(!IsNull(response));
    Require(_ZNK7WebCore20ResourceResponseBase14httpStatusCodeEv(&response) == 200);
    _ZN7WebCore20ResourceResponseBase17setHTTPStatusCodeEi(&response, -1);
    Require(_ZNK7WebCore20ResourceResponseBase14httpStatusCodeEv(&response) == -1);
    _ZN7WebCore20ResourceResponseBase17setHTTPStatusCodeEi(&response, 70000);
    Require(_ZNK7WebCore20ResourceResponseBase14httpStatusCodeEv(&response) == 4464);

    for (int code = 0; code < 600; ++code) {
        _ZN7WebCore20ResourceResponseBase17setHTTPStatusCodeEi(&response, code);
        Require(_ZNK7WebCore20ResourceResponseBase14httpStatusCodeEv(&response) == code);
        Require(_ZNK7WebCore20ResourceResponseBase12isSuccessfulEv(&response) == (code >= 200 && code < 300));
        Require(_ZNK7WebCore20ResourceResponseBase13isRedirectionEv(&response)
            == (code == 301 || code == 302 || code == 303 || code == 307 || code == 308));
    }

    ResourceResponseBase untouched{};
    Require(!_ZNK7WebCore20ResourceResponseBase13isRedirectionEv(&untouched));
    Require(IsNull(untouched));
    Require(Throws([] { _ZNK7WebCore20ResourceResponseBase14httpStatusCodeEv(nullptr); }));
    Require(Throws([] { _ZN7WebCore20ResourceResponseBase17setHTTPStatusCodeEi(nullptr, 200); }));
    Require(Throws([] { _ZNK7WebCore20ResourceResponseBase12isSuccessfulEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore20ResourceResponseBase13isRedirectionEv(nullptr); }));
}

static void TestSetURL() {
    auto first = Make("http://a/");
    auto second = Make("https://b/path?q");
    NKWebKit::WtfAbi::URL url{&first, 1 | (4 << 6), 1, 2, 3, 4, 5, 6, 7};
    ResourceResponseBase response{};
    Require(IsNull(response));

    _ZN7WebCore20ResourceResponseBase6setURLERKN3WTF3URLE(&response, url);
    Require(!IsNull(response));
    Require(response.url.impl == &first && first.refCount == 4);
    Require(response.url.bits == url.bits && response.url.userStart == 1 && response.url.userEnd == 2);
    Require(response.url.passwordEnd == 3 && response.url.hostEnd == 4 && response.url.pathAfterLastSlash == 5);
    Require(response.url.pathEnd == 6 && response.url.queryEnd == 7);

    _ZN7WebCore20ResourceResponseBase6setURLERKN3WTF3URLE(&response, response.url);
    Require(response.url.impl == &first && first.refCount == 4 && response.url.queryEnd == 7);

    url.impl = &second;
    url.queryEnd = 9;
    _ZN7WebCore20ResourceResponseBase6setURLERKN3WTF3URLE(&response, url);
    Require(response.url.impl == &second && second.refCount == 4 && first.refCount == 2 && response.url.queryEnd == 9);

    _ZN7WebCore20ResourceResponseBase6setURLERKN3WTF3URLE(&response, NKWebKit::WtfAbi::URL{});
    Require(!response.url.impl && second.refCount == 2 && response.url.bits == 0);
    Require(Throws([&] { _ZN7WebCore20ResourceResponseBase6setURLERKN3WTF3URLE(nullptr, url); }));
}

static void TestExpectedContentLength() {
    ResourceResponseBase response{};
    Require(IsNull(response));
    _ZN7WebCore20ResourceResponseBase24setExpectedContentLengthEx(&response, -1);
    Require(!IsNull(response));
    Require(_ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(&response) == -1);
    _ZN7WebCore20ResourceResponseBase24setExpectedContentLengthEx(&response, 1234567890123LL);
    Require(_ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(&response) == 1234567890123LL);
    Require(Throws([] { _ZN7WebCore20ResourceResponseBase24setExpectedContentLengthEx(nullptr, 1); }));
}

static void TestTypeAndSource() {
    ResourceResponseBase response{};
    Require(IsNull(response));
    for (auto type : {ResponseType::Basic, ResponseType::Cors, ResponseType::Error, ResponseType::Opaque, ResponseType::Opaqueredirect, ResponseType::Default}) {
        response.stateBits = 0xFF;
        response.parseBits = 0xFF;
        response.taintingSourceBits = 0x10;
        response.typeBits = 0xF8;
        _ZN7WebCore20ResourceResponseBase7setTypeENS0_4TypeE(&response, type);
        Require(!IsNull(response));
        Require(response.stateBits == 0xFE && response.parseBits == 0xFF && response.taintingSourceBits == 0x10);
        Require(response.typeBits == (0xF8 | static_cast<unsigned>(type)));
        Require(_ZNK7WebCore20ResourceResponseBase6sourceEv(&response) == ResponseSource::MemoryCache);
    }
    for (unsigned source = 0; source <= static_cast<unsigned>(ResponseSource::InspectorOverride); ++source) {
        response.taintingSourceBits = static_cast<std::uint8_t>((source << responseSourceShift) | 0xC3);
        Require(static_cast<unsigned>(_ZNK7WebCore20ResourceResponseBase6sourceEv(&response)) == source);
    }
    Require(Throws([] { _ZN7WebCore20ResourceResponseBase7setTypeENS0_4TypeE(nullptr, ResponseType::Basic); }));
    Require(Throws([] { _ZNK7WebCore20ResourceResponseBase6sourceEv(nullptr); }));
}

static void TestCertificateInfo() {
    ResourceResponseBase response{};
    response.certificateInfo.value.verificationError = 9;
    response.certificateInfo.value.privateKeyPassword.impl = reinterpret_cast<StringImpl*>(&response);
    _ZNK7WebCore20ResourceResponseBase22includeCertificateInfoEv(&response);
    const auto& certificate = response.certificateInfo;
    Require(certificate.hasValue && certificate.value.verificationError == 0);
    Require(!certificate.value.certificateChain.buffer && !certificate.value.certificateChain.size);
    Require(!certificate.value.certificateChain.capacity && !certificate.value.privateKey.buffer);
    Require(!certificate.value.privateKey.size && !certificate.value.privateKeyPassword.impl);
    response.certificateInfo.value.verificationError = 7;
    _ZNK7WebCore20ResourceResponseBase22includeCertificateInfoEv(&response);
    Require(certificate.hasValue && certificate.value.verificationError == 7);
    Require(Throws([] { _ZNK7WebCore20ResourceResponseBase22includeCertificateInfoEv(nullptr); }));
}

static void TestLazyInit() {
    ResourceResponseBase response{};
    for (auto level : {Uninitialized, CommonFieldsOnly, AllFields})
        _ZNK7WebCore20ResourceResponseBase8lazyInitENS0_9InitLevelE(&response, level);
    Require(IsNull(response) && response.httpStatusCode == 0);
    Require(Throws([] { _ZNK7WebCore20ResourceResponseBase8lazyInitENS0_9InitLevelE(nullptr, AllFields); }));
}

static std::vector<HTTPHeaderName> CommonKeys(const ResourceResponseBase& response) {
    const auto& common = response.httpHeaderFields.common;
    std::vector<HTTPHeaderName> keys;
    for (unsigned index = 0; index < common.size; ++index) keys.push_back(common.buffer[index].key);
    return keys;
}

static void Populate(ResponseFixture& fixture) {
    fixture.Set(HTTPHeaderName::SetCookie, Text("a=b"));
    fixture.Set(HTTPHeaderName::SetCookie2, Text("c=d"));
    fixture.Set(HTTPHeaderName::ContentType, Text("text/html"));
    fixture.Set(HTTPHeaderName::Location, Text("http://b/"));
    fixture.Set(HTTPHeaderName::Connection, Text("close"));
    fixture.Set(HTTPHeaderName::CacheControl, Text("no-store"));
    fixture.Set(HTTPHeaderName::ContentLength, Text("5"));
    fixture.Set(HTTPHeaderName::Authorization, Text("secret"));
    fixture.Set(HTTPHeaderName::CrossOriginOpenerPolicy, Text("same-origin"));
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&fixture.response, Text("X-Custom"), Text("v"));
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&fixture.response, Text("X-Other"), Text("w"));
}

static void SetTainting(ResourceResponseBase& response, ResponseTainting tainting) {
    response.taintingSourceBits = static_cast<std::uint8_t>(
        (response.taintingSourceBits & ~(responseTaintingMask << responseTaintingShift))
        | (static_cast<unsigned>(tainting) << responseTaintingShift));
}

static void Sanitize(ResponseFixture& fixture, ResponseSanitizationType type) {
    _ZN7WebCore20ResourceResponseBase24sanitizeHTTPHeaderFieldsENS0_16SanitizationTypeE(&fixture.response, type);
}

static std::vector<std::u16string> UncommonKeys(const ResourceResponseBase& response) {
    const auto& uncommon = response.httpHeaderFields.uncommon;
    std::vector<std::u16string> keys;
    for (unsigned index = 0; index < uncommon.size; ++index) keys.push_back(Contents(uncommon.buffer[index].key));
    return keys;
}

static void TestSanitize() {
    using Keys = std::vector<HTTPHeaderName>;
    using Names = std::vector<std::u16string>;
    const Keys withoutCookies{HTTPHeaderName::ContentType, HTTPHeaderName::Location, HTTPHeaderName::Connection,
        HTTPHeaderName::CacheControl, HTTPHeaderName::ContentLength, HTTPHeaderName::Authorization, HTTPHeaderName::CrossOriginOpenerPolicy};
    {
        ResponseFixture fixture;
        Populate(fixture);
        Require(CommonKeys(fixture.response).size() == 9 && fixture.response.httpHeaderFields.uncommon.size == 2);
        Sanitize(fixture, ResponseSanitizationType::RemoveCookies);
        Require(CommonKeys(fixture.response) == withoutCookies);
        Require(UncommonKeys(fixture.response) == Names({u"X-Custom", u"X-Other"}));
        Require(Contents(fixture.Get(HTTPHeaderName::Location)) == u"http://b/");
    }
    {
        ResponseFixture fixture;
        Populate(fixture);
        _ZN7WebCore13HTTPHeaderMap6appendERKN3WTF6StringES4_(&fixture.response.httpHeaderFields, Text("Location"), Text("http://c/"));
        Require(CommonKeys(fixture.response).size() == 10);
        Sanitize(fixture, ResponseSanitizationType::Redirection);
        Require(CommonKeys(fixture.response) == Keys({HTTPHeaderName::Location, HTTPHeaderName::CacheControl,
            HTTPHeaderName::CrossOriginOpenerPolicy, HTTPHeaderName::Location}));
        Require(fixture.response.httpHeaderFields.uncommon.size == 0);
        Require(Contents(fixture.Get(HTTPHeaderName::Location)) == u"http://b/");
        Require(Contents(fixture.Get(HTTPHeaderName::CrossOriginOpenerPolicy)) == u"same-origin");
    }
    {
        ResponseFixture fixture;
        Populate(fixture);
        SetTainting(fixture.response, ResponseTainting::Basic);
        Sanitize(fixture, ResponseSanitizationType::CrossOriginSafe);
        Require(CommonKeys(fixture.response) == withoutCookies);
        Require(UncommonKeys(fixture.response) == Names({u"X-Custom", u"X-Other"}));
    }
    for (auto tainting : {ResponseTainting::Opaque, ResponseTainting::Opaqueredirect}) {
        ResponseFixture fixture;
        Populate(fixture);
        fixture.Set(HTTPHeaderName::AccessControlExposeHeaders, Text("Connection, X-Custom"));
        SetTainting(fixture.response, tainting);
        Sanitize(fixture, ResponseSanitizationType::CrossOriginSafe);
        Require(CommonKeys(fixture.response) == Keys({HTTPHeaderName::ContentType, HTTPHeaderName::Location,
            HTTPHeaderName::CacheControl, HTTPHeaderName::ContentLength, HTTPHeaderName::AccessControlExposeHeaders}));
        Require(fixture.response.httpHeaderFields.uncommon.size == 0);
        Require(Contents(fixture.Get(HTTPHeaderName::ContentType)) == u"text/html");
    }
    struct CorsCase {
        const char* expose;
        Keys common;
        Names uncommon;
    };
    const Keys safe{HTTPHeaderName::ContentType, HTTPHeaderName::Location, HTTPHeaderName::CacheControl, HTTPHeaderName::ContentLength};
    const auto with = [&](HTTPHeaderName extra) {
        auto keys = safe;
        keys.push_back(extra);
        return keys;
    };
    const Keys exposedSafe = with(HTTPHeaderName::AccessControlExposeHeaders);
    const std::vector<CorsCase> cases{
        {nullptr, safe, {}},
        {"x-custom ,, CONNECTION\t", {HTTPHeaderName::ContentType, HTTPHeaderName::Location, HTTPHeaderName::Connection,
            HTTPHeaderName::CacheControl, HTTPHeaderName::ContentLength, HTTPHeaderName::AccessControlExposeHeaders}, {u"X-Custom"}},
        {"x-custom, bad token, Connection", exposedSafe, {}},
        {"x-custom, na(me", exposedSafe, {}},
        {"*", withoutCookies, {u"X-Custom", u"X-Other"}},
        {"x-other, *", withoutCookies, {u"X-Custom", u"X-Other"}},
    };
    for (const auto& test : cases) {
        ResponseFixture fixture;
        Populate(fixture);
        if (test.expose) fixture.Set(HTTPHeaderName::AccessControlExposeHeaders, Text(test.expose));
        SetTainting(fixture.response, ResponseTainting::Cors);
        Sanitize(fixture, ResponseSanitizationType::CrossOriginSafe);
        auto expected = test.common;
        if (test.expose && expected == withoutCookies) expected.push_back(HTTPHeaderName::AccessControlExposeHeaders);
        Require(CommonKeys(fixture.response) == expected);
        Require(UncommonKeys(fixture.response) == test.uncommon);
    }
    {
        ResponseFixture fixture;
        Populate(fixture);
        Require(Throws([&] { Sanitize(fixture, static_cast<ResponseSanitizationType>(3)); }));
        Require(CommonKeys(fixture.response).size() == 9);
        Require(Throws([] { _ZN7WebCore20ResourceResponseBase24sanitizeHTTPHeaderFieldsENS0_16SanitizationTypeE(nullptr, ResponseSanitizationType::Redirection); }));
    }
}

int main() {
    TestDefaultConstruction();
    TestStatusCode();
    TestSetURL();
    TestExpectedContentLength();
    TestTypeAndSource();
    TestCertificateInfo();
    TestLazyInit();
    TestSanitize();
    TestFields();
    TestHTTPFamily();
    TestHeaderMutation();
    TestCacheControl();
    TestTimes();
    TestDisposition();
    TestHTTP09();
    TestFreshnessLifetime();
    TestRevalidationHeaders();
    TestNull();
    return 0;
}

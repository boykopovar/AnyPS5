#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

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

int main() {
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

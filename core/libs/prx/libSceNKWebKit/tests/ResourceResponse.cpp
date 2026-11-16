#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

using NKWebKit::WebCoreAbi::ResourceResponse;
using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WebCoreAbi::StringSlot;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceResponseBase check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::unordered_set<void*> guestAllocations;

static void* APS5_VABI Allocate_nid_no_patch(std::size_t size) {
    void* pointer = std::malloc(size);
    if (pointer) guestAllocations.insert(pointer);
    return pointer;
}

static void APS5_VABI Free_nid_no_patch(void* pointer) {
    Require(guestAllocations.erase(pointer) == 1);
    std::free(pointer);
}

static void* APS5_VABI Calloc_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
static void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
static void* APS5_VABI Align_nid_no_patch(std::size_t, std::size_t size) { return Allocate_nid_no_patch(size); }
static void* APS5_VABI Realign_nid_no_patch(void* pointer, std::size_t size, std::size_t) { return Reallocate_nid_no_patch(pointer, size); }
static int APS5_VABI PosixAlign_nid_no_patch(void** pointer, std::size_t alignment, std::size_t size) {
    *pointer = Align_nid_no_patch(alignment, size);
    return *pointer ? 0 : 12;
}

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

static String AtomText(std::string_view value) {
    auto text = Text(value);
    text.impl->hashAndFlags |= 16;
    return text;
}

static void Demote(StringImpl* impl) {
    if (impl) impl->hashAndFlags &= ~16u;
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

static std::u16string Contents(const StringSlot& slot) {
    Require(slot.impl != nullptr);
    if (slot.impl->hashAndFlags & 4) {
        const auto* bytes = static_cast<const unsigned char*>(slot.impl->data);
        return {bytes, bytes + slot.impl->length};
    }
    return {static_cast<const char16_t*>(slot.impl->data), slot.impl->length};
}

struct ResponseHolder {
    alignas(ResourceResponse) unsigned char storage[sizeof(ResourceResponse)]{};

    ResponseHolder() {
        _ZN7WebCore16ResourceResponseC1Ev(reinterpret_cast<ResourceResponse*>(storage));
    }

    ~ResponseHolder() {
        _ZN7WebCore16ResourceResponseD2Ev(reinterpret_cast<ResourceResponse*>(storage));
    }

    ResourceResponse* get() {
        return reinterpret_cast<ResourceResponse*>(storage);
    }
};

static NKWebKit::WtfAbi::URL MadeUrl(const String& text, unsigned pathEnd) {
    NKWebKit::WtfAbi::URL url{};
    url.impl = text.impl;
    url.bits = 1 | (4 << 6);
    url.pathEnd = pathEnd;
    return url;
}

static bool IsNullBit(const ResourceResponseBase& response) {
    return (response.stateBits & 1) != 0;
}

static void TestDefaultConstruction() {
    for (auto construct : {&_ZN7WebCore16ResourceResponseC1Ev, &_ZN7WebCore16ResourceResponseC2Ev}) {
        alignas(ResourceResponse) unsigned char storage[sizeof(ResourceResponse)];
        std::memset(storage, 0xAB, sizeof(storage));
        auto* response = reinterpret_cast<ResourceResponse*>(storage);
        construct(response);
        Require(!response->url.impl && response->url.bits == 0 && response->url.userStart == 0 && response->url.userEnd == 0
            && response->url.passwordEnd == 0 && response->url.hostEnd == 0 && response->url.pathAfterLastSlash == 0
            && response->url.pathEnd == 0 && response->url.queryEnd == 0);
        Require(!response->mimeType.impl && !response->textEncodingName.impl && !response->httpStatusText.impl && !response->httpVersion.impl);
        Require(response->expectedContentLength == 0);
        Require(!response->httpHeaderFields.common.buffer && response->httpHeaderFields.common.capacity == 0 && response->httpHeaderFields.common.size == 0);
        Require(!response->httpHeaderFields.uncommon.buffer && response->httpHeaderFields.uncommon.capacity == 0 && response->httpHeaderFields.uncommon.size == 0);
        Require(!response->networkLoadMetrics);
        Require(!response->certificateInfo.engaged);
        Require(response->httpStatusCode == 0);
        Require(response->stateBits == 1);
        Require(std::isnan(response->age) && std::isnan(response->date) && std::isnan(response->expires) && std::isnan(response->lastModified));
        Require(response->contentRange.firstBytePosition == 0 && response->contentRange.lastBytePosition == 0
            && response->contentRange.instanceLength == std::numeric_limits<long long>::min());
        Require(std::isnan(response->cacheControlDirectives.maxAge) && std::isnan(response->cacheControlDirectives.maxStale)
            && std::isnan(response->cacheControlDirectives.staleWhileRevalidate));
        Require(response->parsedBits == 0 && response->taintBits == 0 && response->typeBits == 2);
        _ZN7WebCore16ResourceResponseD2Ev(response);
    }
}

static void TestFieldsConstruction() {
    for (auto construct : {&_ZN7WebCore16ResourceResponseC1ERKN3WTF3URLERKNS1_6StringExS7_, &_ZN7WebCore16ResourceResponseC2ERKN3WTF3URLERKNS1_6StringExS7_}) {
        auto urlText = Text("http://a/");
        auto mime = AtomText("text/html");
        auto encoding = AtomText("utf-8");
        const auto urlBase = urlText.impl->refCount;
        const auto mimeBase = mime.impl->refCount;
        const auto encodingBase = encoding.impl->refCount;
        const auto url = MadeUrl(urlText, 10);
        alignas(ResourceResponse) unsigned char storage[sizeof(ResourceResponse)];
        auto* response = reinterpret_cast<ResourceResponse*>(storage);
        construct(response, url, mime, -42, encoding);
        Require(response->url.impl == urlText.impl && response->url.bits == url.bits && response->url.pathEnd == 10);
        Require(urlText.impl->refCount == urlBase + 2);
        Require(response->mimeType.impl == mime.impl && mime.impl->refCount == mimeBase + 2);
        Require(response->expectedContentLength == -42);
        Require(response->textEncodingName.impl == encoding.impl && encoding.impl->refCount == encodingBase + 2);
        Require(!IsNullBit(*response));
        Require(response->certificateInfo.engaged);
        Require(response->certificateInfo.value.verificationError == 0);
        Require(!response->certificateInfo.value.certificateChain.buffer && response->certificateInfo.value.certificateChain.size == 0);
        Require(!response->certificateInfo.value.privateKey.buffer && response->certificateInfo.value.privateKey.size == 0);
        Require(!response->certificateInfo.value.privateKeyPassword.impl);
        Require(std::isnan(response->age) && response->typeBits == 2 && response->taintBits == 0);
        Require(response->httpHeaderFields.common.size == 0 && response->httpHeaderFields.uncommon.size == 0);
        _ZN7WebCore16ResourceResponseD1Ev(response);
        Require(urlText.impl->refCount == urlBase && mime.impl->refCount == mimeBase && encoding.impl->refCount == encodingBase);
        Demote(mime.impl);
        Demote(encoding.impl);
    }

    auto plain = Text("text/html");
    const auto plainBase = plain.impl->refCount;
    const auto url = MadeUrl(plain, 10);
    alignas(ResourceResponse) unsigned char storage[sizeof(ResourceResponse)];
    std::memset(storage, 0xAB, sizeof(storage));
    auto* response = reinterpret_cast<ResourceResponse*>(storage);
    bool thrown = false;
    try {
        _ZN7WebCore16ResourceResponseC1ERKN3WTF3URLERKNS1_6StringExS7_(response, url, plain, -42, String());
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    for (const auto byte : storage) Require(byte == 0xAB);
    Require(plain.impl->refCount == plainBase);
    Demote(plain.impl);
}

static void TestBaseFieldsConstruction() {
    for (auto construct : {&_ZN7WebCore20ResourceResponseBaseC1ERKN3WTF3URLERKNS1_6StringExS7_, &_ZN7WebCore20ResourceResponseBaseC2ERKN3WTF3URLERKNS1_6StringExS7_}) {
        auto urlText = Text("http://a/");
        auto mime = AtomText("text/html");
        auto encoding = AtomText("utf-8");
        const auto urlBase = urlText.impl->refCount;
        const auto mimeBase = mime.impl->refCount;
        const auto encodingBase = encoding.impl->refCount;
        const auto url = MadeUrl(urlText, 10);
        alignas(ResourceResponseBase) unsigned char storage[sizeof(ResourceResponseBase)];
        auto* response = reinterpret_cast<ResourceResponseBase*>(storage);
        construct(response, url, mime, -42, encoding);
        Require(response->url.impl == urlText.impl && response->url.bits == url.bits && response->url.pathEnd == 10);
        Require(urlText.impl->refCount == urlBase + 2);
        Require(response->mimeType.impl == mime.impl && mime.impl->refCount == mimeBase + 2);
        Require(response->expectedContentLength == -42);
        Require(response->textEncodingName.impl == encoding.impl && encoding.impl->refCount == encodingBase + 2);
        Require(!IsNullBit(*response));
        Require(response->certificateInfo.engaged);
        Require(response->certificateInfo.value.verificationError == 0);
        Require(!response->certificateInfo.value.certificateChain.buffer && response->certificateInfo.value.certificateChain.size == 0);
        Require(!response->certificateInfo.value.privateKey.buffer && response->certificateInfo.value.privateKey.size == 0);
        Require(!response->certificateInfo.value.privateKeyPassword.impl);
        Require(std::isnan(response->age) && response->typeBits == 2 && response->taintBits == 0);
        Require(response->httpHeaderFields.common.size == 0 && response->httpHeaderFields.uncommon.size == 0);
        _ZN7WebCore16ResourceResponseD1Ev(reinterpret_cast<ResourceResponse*>(response));
        Require(urlText.impl->refCount == urlBase && mime.impl->refCount == mimeBase && encoding.impl->refCount == encodingBase);
        Demote(mime.impl);
        Demote(encoding.impl);
    }

    auto plain = Text("text/html");
    const auto plainBase = plain.impl->refCount;
    const auto url = MadeUrl(plain, 10);
    alignas(ResourceResponseBase) unsigned char storage[sizeof(ResourceResponseBase)];
    std::memset(storage, 0xAB, sizeof(storage));
    auto* response = reinterpret_cast<ResourceResponseBase*>(storage);
    bool thrown = false;
    try {
        _ZN7WebCore20ResourceResponseBaseC1ERKN3WTF3URLERKNS1_6StringExS7_(response, url, plain, -42, String());
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    for (const auto byte : storage) Require(byte == 0xAB);
    Require(plain.impl->refCount == plainBase);
    Demote(plain.impl);
}

static void TestCopyConstruction() {
    auto urlText = Text("http://a/path");
    auto mime = AtomText("text/html");
    auto encoding = AtomText("utf-8");
    const auto urlBase = urlText.impl->refCount;
    const auto mimeBase = mime.impl->refCount;
    const auto encodingBase = encoding.impl->refCount;
    const auto url = MadeUrl(urlText, 13);
    alignas(ResourceResponse) unsigned char originalStorage[sizeof(ResourceResponse)];
    auto* response = reinterpret_cast<ResourceResponse*>(originalStorage);
    _ZN7WebCore16ResourceResponseC1ERKN3WTF3URLERKNS1_6StringExS7_(response, url, mime, 77, encoding);
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(response, HTTPHeaderName::ContentType, Text("text/html"));
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldERKN3WTF6StringES4_(response, Text("X-Test"), Text("one"));
    response->stateBits = 0x3F;
    response->httpStatusCode = 404;
    response->age = 1.5;
    response->date = 2.5;
    response->expires = 3.5;
    response->lastModified = 4.5;
    response->contentRange = {1, 2, 3};
    response->cacheControlDirectives = {1, 2, 3};
    response->parsedBits = 0x2A;
    response->taintBits = 0x24;
    response->typeBits = 5;

    NKWebKit::WtfAbi::Vector<unsigned char> first;
    first.append(1);
    first.append(2);
    first.append(3);
    NKWebKit::WtfAbi::Vector<unsigned char> second;
    second.append(4);
    NKWebKit::WtfAbi::Vector<NKWebKit::WtfAbi::Vector<unsigned char>> chain;
    chain.append(first);
    chain.append(second);
    NKWebKit::WtfAbi::Vector<unsigned char> key;
    key.append(9);
    key.append(9);
    auto password = Text("secret");
    const auto passwordBase = password.impl->refCount;
    response->certificateInfo.value.certificateChain = chain;
    response->certificateInfo.value.privateKey = key;
    response->certificateInfo.value.privateKeyPassword = password;
    response->certificateInfo.engaged = true;

    void* box = _ZN3WTF10fastMallocEm(8 + 24);
    std::memset(box, 0, 8 + 24);
    *static_cast<unsigned*>(box) = 2;
    response->networkLoadMetrics = box;

    alignas(ResourceResponse) unsigned char storage[sizeof(ResourceResponse)];
    std::memset(storage, 0xAB, sizeof(storage));
    auto* copied = reinterpret_cast<ResourceResponse*>(storage);
    {
        alignas(ResourceResponse) unsigned char brokenStorage[sizeof(ResourceResponse)];
        auto* broken = reinterpret_cast<ResourceResponse*>(brokenStorage);
        _ZN7WebCore16ResourceResponseC1Ev(broken);
        broken->httpHeaderFields.common.size = 1;
        bool thrown = false;
        try {
            _ZN7WebCore16ResourceResponseC2ERKS0_(copied, broken);
        } catch (const std::invalid_argument&) {
            thrown = true;
        }
        Require(thrown);
        for (const auto byte : storage) Require(byte == 0xAB);
        _ZN7WebCore16ResourceResponseD1Ev(broken);
    }
    _ZN7WebCore16ResourceResponseC2ERKS0_(copied, response);
    Require(copied->url.impl == response->url.impl && copied->url.bits == url.bits);
    Require(urlText.impl->refCount == urlBase + 4);
    Require(copied->mimeType.impl == mime.impl && mime.impl->refCount == mimeBase + 4);
    Require(copied->expectedContentLength == 77);
    Require(copied->httpHeaderFields.common.buffer != response->httpHeaderFields.common.buffer);
    Require(copied->httpHeaderFields.common.size == 1 && copied->httpHeaderFields.common.capacity == 1);
    Require(Contents(copied->httpHeaderFields.common.buffer[0].value) == u"text/html");
    Require(copied->httpHeaderFields.uncommon.buffer != response->httpHeaderFields.uncommon.buffer);
    Require(copied->httpHeaderFields.uncommon.size == 1 && copied->httpHeaderFields.uncommon.capacity == 1);
    Require(Contents(copied->httpHeaderFields.uncommon.buffer[0].key) == u"X-Test");
    Require(copied->httpHeaderFields.uncommon.buffer[0].key.impl == response->httpHeaderFields.uncommon.buffer[0].key.impl);
    Require(copied->certificateInfo.engaged);
    Require(copied->certificateInfo.value.certificateChain.buffer != response->certificateInfo.value.certificateChain.buffer);
    Require(copied->certificateInfo.value.certificateChain.size == 2);
    Require(copied->certificateInfo.value.certificateChain.buffer[0].buffer != response->certificateInfo.value.certificateChain.buffer[0].buffer);
    Require(copied->certificateInfo.value.certificateChain.buffer[0].size == 3);
    Require(copied->certificateInfo.value.certificateChain.buffer[0].buffer[0] == 1
        && copied->certificateInfo.value.certificateChain.buffer[0].buffer[1] == 2
        && copied->certificateInfo.value.certificateChain.buffer[0].buffer[2] == 3);
    Require(copied->certificateInfo.value.certificateChain.buffer[1].size == 1 && copied->certificateInfo.value.certificateChain.buffer[1].buffer[0] == 4);
    Require(copied->certificateInfo.value.privateKey.size == 2 && copied->certificateInfo.value.privateKey.buffer[0] == 9);
    Require(copied->certificateInfo.value.privateKeyPassword.impl == password.impl && password.impl->refCount == passwordBase + 4);
    Require(copied->networkLoadMetrics == box && *static_cast<unsigned*>(box) == 3);
    Require(copied->stateBits == 0x3F);
    Require(copied->httpStatusCode == 404);
    Require(copied->age == 1.5 && copied->date == 2.5 && copied->expires == 3.5 && copied->lastModified == 4.5);
    Require(std::memcmp(&copied->contentRange, &response->contentRange, sizeof(copied->contentRange)) == 0);
    Require(std::memcmp(&copied->cacheControlDirectives, &response->cacheControlDirectives, sizeof(copied->cacheControlDirectives)) == 0);
    Require(copied->parsedBits == 0x2A && copied->taintBits == 0x24 && copied->typeBits == 5);

    alignas(ResourceResponse) unsigned char copied2Storage[sizeof(ResourceResponse)];
    auto* copied2 = reinterpret_cast<ResourceResponse*>(copied2Storage);
    _ZN7WebCore16ResourceResponseC1ERKS0_(copied2, response);
    Require(copied2->url.impl == response->url.impl && copied2->expectedContentLength == 77);
    Require(copied2->httpHeaderFields.common.size == 1 && Contents(copied2->httpHeaderFields.common.buffer[0].value) == u"text/html");
    Require(copied2->networkLoadMetrics == box && *static_cast<unsigned*>(box) == 4);
    Require(copied2->certificateInfo.engaged && copied2->certificateInfo.value.privateKeyPassword.impl == password.impl);
    _ZN7WebCore16ResourceResponseD1Ev(copied2);
    Require(*static_cast<unsigned*>(box) == 3);

    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(response, HTTPHeaderName::ContentType, Text("text/plain"));
    Require(Contents(copied->httpHeaderFields.common.buffer[0].value) == u"text/html");

    _ZN7WebCore16ResourceResponseD1Ev(copied);
    Require(*static_cast<unsigned*>(box) == 2);
    Require(password.impl->refCount == passwordBase + 2);
    Require(urlText.impl->refCount == urlBase + 2 && mime.impl->refCount == mimeBase + 2 && encoding.impl->refCount == encodingBase + 2);
    _ZN7WebCore16ResourceResponseD2Ev(response);
    Require(*static_cast<unsigned*>(box) == 1);
    Require(urlText.impl->refCount == urlBase && mime.impl->refCount == mimeBase && encoding.impl->refCount == encodingBase && password.impl->refCount == passwordBase);
    _ZN3WTF8fastFreeEPv(box);
    Demote(mime.impl);
    Demote(encoding.impl);
}

static void TestCopyAssignment() {
    auto urlText = Text("http://a/");
    auto mime = AtomText("text/html");
    const auto urlBase = urlText.impl->refCount;
    const auto mimeBase = mime.impl->refCount;
    const auto url = MadeUrl(urlText, 10);
    alignas(ResourceResponse) unsigned char targetStorage[sizeof(ResourceResponse)];
    auto* target = reinterpret_cast<ResourceResponse*>(targetStorage);
    _ZN7WebCore16ResourceResponseC1ERKN3WTF3URLERKNS1_6StringExS7_(target, url, mime, 5, mime);
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(target, HTTPHeaderName::ContentType, Text("text/html"));
    _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(target, HTTPHeaderName::ContentType, Text("text/plain"));
    Require(target->httpHeaderFields.common.size == 1 && target->httpHeaderFields.common.capacity == 6);

    alignas(ResourceResponse) unsigned char sourceStorage[sizeof(ResourceResponse)];
    auto* source = reinterpret_cast<ResourceResponse*>(sourceStorage);
    _ZN7WebCore16ResourceResponseC2Ev(source);
    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(source, HTTPHeaderName::ContentType, Text("text/xml"));

    auto& assign = _ZN7WebCore16ResourceResponseaSERKS0_;
    Require(&assign(target, source) == target);
    Require(target->httpHeaderFields.common.size == 1);
    Require(target->httpHeaderFields.common.capacity == 6);
    Require(Contents(target->httpHeaderFields.common.buffer[0].value) == u"text/xml");
    Require(IsNullBit(*target));

    _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(source, HTTPHeaderName::ContentType, Text("a"));
    _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(source, HTTPHeaderName::ContentType, Text("b"));
    _ZN7WebCore20ResourceResponseBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(source, HTTPHeaderName::ContentType, Text("c"));
    Require(source->httpHeaderFields.common.size == 1 && source->httpHeaderFields.common.capacity == 6);
    assign(target, source);
    Require(target->httpHeaderFields.common.size == 1 && target->httpHeaderFields.common.capacity == 6);
    Require(Contents(target->httpHeaderFields.common.buffer[0].value) == u"a, b, c");

    assign(target, target);
    Require(target->httpHeaderFields.common.size == 1);
    Require(Contents(target->httpHeaderFields.common.buffer[0].value) == u"a, b, c");

    auto big = AtomText("big");
    const auto bigBase = big.impl->refCount;
    _ZN7WebCore20ResourceResponseBase19setTextEncodingNameERKN3WTF6StringE(source, big);
    assign(target, source);
    Require(target->textEncodingName.impl == big.impl && big.impl->refCount == bigBase + 4);
    Require(source->textEncodingName.impl == big.impl);
    _ZN7WebCore20ResourceResponseBase19setTextEncodingNameERKN3WTF6StringE(source, String());
    Require(!source->textEncodingName.impl);
    assign(target, source);
    Require(!target->textEncodingName.impl);
    Require(big.impl->refCount == bigBase);

    source->stateBits = 0x2A;
    source->httpStatusCode = 201;
    source->typeBits = 4;
    assign(target, source);
    Require(target->stateBits == 0x2A && target->httpStatusCode == 201 && target->typeBits == 4);

    NKWebKit::WtfAbi::Vector<unsigned char> key;
    key.append(7);
    auto password = Text("pass");
    const auto passwordBase = password.impl->refCount;
    source->certificateInfo.value.verificationError = 5;
    source->certificateInfo.value.privateKey = key;
    source->certificateInfo.value.privateKeyPassword = password;
    source->certificateInfo.engaged = true;
    assign(target, source);
    Require(target->certificateInfo.engaged);
    Require(target->certificateInfo.value.verificationError == 5);
    Require(target->certificateInfo.value.privateKey.size == 1 && target->certificateInfo.value.privateKey.buffer[0] == 7);
    Require(target->certificateInfo.value.privateKey.buffer != source->certificateInfo.value.privateKey.buffer);
    Require(target->certificateInfo.value.privateKeyPassword.impl == password.impl && password.impl->refCount == passwordBase + 4);
    assign(target, target);
    Require(password.impl->refCount == passwordBase + 4);
    source->certificateInfo.value.~CertificateInfo();
    source->certificateInfo.engaged = false;
    assign(target, source);
    Require(!target->certificateInfo.engaged);
    Require(password.impl->refCount == passwordBase);

    new (&source->certificateInfo.value) NKWebKit::WebCoreAbi::CertificateInfo{};
    source->certificateInfo.engaged = true;
    source->certificateInfo.value.verificationError = 7;
    source->certificateInfo.value.privateKey = key;
    source->certificateInfo.value.privateKeyPassword = password;
    assign(target, source);
    Require(target->certificateInfo.engaged && target->certificateInfo.value.verificationError == 7);
    Require(target->certificateInfo.value.privateKey.size == 1 && target->certificateInfo.value.privateKey.buffer[0] == 7);
    Require(target->certificateInfo.value.privateKey.buffer != source->certificateInfo.value.privateKey.buffer);
    Require(target->certificateInfo.value.privateKeyPassword.impl == password.impl && password.impl->refCount == passwordBase + 4);

    auto plainMime = Text("text/plain");
    const auto plainMimeBase = plainMime.impl->refCount;
    source->mimeType.impl = plainMime.impl;
    bool slotThrown = false;
    try {
        assign(target, source);
    } catch (const std::runtime_error&) {
        slotThrown = true;
    }
    Require(slotThrown);
    Require(plainMime.impl->refCount == plainMimeBase);
    Require(Contents(target->httpHeaderFields.common.buffer[0].value) == u"a, b, c");
    source->mimeType.impl = nullptr;

    void* heldBox = _ZN3WTF10fastMallocEm(8 + 24);
    std::memset(heldBox, 0, 8 + 24);
    *static_cast<unsigned*>(heldBox) = 1;
    *reinterpret_cast<void**>(static_cast<unsigned char*>(heldBox) + 8) = heldBox;
    target->networkLoadMetrics = heldBox;
    bool boxThrown = false;
    try {
        assign(target, source);
    } catch (const std::runtime_error&) {
        boxThrown = true;
    }
    Require(boxThrown);
    Require(target->networkLoadMetrics == heldBox && *static_cast<unsigned*>(heldBox) == 1);
    Require(!source->networkLoadMetrics);
    *reinterpret_cast<void**>(static_cast<unsigned char*>(heldBox) + 8) = nullptr;
    assign(target, source);
    Require(!target->networkLoadMetrics);

    void* box = _ZN3WTF10fastMallocEm(8 + 24);
    std::memset(box, 0, 8 + 24);
    *static_cast<unsigned*>(box) = 2;
    source->networkLoadMetrics = box;
    assign(target, source);
    Require(*static_cast<unsigned*>(box) == 3);
    Require(target->networkLoadMetrics == box);
    assign(target, target);
    Require(*static_cast<unsigned*>(box) == 3);
    _ZN7WebCore16ResourceResponseD2Ev(target);
    Require(*static_cast<unsigned*>(box) == 2);
    _ZN7WebCore16ResourceResponseD2Ev(source);
    Require(*static_cast<unsigned*>(box) == 1);
    _ZN3WTF8fastFreeEPv(box);
    Require(urlText.impl->refCount == urlBase && mime.impl->refCount == mimeBase && password.impl->refCount == passwordBase);
    Demote(mime.impl);
    Demote(big.impl);
}

static void TestSetters() {
    ResponseHolder holder;
    auto* response = holder.get();
    Require(IsNullBit(*response));
    auto type = AtomText("text/html");
    const auto typeBase = type.impl->refCount;
    _ZN7WebCore20ResourceResponseBase11setMimeTypeERKN3WTF6StringE(response, type);
    Require(!IsNullBit(*response));
    Require(Contents(response->mimeType) == u"text/html");
    Require(type.impl->refCount == typeBase + 2);

    auto replacement = AtomText("text/xml");
    const auto replacementBase = replacement.impl->refCount;
    _ZN7WebCore20ResourceResponseBase11setMimeTypeERKN3WTF6StringE(response, replacement);
    Require(Contents(response->mimeType) == u"text/xml");
    Require(type.impl->refCount == typeBase);
    Require(replacement.impl->refCount == replacementBase + 2);

    auto plain = Text("text/html");
    const auto plainBase = plain.impl->refCount;
    bool thrown = false;
    try {
        _ZN7WebCore20ResourceResponseBase11setMimeTypeERKN3WTF6StringE(response, plain);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    Require(Contents(response->mimeType) == u"text/xml");
    Require(!IsNullBit(*response));
    Require(plain.impl->refCount == plainBase);

    _ZN7WebCore20ResourceResponseBase11setMimeTypeERKN3WTF6StringE(response, String());
    Require(!response->mimeType.impl);
    Require(!IsNullBit(*response));
    Require(replacement.impl->refCount == replacementBase);

    auto encoding = AtomText("utf-8");
    const auto encodingBase = encoding.impl->refCount;
    _ZN7WebCore20ResourceResponseBase19setTextEncodingNameERKN3WTF6StringE(response, encoding);
    Require(!IsNullBit(*response));
    Require(Contents(response->textEncodingName) == u"utf-8");
    Require(encoding.impl->refCount == encodingBase + 2);

    auto plainEncoding = Text("latin-1");
    const auto plainEncodingBase = plainEncoding.impl->refCount;
    thrown = false;
    try {
        _ZN7WebCore20ResourceResponseBase19setTextEncodingNameERKN3WTF6StringE(response, plainEncoding);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    Require(Contents(response->textEncodingName) == u"utf-8");
    Require(!IsNullBit(*response));
    Require(plainEncoding.impl->refCount == plainEncodingBase);

    ResponseHolder statusHolder;
    auto* statusResponse = statusHolder.get();
    auto status = AtomText("OK");
    const auto statusBase = status.impl->refCount;
    _ZN7WebCore20ResourceResponseBase17setHTTPStatusTextERKN3WTF6StringE(statusResponse, status);
    Require(IsNullBit(*statusResponse));
    Require(Contents(statusResponse->httpStatusText) == u"OK");
    Require(status.impl->refCount == statusBase + 2);

    auto plainStatus = Text("Bad");
    const auto plainStatusBase = plainStatus.impl->refCount;
    thrown = false;
    try {
        _ZN7WebCore20ResourceResponseBase17setHTTPStatusTextERKN3WTF6StringE(statusResponse, plainStatus);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    Require(Contents(statusResponse->httpStatusText) == u"OK");
    Require(plainStatus.impl->refCount == plainStatusBase);

    auto version = AtomText("HTTP/1.1");
    const auto versionBase = version.impl->refCount;
    _ZN7WebCore20ResourceResponseBase14setHTTPVersionERKN3WTF6StringE(statusResponse, version);
    Require(IsNullBit(*statusResponse));
    Require(Contents(statusResponse->httpVersion) == u"HTTP/1.1");
    Require(version.impl->refCount == versionBase + 2);

    auto plainVersion = Text("HTTP/1.0");
    const auto plainVersionBase = plainVersion.impl->refCount;
    thrown = false;
    try {
        _ZN7WebCore20ResourceResponseBase14setHTTPVersionERKN3WTF6StringE(statusResponse, plainVersion);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    Require(Contents(statusResponse->httpVersion) == u"HTTP/1.1");
    Require(plainVersion.impl->refCount == plainVersionBase);

    bool thrownInvalid = false;
    try {
        _ZN7WebCore20ResourceResponseBase11setMimeTypeERKN3WTF6StringE(nullptr, type);
    } catch (const std::invalid_argument&) {
        thrownInvalid = true;
    }
    Require(thrownInvalid);
    thrownInvalid = false;
    try {
        _ZN7WebCore20ResourceResponseBase19setTextEncodingNameERKN3WTF6StringE(nullptr, type);
    } catch (const std::invalid_argument&) {
        thrownInvalid = true;
    }
    Require(thrownInvalid);
    thrownInvalid = false;
    try {
        _ZN7WebCore20ResourceResponseBase17setHTTPStatusTextERKN3WTF6StringE(nullptr, type);
    } catch (const std::invalid_argument&) {
        thrownInvalid = true;
    }
    Require(thrownInvalid);
    thrownInvalid = false;
    try {
        _ZN7WebCore20ResourceResponseBase14setHTTPVersionERKN3WTF6StringE(nullptr, type);
    } catch (const std::invalid_argument&) {
        thrownInvalid = true;
    }
    Require(thrownInvalid);

    Demote(type.impl);
    Demote(replacement.impl);
    Demote(encoding.impl);
    Demote(status.impl);
    Demote(version.impl);
}

static void TestPlatformSuggestedFilename() {
    ResponseHolder holder;
    auto* response = holder.get();
    Require(!_ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(response).impl);

    auto setDisposition = [&response](const String& value) {
        _ZN7WebCore20ResourceResponseBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(response, HTTPHeaderName::ContentDisposition, value);
    };

    setDisposition(Text("attachment; filename=report.pdf"));
    Require(Contents(_ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(response)) == u"report.pdf");
    setDisposition(Text("inline; filename=plain.txt"));
    Require(Contents(_ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(response)) == u"plain.txt");
    setDisposition(Text("attachment; filename=\vx.txt\v"));
    Require(Contents(_ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(response)) == u"\vx.txt\v");
    setDisposition(Text("attachment; Filename=ignored"));
    Require(!_ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(response).impl);
    setDisposition(Text("attachment; filename=\"a;b\".txt"));
    Require(Contents(_ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(response)).empty());
    setDisposition(Text(u"attachment; filename=\"r\u00e9sum\u00e9.txt\""));
    Require(Contents(_ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(response)) == u"r\u00e9sum\u00e9.txt");
    setDisposition(Text("attachment; filename=\"\""));
    Require(Contents(_ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(response)).empty());
    setDisposition(Text("attachment"));
    Require(!_ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(response).impl);
}

static void TestLifecycleNulls() {
    bool thrown = false;
    try {
        _ZN7WebCore16ResourceResponseC2Ev(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
    thrown = false;
    try {
        _ZN7WebCore16ResourceResponseC2ERKS0_(nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
    thrown = false;
    try {
        _ZN7WebCore16ResourceResponseD2Ev(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
    thrown = false;
    const NKWebKit::WtfAbi::URL emptyUrl{};
    try {
        _ZN7WebCore20ResourceResponseBaseC2ERKN3WTF3URLERKNS1_6StringExS7_(nullptr, emptyUrl, String(), 0, String());
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);

    alignas(ResourceResponse) unsigned char storage[sizeof(ResourceResponse)];
    auto* response = reinterpret_cast<ResourceResponse*>(storage);
    _ZN7WebCore16ResourceResponseC1Ev(response);
    thrown = false;
    try {
        _ZN7WebCore16ResourceResponseC1ERKS0_(response, nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
    _ZN7WebCore16ResourceResponseD1Ev(response);

    thrown = false;
    try {
        _ZNK7WebCore16ResourceResponse25platformSuggestedFilenameEv(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
    thrown = false;
    try {
        _ZNK7WebCore16ResourceResponse13cfURLResponseEv();
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
}

static void TestMetricsBoxRelease() {
    void* box = _ZN3WTF10fastMallocEm(8 + 24);
    std::memset(box, 0, 8 + 24);
    *static_cast<unsigned*>(box) = 1;
    {
        ResponseHolder holder;
        holder.get()->networkLoadMetrics = box;
    }

    void* held = _ZN3WTF10fastMallocEm(8 + 24);
    std::memset(held, 0, 8 + 24);
    *static_cast<unsigned*>(held) = 1;
    *reinterpret_cast<void**>(static_cast<unsigned char*>(held) + 8) = held;
    alignas(ResourceResponse) unsigned char storage[sizeof(ResourceResponse)];
    auto* response = reinterpret_cast<ResourceResponse*>(storage);
    _ZN7WebCore16ResourceResponseC1Ev(response);
    response->networkLoadMetrics = held;
    bool thrown = false;
    try {
        _ZN7WebCore16ResourceResponseD2Ev(response);
    } catch (const std::runtime_error&) {
        thrown = true;
    }
    Require(thrown);
    Require(*static_cast<unsigned*>(held) == 1);
    *reinterpret_cast<void**>(static_cast<unsigned char*>(held) + 8) = nullptr;
    _ZN7WebCore16ResourceResponseD2Ev(response);
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestFields();
    TestHTTPFamily();
    TestHeaderMutation();
    TestCacheControl();
    TestTimes();
    TestDisposition();
    TestHTTP09();
    TestNull();
    TestDefaultConstruction();
    TestFieldsConstruction();
    TestBaseFieldsConstruction();
    TestCopyConstruction();
    TestCopyAssignment();
    TestSetters();
    TestPlatformSuggestedFilename();
    TestLifecycleNulls();
    TestMetricsBoxRelease();
    Require(guestAllocations.empty());
    return 0;
}

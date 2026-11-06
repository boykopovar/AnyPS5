#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using WKPoint = NKWebKit::PointValue;
using WKSize = NKWebKit::SizeValue;
using WKRect = NKWebKit::RectValue;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "libSceNKWebKit Base check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void TestTypeIDs() {
    Require(WKArrayGetTypeID() == 1);
    Require(WKDataGetTypeID() == 8);
    Require(WKDictionaryGetTypeID() == 9);
    Require(WKErrorGetTypeID() == 10);
    Require(WKSecurityOriginGetTypeID() == 18);
    Require(WKStringGetTypeID() == 21);
    Require(WKURLGetTypeID() == 22);
    Require(WKURLRequestGetTypeID() == 23);
    Require(WKURLResponseGetTypeID() == 24);
    Require(WKUserContentURLPatternGetTypeID() == 25);
    Require(WKBooleanGetTypeID() == 30);
    Require(WKDoubleGetTypeID() == 31);
    Require(WKUInt64GetTypeID() == 32);
    Require(WKPointGetTypeID() == 34);
    Require(WKSizeGetTypeID() == 35);
    Require(WKRectGetTypeID() == 36);
    Require(WKCertificateInfoGetTypeID() == 0);
    Require(WKApplicationCacheManagerGetTypeID() == 0);
    Require(WKBundleBackForwardListGetTypeID() == 0);
    Require(WKContextMenuItemGetTypeID() == 0);
    Require(WKUserContentExtensionStoreGetTypeID() == 0);
    Require(WKAXObjectGetTypeID() == 132);
    WKRenderLayerGetTypeID();
    WKRenderObjectGetTypeID();
}

static void TestRetainRelease() {
    auto* s = WKStringCreateWithUTF8CString("hello");
    Require(WKGetTypeID(s) == WKStringGetTypeID());
    auto* retained = WKRetain(s);
    Require(retained == s);
    WKRelease(retained);
    WKRelease(s);

    bool caught = false;
    try {
        WKRetain(nullptr);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);
}

static void TestStrings() {
    auto* s = WKStringCreateWithUTF8CString("Hello, World!");
    Require(!WKStringIsEmpty(s));
    Require(WKStringGetLength(s) == 13);
    Require(WKStringGetMaximumUTF8CStringSize(s) == 13 * 3 + 1);

    unsigned short chars[16];
    std::size_t copied = WKStringGetCharacters(s, chars, 5);
    Require(copied == 5);
    Require(chars[0] == 'H' && chars[1] == 'e' && chars[2] == 'l' && chars[3] == 'l' && chars[4] == 'o');

    char buf[32];
    std::size_t written = WKStringGetUTF8CString(s, buf, sizeof(buf));
    Require(written == 14);
    Require(std::strcmp(buf, "Hello, World!") == 0);

    char smallBuf[6];
    std::size_t truncated = WKStringGetUTF8CString(s, smallBuf, sizeof(smallBuf));
    Require(truncated == 6);
    Require(std::strcmp(smallBuf, "Hello") == 0);

    Require(WKStringGetUTF8CString(s, buf, 0) == 0);

    auto* s2 = WKStringCreateWithUTF8CString("hello, world!");
    Require(!WKStringIsEqual(s, s2));
    Require(WKStringIsEqualToUTF8CString(s, "Hello, World!"));
    Require(!WKStringIsEqualToUTF8CString(s, "hello, world!"));
    Require(WKStringIsEqualToUTF8CStringIgnoringCase(s, "hello, world!"));

    auto* emptyStr = WKStringCreateWithUTF8CString("");
    Require(WKStringIsEmpty(emptyStr));
    Require(WKStringGetLength(emptyStr) == 0);
    Require(WKStringGetMaximumUTF8CStringSize(emptyStr) == 1);
    buf[0] = 'X';
    Require(WKStringGetUTF8CString(emptyStr, buf, sizeof(buf)) == 1);
    Require(buf[0] == '\0');

    auto* nullStr = WKStringCreateWithUTF8CString(nullptr);
    Require(WKStringIsEmpty(nullStr));
    Require(WKStringGetLength(nullStr) == 0);

    auto* jsStr = WKStringCopyJSString(s);
    Require(jsStr != nullptr);
    Require(JSStringGetLength_nid_postfix(jsStr) == 13);
    auto* roundTrip = WKStringCreateWithJSString(jsStr);
    Require(WKStringIsEqual(s, roundTrip));
    JSStringRelease_nid_postfix(jsStr);
    WKRelease(roundTrip);

    auto* emptyJs = WKStringCopyJSString(nullStr);
    Require(emptyJs != nullptr);
    Require(JSStringGetLength_nid_postfix(emptyJs) == 0);
    JSStringRelease_nid_postfix(emptyJs);
    auto* invalidUtf8 = WKStringCreateWithUTF8CString("\xC3\x28");
    Require(WKStringIsEmpty(invalidUtf8));
    auto* invalidJs = WKStringCopyJSString(invalidUtf8);
    Require(invalidJs != nullptr);
    JSStringRelease_nid_postfix(invalidJs);
    WKRelease(invalidUtf8);
    auto* fromNullJs = WKStringCreateWithJSString(nullptr);
    Require(WKStringIsEmpty(fromNullJs));
    Require(WKStringCopyJSString(fromNullJs) == nullptr);
    WKRelease(fromNullJs);
    auto* nullJs = JSStringCreateWithUTF8CString_nid_postfix(nullptr);
    auto* fromNullJsString = WKStringCreateWithJSString(nullJs);
    auto* fromNullJsCopy = WKStringCopyJSString(fromNullJsString);
    Require(fromNullJsCopy != nullptr);
    JSStringRelease_nid_postfix(fromNullJsCopy);
    WKRelease(fromNullJsString);
    JSStringRelease_nid_postfix(nullJs);

    WKRelease(s);
    WKRelease(s2);
    WKRelease(emptyStr);
    WKRelease(nullStr);
}

static void TestURL() {
    auto* u = WKURLCreateWithUTF8CString("https://example.com/test?a=1#frag");
    Require(WKGetTypeID(u) == WKURLGetTypeID());
    auto* str = WKURLCopyString(u);
    Require(WKStringIsEqualToUTF8CString(str, "https://example.com/test?a=1#frag"));
    WKRelease(str);
    WKRelease(u);
}

static void TestArray() {
    auto* s1 = WKStringCreateWithUTF8CString("one");
    auto* s2 = WKStringCreateWithUTF8CString("two");
    const void* items[] = { s1, s2 };
    auto* arr = WKArrayCreate(items, 2);
    Require(WKGetTypeID(arr) == WKArrayGetTypeID());
    Require(WKArrayGetSize(arr) == 2);
    Require(WKStringIsEqual(WKArrayGetItemAtIndex(arr, 0), s1));
    Require(WKStringIsEqual(WKArrayGetItemAtIndex(arr, 1), s2));

    bool caught = false;
    try {
        WKArrayGetItemAtIndex(arr, 2);
    } catch (const std::out_of_range&) {
        caught = true;
    }
    Require(caught);

    auto* mutArr = WKMutableArrayCreate();
    Require(WKArrayGetSize(mutArr) == 0);
    WKArrayAppendItem(mutArr, s1);
    WKArrayAppendItem(mutArr, s2);
    Require(WKArrayGetSize(mutArr) == 2);
    WKArrayRemoveItemAtIndex(mutArr, 0);
    Require(WKArrayGetSize(mutArr) == 1);
    Require(WKStringIsEqual(WKArrayGetItemAtIndex(mutArr, 0), s2));

    auto* adoptS = WKStringCreateWithUTF8CString("adopted");
    const void* adoptItems[] = { adoptS };
    auto* adoptArr = WKArrayCreateAdoptingValues(adoptItems, 1);
    Require(WKArrayGetSize(adoptArr) == 1);

    WKRelease(adoptArr);
    WKRelease(mutArr);
    WKRelease(arr);
    WKRelease(s1);
    WKRelease(s2);
}

static void TestDictionary() {
    auto* k1 = WKStringCreateWithUTF8CString("key1");
    auto* k2 = WKStringCreateWithUTF8CString("key2");
    auto* v1 = WKStringCreateWithUTF8CString("val1");
    auto* v2 = WKStringCreateWithUTF8CString("val2");
    const void* keys[] = { k1, k2 };
    const void* vals[] = { v1, v2 };
    auto* dict = WKDictionaryCreate(keys, vals, 2);
    Require(WKGetTypeID(dict) == WKDictionaryGetTypeID());
    Require(WKDictionaryGetSize(dict) == 2);
    Require(WKStringIsEqual(WKDictionaryGetItemForKey(dict, k1), v1));
    Require(WKStringIsEqual(WKDictionaryGetItemForKey(dict, k2), v2));

    auto* nonExistent = WKStringCreateWithUTF8CString("other");
    Require(WKDictionaryGetItemForKey(dict, nonExistent) == nullptr);

    auto* allKeys = WKDictionaryCopyKeys(dict);
    Require(WKArrayGetSize(allKeys) == 2);
    WKRelease(allKeys);

    auto* mutDict = WKMutableDictionaryCreate();
    Require(WKDictionarySetItem(mutDict, k1, v1) == true);
    Require(WKDictionarySetItem(mutDict, k1, v2) == false);
    Require(WKDictionaryGetSize(mutDict) == 1);
    Require(WKStringIsEqual(WKDictionaryGetItemForKey(mutDict, k1), v2));

    WKRelease(mutDict);
    WKRelease(dict);
    WKRelease(nonExistent);
    WKRelease(k1);
    WKRelease(k2);
    WKRelease(v1);
    WKRelease(v2);
}

static void TestData() {
    const unsigned char raw[] = { 0xDE, 0xAD, 0xBE, 0xEF };
    auto* d = WKDataCreate(raw, sizeof(raw));
    Require(WKGetTypeID(d) == WKDataGetTypeID());
    Require(WKDataGetSize(d) == 4);
    Require(std::memcmp(WKDataGetBytes(d), raw, 4) == 0);
    WKRelease(d);
}

static void TestNumbers() {
    auto* b = WKBooleanCreate(true);
    Require(WKGetTypeID(b) == WKBooleanGetTypeID());
    Require(WKBooleanGetValue(b) == true);
    WKRelease(b);

    auto* dbl = WKDoubleCreate(3.14159);
    Require(WKGetTypeID(dbl) == WKDoubleGetTypeID());
    Require(WKDoubleGetValue(dbl) == 3.14159);
    WKRelease(dbl);

    auto* u = WKUInt64Create(123456789012345ULL);
    Require(WKGetTypeID(u) == WKUInt64GetTypeID());
    Require(WKUInt64GetValue(u) == 123456789012345ULL);
    WKRelease(u);
}

static void TestGeometry() {
    WKPoint pt{ 10.5, 20.25 };
    auto* pObj = WKPointCreate(pt);
    Require(WKGetTypeID(pObj) == WKPointGetTypeID());
    WKPoint pVal = WKPointGetValue(pObj);
    Require(pVal.x == 10.5 && pVal.y == 20.25);
    WKRelease(pObj);

    WKSize sz{ 100.0, 200.0 };
    auto* sObj = WKSizeCreate(sz);
    Require(WKGetTypeID(sObj) == WKSizeGetTypeID());
    WKSize sVal = WKSizeGetValue(sObj);
    Require(sVal.width == 100.0 && sVal.height == 200.0);
    WKRelease(sObj);

    WKRect rc{ { 1.0, 2.0 }, { 30.0, 40.0 } };
    auto* rObj = WKRectCreate(rc);
    Require(WKGetTypeID(rObj) == WKRectGetTypeID());
    WKRect rVal = WKRectGetValue(rObj);
    Require(rVal.origin.x == 1.0 && rVal.origin.y == 2.0);
    Require(rVal.size.width == 30.0 && rVal.size.height == 40.0);
    WKRelease(rObj);
}

static void TestUserContentURLPattern() {
    auto* patStr = WKStringCreateWithUTF8CString("http://*.example.com/foo/*");
    auto* pat = WKUserContentURLPatternCreate(patStr);
    Require(WKGetTypeID(pat) == WKUserContentURLPatternGetTypeID());
    Require(WKUserContentURLPatternIsValid(pat));
    Require(WKUserContentURLPatternMatchesSubdomains(pat));

    auto* schemeStr = WKUserContentURLPatternCopyScheme(pat);
    Require(WKStringIsEqualToUTF8CString(schemeStr, "http"));
    WKRelease(schemeStr);

    auto* hostStr = WKUserContentURLPatternCopyHost(pat);
    Require(WKStringIsEqualToUTF8CString(hostStr, "example.com"));
    WKRelease(hostStr);

    auto* url1 = WKURLCreateWithUTF8CString("http://example.com/foo/bar");
    Require(WKUserContentURLPatternMatchesURL(pat, url1));
    WKRelease(url1);

    auto* url2 = WKURLCreateWithUTF8CString("http://sub.example.com/foo/baz?query=1#hash");
    Require(WKUserContentURLPatternMatchesURL(pat, url2));
    WKRelease(url2);

    auto* urlPort = WKURLCreateWithUTF8CString("http://example.com:8080/foo/bar");
    Require(WKUserContentURLPatternMatchesURL(pat, urlPort));
    WKRelease(urlPort);

    auto* urlAuth = WKURLCreateWithUTF8CString("http://user:pass@example.com/foo/bar");
    Require(WKUserContentURLPatternMatchesURL(pat, urlAuth));
    WKRelease(urlAuth);

    auto* urlAuthPort = WKURLCreateWithUTF8CString("http://user:pass@example.com:8080/foo/bar");
    Require(WKUserContentURLPatternMatchesURL(pat, urlAuthPort));
    WKRelease(urlAuthPort);

    auto* ipv6Pat = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("http://[2001:db8::1]/api/*"));
    Require(WKUserContentURLPatternIsValid(ipv6Pat));
    auto* ipv6Url = WKURLCreateWithUTF8CString("http://user:secret@[2001:db8::1]:9000/api/v1");
    Require(WKUserContentURLPatternMatchesURL(ipv6Pat, ipv6Url));
    WKRelease(ipv6Url);
    WKRelease(ipv6Pat);

    auto* urlDiffScheme = WKURLCreateWithUTF8CString("https://example.com/foo/bar");
    Require(!WKUserContentURLPatternMatchesURL(pat, urlDiffScheme));
    WKRelease(urlDiffScheme);

    auto* urlDiffHost = WKURLCreateWithUTF8CString("http://other.com/foo/bar");
    Require(!WKUserContentURLPatternMatchesURL(pat, urlDiffHost));
    WKRelease(urlDiffHost);

    auto* urlDiffPath = WKURLCreateWithUTF8CString("http://example.com/bar");
    Require(!WKUserContentURLPatternMatchesURL(pat, urlDiffPath));
    WKRelease(urlDiffPath);

    auto* urlCase = WKURLCreateWithUTF8CString("HTTP://EXAMPLE.COM/foo/bar");
    Require(WKUserContentURLPatternMatchesURL(pat, urlCase));
    WKRelease(urlCase);

    auto* simplePat = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("http://example.com/*"));
    Require(WKUserContentURLPatternIsValid(simplePat));
    auto* queryUrl = WKURLCreateWithUTF8CString("http://example.com?x=1");
    Require(WKUserContentURLPatternMatchesURL(simplePat, queryUrl));
    WKRelease(queryUrl);
    auto* fragUrl = WKURLCreateWithUTF8CString("http://example.com#part");
    Require(WKUserContentURLPatternMatchesURL(simplePat, fragUrl));
    WKRelease(fragUrl);
    auto* literalStarUrl = WKURLCreateWithUTF8CString("http://example.com/*tail");
    Require(WKUserContentURLPatternMatchesURL(simplePat, literalStarUrl));
    WKRelease(literalStarUrl);
    WKRelease(simplePat);

    auto* upperPat = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("http://EXAMPLE.COM/*"));
    Require(WKUserContentURLPatternIsValid(upperPat));
    auto* lowerUrl = WKURLCreateWithUTF8CString("http://example.com/foo");
    Require(WKUserContentURLPatternMatchesURL(upperPat, lowerUrl));
    WKRelease(lowerUrl);
    WKRelease(upperPat);

    auto* allSchemes = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("*://example.com/*"));
    Require(WKUserContentURLPatternIsValid(allSchemes));
    auto* httpUrl = WKURLCreateWithUTF8CString("http://example.com/test");
    Require(WKUserContentURLPatternMatchesURL(allSchemes, httpUrl));
    WKRelease(httpUrl);
    auto* httpsUrl = WKURLCreateWithUTF8CString("https://example.com/test");
    Require(WKUserContentURLPatternMatchesURL(allSchemes, httpsUrl));
    WKRelease(httpsUrl);
    auto* ftpUrl = WKURLCreateWithUTF8CString("ftp://example.com/test");
    Require(!WKUserContentURLPatternMatchesURL(allSchemes, ftpUrl));
    WKRelease(ftpUrl);
    WKRelease(allSchemes);

    auto* filePat = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("file:///path/*"));
    Require(WKUserContentURLPatternIsValid(filePat));
    auto* fileUrl = WKURLCreateWithUTF8CString("file:///path/to/file");
    Require(WKUserContentURLPatternMatchesURL(filePat, fileUrl));
    WKRelease(fileUrl);
    WKRelease(filePat);

    auto* invalidPat = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("not_a_valid_pattern"));
    Require(!WKUserContentURLPatternIsValid(invalidPat));
    auto* testUrl = WKURLCreateWithUTF8CString("http://example.com/");
    Require(!WKUserContentURLPatternMatchesURL(invalidPat, testUrl));
    WKRelease(testUrl);
    auto* invalidScheme = WKUserContentURLPatternCopyScheme(invalidPat);
    Require(WKStringIsEmpty(invalidScheme));
    WKRelease(invalidScheme);
    WKRelease(invalidPat);

    auto* missingHost = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("http://"));
    Require(!WKUserContentURLPatternIsValid(missingHost));
    auto* missingHostScheme = WKUserContentURLPatternCopyScheme(missingHost);
    Require(WKStringIsEqualToUTF8CString(missingHostScheme, "http"));
    WKRelease(missingHostScheme);
    WKRelease(missingHost);

    auto* allHosts = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("https://*/"));
    Require(WKUserContentURLPatternIsValid(allHosts));
    Require(WKUserContentURLPatternMatchesSubdomains(allHosts));
    auto* allHostsHost = WKUserContentURLPatternCopyHost(allHosts);
    Require(WKStringIsEmpty(allHostsHost));
    WKRelease(allHostsHost);
    WKRelease(allHosts);

    auto* fileLocalhost = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("FILE://LocalHost/x"));
    Require(WKUserContentURLPatternIsValid(fileLocalhost));
    Require(!WKUserContentURLPatternMatchesSubdomains(fileLocalhost));
    auto* fileHost = WKUserContentURLPatternCopyHost(fileLocalhost);
    Require(WKStringIsEmpty(fileHost));
    WKRelease(fileHost);
    WKRelease(fileLocalhost);

    auto* fileNoHost = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("file:///"));
    Require(WKUserContentURLPatternIsValid(fileNoHost));
    WKRelease(fileNoHost);

    auto* innerStar = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("http://a.*.com/"));
    Require(!WKUserContentURLPatternIsValid(innerStar));
    auto* innerStarHost = WKUserContentURLPatternCopyHost(innerStar);
    Require(WKStringIsEqualToUTF8CString(innerStarHost, "a.*.com"));
    WKRelease(innerStarHost);
    WKRelease(innerStar);

    WKRelease(pat);
    WKRelease(patStr);
}

static void TestURLRequestResponse() {
    Require(WKURLRequestGetTypeID() == 23);
    Require(WKURLResponseGetTypeID() == 24);

    auto responseObj = NKWebKit::Make<NKWebKit::URLResponse>(
        NKWebKit::Make<NKWebKit::URL>("https://example.com/test.png"),
        NKWebKit::String::FromUtf8("image/png"),
        200,
        NKWebKit::String::FromUtf8("test.png"),
        true,
        123456789LL
    );
    const void* responseRef = NKWebKit::ToRef(responseObj);

    auto* url = WKURLResponseCopyURL(responseRef);
    auto* urlString = WKURLCopyString(url);
    Require(WKStringIsEqualToUTF8CString(urlString, "https://example.com/test.png"));
    WKRelease(urlString);
    WKRelease(url);

    auto* mime = WKURLResponseCopyMIMEType(responseRef);
    Require(WKStringIsEqualToUTF8CString(mime, "image/png"));
    WKRelease(mime);

    Require(WKURLResponseHTTPStatusCode(responseRef) == 200);

    auto* filename = WKURLResponseCopySuggestedFilename(responseRef);
    Require(WKStringIsEqualToUTF8CString(filename, "test.png"));
    WKRelease(filename);

    Require(WKURLResponseIsAttachment(responseRef));
    Require(WKURLResponseGetExpectedContentLength(responseRef) == 123456789u);
    WKRelease(responseRef);

    for (const auto& [stored, expected] : {std::pair<std::int64_t, std::uint32_t>{4294967297LL, 1u}, std::pair<std::int64_t, std::uint32_t>{-1LL, std::numeric_limits<std::uint32_t>::max()}}) {
        auto lengthObj = NKWebKit::Make<NKWebKit::URLResponse>(NKWebKit::Make<NKWebKit::URL>("https://example.com/"), NKWebKit::String::FromUtf8(""), 200, NKWebKit::String::FromUtf8(""), false, stored);
        const void* lengthRef = NKWebKit::ToRef(lengthObj);
        Require(WKURLResponseGetExpectedContentLength(lengthRef) == expected);
        WKRelease(lengthRef);
    }

    auto defaultObj = NKWebKit::Make<NKWebKit::URLResponse>();
    const void* defaultRef = NKWebKit::ToRef(defaultObj);

    Require(WKURLResponseCopyURL(defaultRef) == nullptr);

    auto* emptyMime = WKURLResponseCopyMIMEType(defaultRef);
    Require(WKStringIsEmpty(emptyMime));
    WKRelease(emptyMime);

    Require(WKURLResponseHTTPStatusCode(defaultRef) == 0);

    auto* emptyFilename = WKURLResponseCopySuggestedFilename(defaultRef);
    Require(WKStringIsEmpty(emptyFilename));
    WKRelease(emptyFilename);

    Require(!WKURLResponseIsAttachment(defaultRef));
    Require(WKURLResponseGetExpectedContentLength(defaultRef) == 0);
    WKRelease(defaultRef);

    bool threw = false;
    try {
        WKURLResponseCopyURL(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKURLResponseCopyMIMEType(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKURLResponseHTTPStatusCode(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKURLResponseCopySuggestedFilename(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKURLResponseIsAttachment(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKURLResponseGetExpectedContentLength(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestError() {
    Require(WKErrorGetTypeID() == 10);
    auto* domain = WKErrorCopyWKErrorDomain();
    Require(WKStringIsEqualToUTF8CString(domain, "WebKitErrorDomain"));
    WKRelease(domain);
}

static void TestHitTestResult() {
    Require(WKHitTestResultGetTypeID() == 63);
    auto hit = NKWebKit::Make<NKWebKit::HitTestResult>();
    hit->absoluteImageURL = "https://example.com/img.png";
    hit->absoluteLinkURL = "https://example.com/link";
    hit->absoluteMediaURL = "https://example.com/media.mp4";
    hit->absolutePDFURL = "https://example.com/doc.pdf";
    hit->linkLabel = u"Click here";
    hit->linkTitle = u"Title";
    hit->lookupText = u"Search text";
    hit->elementBoundingBox = WKRect{{1.0, 2.0}, {30.0, 40.0}};
    hit->isContentEditable = true;

    const void* ref = NKWebKit::ToRef(hit);
    auto* imgUrl = WKHitTestResultCopyAbsoluteImageURL(ref);
    Require(imgUrl != nullptr);
    auto* imgUrlStr = WKURLCopyString(imgUrl);
    Require(WKStringIsEqualToUTF8CString(imgUrlStr, "https://example.com/img.png"));
    WKRelease(imgUrlStr);
    WKRelease(imgUrl);

    auto* linkUrl = WKHitTestResultCopyAbsoluteLinkURL(ref);
    Require(linkUrl != nullptr);
    auto* linkUrlStr = WKURLCopyString(linkUrl);
    Require(WKStringIsEqualToUTF8CString(linkUrlStr, "https://example.com/link"));
    WKRelease(linkUrlStr);
    WKRelease(linkUrl);

    auto* mediaUrl = WKHitTestResultCopyAbsoluteMediaURL(ref);
    Require(mediaUrl != nullptr);
    auto* mediaUrlStr = WKURLCopyString(mediaUrl);
    Require(WKStringIsEqualToUTF8CString(mediaUrlStr, "https://example.com/media.mp4"));
    WKRelease(mediaUrlStr);
    WKRelease(mediaUrl);

    auto* pdfUrl = WKHitTestResultCopyAbsolutePDFURL(ref);
    Require(pdfUrl != nullptr);
    auto* pdfUrlStr = WKURLCopyString(pdfUrl);
    Require(WKStringIsEqualToUTF8CString(pdfUrlStr, "https://example.com/doc.pdf"));
    WKRelease(pdfUrlStr);
    WKRelease(pdfUrl);

    auto* label = WKHitTestResultCopyLinkLabel(ref);
    Require(WKStringIsEqualToUTF8CString(label, "Click here"));
    WKRelease(label);

    auto* title = WKHitTestResultCopyLinkTitle(ref);
    Require(WKStringIsEqualToUTF8CString(title, "Title"));
    WKRelease(title);

    auto* lookup = WKHitTestResultCopyLookupText(ref);
    Require(WKStringIsEqualToUTF8CString(lookup, "Search text"));
    WKRelease(lookup);

    WKRect box = WKHitTestResultGetElementBoundingBox(ref);
    Require(box.origin.x == 1.0 && box.origin.y == 2.0);
    Require(box.size.width == 30.0 && box.size.height == 40.0);
    Require(WKHitTestResultIsContentEditable(ref));

    auto unset = NKWebKit::Make<NKWebKit::HitTestResult>();
    const void* unsetRef = NKWebKit::ToRef(unset);
    Require(WKHitTestResultCopyAbsoluteImageURL(unsetRef) == nullptr);
    Require(WKHitTestResultCopyAbsoluteLinkURL(unsetRef) == nullptr);
    Require(WKHitTestResultCopyAbsoluteMediaURL(unsetRef) == nullptr);
    Require(WKHitTestResultCopyAbsolutePDFURL(unsetRef) == nullptr);
    WKRelease(unsetRef);

    bool threw = false;
    try {
        WKHitTestResultCopyLinkLabel(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    WKRelease(ref);
}

static void TestBundleHitTestResult() {
    Require(WKBundleHitTestResultGetTypeID() == 120);
    auto hit = NKWebKit::Make<NKWebKit::BundleHitTestResult>();
    const void* ref = hit.Get();

    Require(WKBundleHitTestResultCopyAbsoluteImageURL(ref) == nullptr);
    Require(WKBundleHitTestResultCopyAbsoluteLinkURL(ref) == nullptr);
    Require(WKBundleHitTestResultCopyAbsoluteMediaURL(ref) == nullptr);
    Require(WKBundleHitTestResultCopyAbsolutePDFURL(ref) == nullptr);
    auto* unsetLabel = WKBundleHitTestResultCopyLinkLabel(ref);
    Require(unsetLabel != nullptr);
    Require(WKStringIsEqualToUTF8CString(unsetLabel, ""));
    WKRelease(unsetLabel);
    auto* unsetTitle = WKBundleHitTestResultCopyLinkTitle(ref);
    Require(unsetTitle != nullptr);
    Require(WKStringIsEqualToUTF8CString(unsetTitle, ""));
    WKRelease(unsetTitle);

    hit->absoluteImageURL = NKWebKit::Make<NKWebKit::URL>("https://example.com/img.png");
    hit->absoluteLinkURL = NKWebKit::Make<NKWebKit::URL>("https://example.com/link");
    hit->absoluteMediaURL = NKWebKit::Make<NKWebKit::URL>("https://example.com/media.mp4");
    hit->absolutePDFURL = NKWebKit::Make<NKWebKit::URL>("https://example.com/doc.pdf");
    hit->linkLabel = NKWebKit::String::FromUtf8("Example Link");
    hit->linkTitle = NKWebKit::String::FromUtf8("Example Title");

    auto* imgUrl = WKBundleHitTestResultCopyAbsoluteImageURL(ref);
    Require(imgUrl != nullptr);
    auto* imgStr = WKURLCopyString(imgUrl);
    Require(WKStringIsEqualToUTF8CString(imgStr, "https://example.com/img.png"));
    WKRelease(imgStr);
    WKRelease(imgUrl);

    auto* linkUrl = WKBundleHitTestResultCopyAbsoluteLinkURL(ref);
    Require(linkUrl != nullptr);
    auto* linkStr = WKURLCopyString(linkUrl);
    Require(WKStringIsEqualToUTF8CString(linkStr, "https://example.com/link"));
    WKRelease(linkStr);
    WKRelease(linkUrl);

    auto* mediaUrl = WKBundleHitTestResultCopyAbsoluteMediaURL(ref);
    Require(mediaUrl != nullptr);
    auto* mediaStr = WKURLCopyString(mediaUrl);
    Require(WKStringIsEqualToUTF8CString(mediaStr, "https://example.com/media.mp4"));
    WKRelease(mediaStr);
    WKRelease(mediaUrl);

    auto* pdfUrl = WKBundleHitTestResultCopyAbsolutePDFURL(ref);
    Require(pdfUrl != nullptr);
    auto* pdfStr = WKURLCopyString(pdfUrl);
    Require(WKStringIsEqualToUTF8CString(pdfStr, "https://example.com/doc.pdf"));
    WKRelease(pdfStr);
    WKRelease(pdfUrl);

    auto* label = WKBundleHitTestResultCopyLinkLabel(ref);
    Require(label != nullptr);
    Require(WKStringIsEqualToUTF8CString(label, "Example Link"));
    WKRelease(label);

    auto* title = WKBundleHitTestResultCopyLinkTitle(ref);
    Require(title != nullptr);
    Require(WKStringIsEqualToUTF8CString(title, "Example Title"));
    WKRelease(title);

    bool threw = false;
    try {
        WKBundleHitTestResultCopyLinkLabel(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestDownload() {
    Require(WKDownloadGetTypeID() == 52);
    auto download = NKWebKit::Make<NKWebKit::Download>();
    const void* ref = download.Get();

    Require(!WKDownloadGetWasUserInitiated(ref));
    Require(WKDownloadCopyRequest(ref) == nullptr);
    Require(WKDownloadGetOriginatingPage(ref) == nullptr);

    download->wasUserInitiated = true;
    Require(WKDownloadGetWasUserInitiated(ref));

    auto dummyReq = NKWebKit::Make<NKWebKit::Data>(std::vector<std::uint8_t>{1, 2, 3});
    download->request = std::move(dummyReq);
    auto* req = WKDownloadCopyRequest(ref);
    Require(req != nullptr);
    Require(WKGetTypeID(req) == WKDataGetTypeID());
    WKRelease(req);

    bool threw = false;
    try {
        WKDownloadGetWasUserInitiated(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestAXObject() {
    Require(WKAXObjectGetTypeID() == 132);
    auto ax = NKWebKit::Make<NKWebKit::AccessibilityObject>();
    const void* ref = ax.Get();

    Require(WKAXObjectCopyTitle(ref) == nullptr);
    Require(WKAXObjectCopyDescription(ref) == nullptr);
    Require(WKAXObjectCopyHelpText(ref) == nullptr);
    Require(WKAXObjectCopyURL(ref) == nullptr);

    ax->title = NKWebKit::String::FromUtf8("Submit Button");
    ax->description = NKWebKit::String::FromUtf8("Submits the form");
    ax->helpText = NKWebKit::String::FromUtf8("Click to send your input");
    ax->url = NKWebKit::String::FromUtf8("https://example.com/action");

    auto* title = WKAXObjectCopyTitle(ref);
    Require(title != nullptr);
    Require(WKStringIsEqualToUTF8CString(title, "Submit Button"));
    WKRelease(title);

    auto* desc = WKAXObjectCopyDescription(ref);
    Require(desc != nullptr);
    Require(WKStringIsEqualToUTF8CString(desc, "Submits the form"));
    WKRelease(desc);

    auto* help = WKAXObjectCopyHelpText(ref);
    Require(help != nullptr);
    Require(WKStringIsEqualToUTF8CString(help, "Click to send your input"));
    WKRelease(help);

    auto* url = WKAXObjectCopyURL(ref);
    Require(url != nullptr);
    Require(WKGetTypeID(url) == WKStringGetTypeID());
    Require(WKStringIsEqualToUTF8CString(url, "https://example.com/action"));
    WKRelease(url);

    bool threw = false;
    try {
        WKAXObjectCopyTitle(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestPopupMenuItem() {
    Require(WKPopupMenuItemGetTypeID() == 131);
    auto item = NKWebKit::Make<NKWebKit::PopupMenuItem>();
    const void* ref = item.Get();

    Require(WKPopupMenuItemIsEnabled(ref));
    Require(WKPopupMenuItemIsItem(ref));
    Require(!WKPopupMenuItemIsSeparator(ref));
    Require(WKPopupMenuItemCopyText(ref) == nullptr);

    item->text = NKWebKit::String::FromUtf8("Option 1");
    auto* text = WKPopupMenuItemCopyText(ref);
    Require(text != nullptr);
    Require(WKStringIsEqualToUTF8CString(text, "Option 1"));
    WKRelease(text);

    item->enabled = false;
    Require(!WKPopupMenuItemIsEnabled(ref));
    Require(WKPopupMenuItemIsItem(ref));
    Require(!WKPopupMenuItemIsSeparator(ref));

    item->enabled = true;
    item->isItem = false;
    Require(WKPopupMenuItemIsEnabled(ref));
    Require(!WKPopupMenuItemIsItem(ref));
    Require(!WKPopupMenuItemIsSeparator(ref));

    item->isItem = true;
    item->isSeparator = true;
    Require(WKPopupMenuItemIsEnabled(ref));
    Require(WKPopupMenuItemIsItem(ref));
    Require(WKPopupMenuItemIsSeparator(ref));

    bool threw = false;
    try {
        WKPopupMenuItemIsEnabled(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestOpenPanelResultListener() {
    Require(WKOpenPanelResultListenerGetTypeID() == 80);

    auto listener = NKWebKit::Make<NKWebKit::OpenPanelResultListener>();
    const void* ref = listener.Get();

    Require(!listener->cancelled);
    Require(listener->chosenFiles.empty());

    WKOpenPanelResultListenerCancel(ref);
    Require(listener->cancelled);

    auto urlObj = NKWebKit::Make<NKWebKit::URL>("file:///path/to/file.txt");
    const void* urlValues[] = { urlObj.Get() };
    const void* fileArray = WKArrayCreate(urlValues, 1);

    WKOpenPanelResultListenerChooseFiles(ref, fileArray, nullptr);
    Require(!listener->cancelled);
    Require(listener->chosenFiles.size() == 1);
    Require(listener->chosenFiles[0]->Utf8() == "/path/to/file.txt");
    WKRelease(fileArray);

    WKOpenPanelResultListenerChooseFiles(ref, nullptr, nullptr);
    Require(listener->chosenFiles.empty());

    bool threw = false;
    try {
        WKOpenPanelResultListenerCancel(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKOpenPanelResultListenerChooseFiles(nullptr, nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestCertificateInfo() {
    Require(WKCertificateInfoGetTypeID() == 0);
    Require(WKCertificateInfoCreateWithCertficateChain(nullptr) == nullptr);
    Require(WKCertificateInfoGetVerificationError(nullptr) == 0);
    Require(WKCertificateInfoCopyVerificationErrorDescription(nullptr) == nullptr);
    Require(WKCertificateInfoGetCertificateChainSize(nullptr) == 0);
    Require(WKCertificateInfoCopyCertificateAtIndex(nullptr, 0) == nullptr);
}

int main() {
    TestTypeIDs();
    TestRetainRelease();
    TestStrings();
    TestURL();
    TestArray();
    TestDictionary();
    TestData();
    TestNumbers();
    TestGeometry();
    TestUserContentURLPattern();
    TestURLRequestResponse();
    TestError();
    TestHitTestResult();
    TestBundleHitTestResult();
    TestDownload();
    TestAXObject();
    TestPopupMenuItem();
    TestOpenPanelResultListener();
    TestCertificateInfo();
    std::printf("All libSceNKWebKit Base tests passed!\n");
    return 0;
}

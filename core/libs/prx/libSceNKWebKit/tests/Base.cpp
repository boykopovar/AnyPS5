#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceNKWebKit/src/NavigationData.hpp"
#include "prx/libSceNKWebKit/src/BackForwardList.hpp"
#include "prx/libSceNKWebKit/src/ProtectionSpace.hpp"
#include "prx/libSceNKWebKit/src/FrameHandle.hpp"
#include "prx/libSceNKWebKit/src/OpenPanelParameters.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/src/RenderTree.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <thread>
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
    Require(WKRenderLayerGetTypeID() == static_cast<std::uint32_t>(NKWebKit::ObjectType::RenderLayer));
    Require(WKRenderObjectGetTypeID() == static_cast<std::uint32_t>(NKWebKit::ObjectType::RenderObject));
    Require(WKSessionStateGetTypeID() == 19);
    Require(WKSerializedScriptValueGetTypeID() == 20);
    Require(WKContextGetTypeID() == 85);
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

    auto* scheme = WKURLCopyScheme(u);
    Require(WKStringIsEqualToUTF8CString(scheme, "https"));
    WKRelease(scheme);

    auto* host = WKURLCopyHostName(u);
    Require(WKStringIsEqualToUTF8CString(host, "example.com"));
    WKRelease(host);

    auto* path = WKURLCopyPath(u);
    Require(WKStringIsEqualToUTF8CString(path, "/test"));
    WKRelease(path);

    auto* lastComp = WKURLCopyLastPathComponent(u);
    Require(WKStringIsEqualToUTF8CString(lastComp, "test"));
    WKRelease(lastComp);

    auto* same = WKURLCreateWithUTF8CString("https://example.com/test?a=1#frag");
    auto* diff = WKURLCreateWithUTF8CString("https://example.com/other");
    Require(WKURLIsEqual(u, u));
    Require(WKURLIsEqual(u, same));
    Require(!WKURLIsEqual(u, diff));
    Require(!WKURLIsEqual(u, nullptr));
    Require(WKURLIsEqual(nullptr, nullptr));
    WKRelease(same);
    WKRelease(diff);

    auto* fileUrl = WKURLCreateWithUTF8CString("file:///path/to/folder/file.tar.gz");
    auto* fileScheme = WKURLCopyScheme(fileUrl);
    Require(WKStringIsEqualToUTF8CString(fileScheme, "file"));
    WKRelease(fileScheme);
    auto* fileHost = WKURLCopyHostName(fileUrl);
    Require(WKStringIsEqualToUTF8CString(fileHost, ""));
    WKRelease(fileHost);
    auto* filePath = WKURLCopyPath(fileUrl);
    Require(WKStringIsEqualToUTF8CString(filePath, "/path/to/folder/file.tar.gz"));
    WKRelease(filePath);
    auto* fileLast = WKURLCopyLastPathComponent(fileUrl);
    Require(WKStringIsEqualToUTF8CString(fileLast, "file.tar.gz"));
    WKRelease(fileLast);
    WKRelease(fileUrl);

    auto* authUrl = WKURLCreateWithUTF8CString("http://user:secret@sub.domain.org:8080/dir/subdir/");
    auto* authHost = WKURLCopyHostName(authUrl);
    Require(WKStringIsEqualToUTF8CString(authHost, "sub.domain.org"));
    WKRelease(authHost);
    auto* authPath = WKURLCopyPath(authUrl);
    Require(WKStringIsEqualToUTF8CString(authPath, "/dir/subdir/"));
    WKRelease(authPath);
    auto* authLast = WKURLCopyLastPathComponent(authUrl);
    Require(WKStringIsEqualToUTF8CString(authLast, "subdir"));
    WKRelease(authLast);
    WKRelease(authUrl);

    auto* ipv6Url = WKURLCreateWithUTF8CString("http://[2001:db8::1]:9000/api");
    auto* ipv6Host = WKURLCopyHostName(ipv6Url);
    Require(WKStringIsEqualToUTF8CString(ipv6Host, "[2001:db8::1]"));
    WKRelease(ipv6Host);
    WKRelease(ipv6Url);

    auto* aboutUrl = WKURLCreateWithUTF8CString("about:blank");
    auto* aboutScheme = WKURLCopyScheme(aboutUrl);
    Require(WKStringIsEqualToUTF8CString(aboutScheme, "about"));
    WKRelease(aboutScheme);
    auto* aboutPath = WKURLCopyPath(aboutUrl);
    Require(WKStringIsEqualToUTF8CString(aboutPath, "blank"));
    WKRelease(aboutPath);
    auto* aboutLast = WKURLCopyLastPathComponent(aboutUrl);
    Require(WKStringIsEqualToUTF8CString(aboutLast, "blank"));
    WKRelease(aboutLast);
    WKRelease(aboutUrl);

    auto* rootUrl = WKURLCreateWithUTF8CString("https://example.com/");
    auto* rootPath = WKURLCopyPath(rootUrl);
    Require(WKStringIsEqualToUTF8CString(rootPath, "/"));
    WKRelease(rootPath);
    auto* rootLast = WKURLCopyLastPathComponent(rootUrl);
    Require(WKStringIsEqualToUTF8CString(rootLast, ""));
    WKRelease(rootLast);
    WKRelease(rootUrl);

    auto* base = WKURLCreateWithUTF8CString("https://example.com/dir/page.html");
    auto* r1 = WKURLCreateWithBaseURL(base, "other.html");
    auto* s1 = WKURLCopyString(r1);
    Require(WKStringIsEqualToUTF8CString(s1, "https://example.com/dir/other.html"));
    WKRelease(s1);
    WKRelease(r1);

    auto* r2 = WKURLCreateWithBaseURL(base, "../top.html");
    auto* s2 = WKURLCopyString(r2);
    Require(WKStringIsEqualToUTF8CString(s2, "https://example.com/top.html"));
    WKRelease(s2);
    WKRelease(r2);

    auto* r3 = WKURLCreateWithBaseURL(base, "/root.html");
    auto* s3 = WKURLCopyString(r3);
    Require(WKStringIsEqualToUTF8CString(s3, "https://example.com/root.html"));
    WKRelease(s3);
    WKRelease(r3);

    auto* r4 = WKURLCreateWithBaseURL(base, "?query=1");
    auto* s4 = WKURLCopyString(r4);
    Require(WKStringIsEqualToUTF8CString(s4, "https://example.com/dir/page.html?query=1"));
    WKRelease(s4);
    WKRelease(r4);

    auto* r5 = WKURLCreateWithBaseURL(base, "#section");
    auto* s5 = WKURLCopyString(r5);
    Require(WKStringIsEqualToUTF8CString(s5, "https://example.com/dir/page.html#section"));
    WKRelease(s5);
    WKRelease(r5);

    auto* r6 = WKURLCreateWithBaseURL(base, "//cdn.example.org/lib.js");
    auto* s6 = WKURLCopyString(r6);
    Require(WKStringIsEqualToUTF8CString(s6, "https://cdn.example.org/lib.js"));
    WKRelease(s6);
    WKRelease(r6);

    auto* r7 = WKURLCreateWithBaseURL(base, "http://standalone.org/");
    auto* s7 = WKURLCopyString(r7);
    Require(WKStringIsEqualToUTF8CString(s7, "http://standalone.org/"));
    WKRelease(s7);
    WKRelease(r7);

    auto* r8 = WKURLCreateWithBaseURL(base, nullptr);
    auto* s8 = WKURLCopyString(r8);
    Require(WKStringIsEqualToUTF8CString(s8, "https://example.com/dir/page.html"));
    WKRelease(s8);
    WKRelease(r8);

    auto* r9 = WKURLCreateWithBaseURL(nullptr, "https://example.com/");
    auto* s9 = WKURLCopyString(r9);
    Require(WKStringIsEqualToUTF8CString(s9, "https://example.com/"));
    WKRelease(s9);
    WKRelease(r9);

    auto* baseOrg = WKURLCreateWithUTF8CString("https://example.org/dir/page");
    auto* rDotDot = WKURLCreateWithBaseURL(baseOrg, "/a/../b");
    auto* sDotDot = WKURLCopyString(rDotDot);
    Require(WKStringIsEqualToUTF8CString(sDotDot, "https://example.org/b"));
    WKRelease(sDotDot);
    WKRelease(rDotDot);

    auto* rDot = WKURLCreateWithBaseURL(baseOrg, "/a/./b");
    auto* sDot = WKURLCopyString(rDot);
    Require(WKStringIsEqualToUTF8CString(sDot, "https://example.org/a/b"));
    WKRelease(sDot);
    WKRelease(rDot);

    auto* rDoubleSlash = WKURLCreateWithBaseURL(baseOrg, "/a//b");
    auto* sDoubleSlash = WKURLCopyString(rDoubleSlash);
    Require(WKStringIsEqualToUTF8CString(sDoubleSlash, "https://example.org/a//b"));
    WKRelease(sDoubleSlash);
    WKRelease(rDoubleSlash);

    auto* rRelDoubleSlash = WKURLCreateWithBaseURL(baseOrg, "sub//item");
    auto* sRelDoubleSlash = WKURLCopyString(rRelDoubleSlash);
    Require(WKStringIsEqualToUTF8CString(sRelDoubleSlash, "https://example.org/dir/sub//item"));
    WKRelease(sRelDoubleSlash);
    WKRelease(rRelDoubleSlash);
    WKRelease(baseOrg);

    auto* baseFrag = WKURLCreateWithUTF8CString("https://example.org/a?q#old");
    auto* rEmpty = WKURLCreateWithBaseURL(baseFrag, "");
    auto* sEmpty = WKURLCopyString(rEmpty);
    Require(WKStringIsEqualToUTF8CString(sEmpty, "https://example.org/a?q"));
    WKRelease(sEmpty);
    WKRelease(rEmpty);
    WKRelease(baseFrag);

    auto* upperUrl = WKURLCreateWithUTF8CString("HTTP://Example.COM/a");
    auto* upperScheme = WKURLCopyScheme(upperUrl);
    Require(WKStringIsEqualToUTF8CString(upperScheme, "http"));
    WKRelease(upperScheme);
    auto* upperHost = WKURLCopyHostName(upperUrl);
    Require(WKStringIsEqualToUTF8CString(upperHost, "example.com"));
    WKRelease(upperHost);
    auto* upperStr = WKURLCopyString(upperUrl);
    Require(WKStringIsEqualToUTF8CString(upperStr, "HTTP://Example.COM/a"));
    WKRelease(upperStr);
    auto* lowerUrl = WKURLCreateWithUTF8CString("http://example.com/a");
    Require(WKURLIsEqual(upperUrl, lowerUrl));
    WKRelease(lowerUrl);
    WKRelease(upperUrl);

    auto* bslashUrl = WKURLCreateWithUTF8CString("   https://example.com/dir\\file.txt   ");
    auto* bslashPath = WKURLCopyPath(bslashUrl);
    Require(WKStringIsEqualToUTF8CString(bslashPath, "/dir/file.txt"));
    WKRelease(bslashPath);
    auto* bslashLast = WKURLCopyLastPathComponent(bslashUrl);
    Require(WKStringIsEqualToUTF8CString(bslashLast, "file.txt"));
    WKRelease(bslashLast);
    auto* bslashStr = WKURLCopyString(bslashUrl);
    Require(WKStringIsEqualToUTF8CString(bslashStr, "   https://example.com/dir\\file.txt   "));
    WKRelease(bslashStr);
    WKRelease(bslashUrl);

    WKRelease(base);
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

    bool rejectedNull = false;
    try {
        WKArrayCreate(nullptr, 1);
    } catch (...) {
        rejectedNull = true;
    }
    Require(rejectedNull);
    rejectedNull = false;
    try {
        WKArrayCreateAdoptingValues(nullptr, 1);
    } catch (...) {
        rejectedNull = true;
    }
    Require(rejectedNull);
    rejectedNull = false;
    try {
        WKDictionaryCreate(nullptr, nullptr, 1);
    } catch (...) {
        rejectedNull = true;
    }
    Require(rejectedNull);

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

    Require(WKURLRequestCreateWithWKURL(nullptr) == nullptr);

    auto* url = WKURLCreateWithUTF8CString("https://playstation.com/en-us/");
    auto* request = WKURLRequestCreateWithWKURL(url);
    Require(request != nullptr);
    Require(WKGetTypeID(request) == WKURLRequestGetTypeID());

    auto* reqUrl = WKURLRequestCopyURL(request);
    Require(reqUrl != nullptr);
    auto* reqUrlStr = WKURLCopyString(reqUrl);
    Require(WKStringIsEqualToUTF8CString(reqUrlStr, "https://playstation.com/en-us/"));
    WKRelease(reqUrlStr);
    WKRelease(reqUrl);

    auto* partyUrl = WKURLRequestCopyFirstPartyForCookies(request);
    Require(partyUrl == nullptr);

    auto* method = WKURLRequestCopyHTTPMethod(request);
    Require(method != nullptr);
    Require(WKStringIsEqualToUTF8CString(method, "GET"));
    WKRelease(method);

    const unsigned char postData[] = "test=123";
    auto* bodyData = WKDataCreate(postData, sizeof(postData) - 1);
    auto* modifiedRequest = WKURLRequestCopySettingHTTPBody(request, bodyData);
    Require(modifiedRequest != nullptr);
    Require(modifiedRequest != request);

    auto* modMethod = WKURLRequestCopyHTTPMethod(modifiedRequest);
    Require(WKStringIsEqualToUTF8CString(modMethod, "GET"));
    WKRelease(modMethod);

    auto* clearedRequest = WKURLRequestCopySettingHTTPBody(modifiedRequest, nullptr);
    Require(clearedRequest != nullptr);
    WKRelease(clearedRequest);

    WKRelease(modifiedRequest);
    WKRelease(bodyData);
    WKRelease(request);
    WKRelease(url);

    auto responseObj = NKWebKit::Make<NKWebKit::URLResponse>(
        NKWebKit::Make<NKWebKit::URL>("https://example.com/test.png"),
        NKWebKit::String::FromUtf8("image/png"),
        200,
        NKWebKit::String::FromUtf8("test.png"),
        true,
        123456789LL
    );
    const void* responseRef = NKWebKit::ToRef(responseObj);

    auto* responseUrl = WKURLResponseCopyURL(responseRef);
    auto* urlString = WKURLCopyString(responseUrl);
    Require(WKStringIsEqualToUTF8CString(urlString, "https://example.com/test.png"));
    WKRelease(urlString);
    WKRelease(responseUrl);

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

template<typename Function>
static bool ThrowsNavigationData(Function function, const void* ref) {
    try {
        function(ref);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void TestNavigationData() {
    auto* data = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::NavigationData>(NKWebKit::String::FromUtf8("Page Title"), std::string("https://example.com/final"), std::string("https://example.com/original")));
    Require(WKGetTypeID(data) == WKNavigationDataGetTypeID());
    auto* title = WKNavigationDataCopyTitle(data);
    Require(WKStringIsEqualToUTF8CString(title, "Page Title"));
    WKRelease(title);
    auto* url = WKNavigationDataCopyURL(data);
    auto* urlString = WKURLCopyString(url);
    Require(WKStringIsEqualToUTF8CString(urlString, "https://example.com/original"));
    WKRelease(urlString);
    WKRelease(url);
    auto* destination = WKNavigationDataCopyNavigationDestinationURL(data);
    auto* destinationString = WKURLCopyString(destination);
    Require(WKStringIsEqualToUTF8CString(destinationString, "https://example.com/final"));
    WKRelease(destinationString);
    WKRelease(destination);
    WKRelease(data);

    auto* empty = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::NavigationData>());
    auto* emptyTitle = WKNavigationDataCopyTitle(empty);
    Require(WKStringIsEmpty(emptyTitle));
    WKRelease(emptyTitle);
    Require(WKNavigationDataCopyURL(empty) == nullptr);
    Require(WKNavigationDataCopyNavigationDestinationURL(empty) == nullptr);
    WKRelease(empty);

    auto* emptyURL = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::NavigationData>(nullptr, std::string(), std::string()));
    auto* blank = WKNavigationDataCopyNavigationDestinationURL(emptyURL);
    Require(blank != nullptr);
    auto* blankString = WKURLCopyString(blank);
    Require(WKStringIsEmpty(blankString));
    WKRelease(blankString);
    WKRelease(blank);
    WKRelease(emptyURL);

    auto* notData = NKWebKit::ToRef(NKWebKit::String::FromUtf8("not navigation data"));
    Require(ThrowsNavigationData(WKNavigationDataCopyTitle, notData));
    Require(ThrowsNavigationData(WKNavigationDataCopyURL, notData));
    Require(ThrowsNavigationData(WKNavigationDataCopyNavigationDestinationURL, notData));
    Require(ThrowsNavigationData(WKNavigationDataCopyTitle, nullptr));
    Require(ThrowsNavigationData(WKNavigationDataCopyURL, nullptr));
    WKRelease(notData);
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

static NKWebKit::Retained<NKWebKit::BackForwardListItem> MakeBackForwardItem(const char* url, const char* title, const char* originalURL) {
    return NKWebKit::Make<NKWebKit::BackForwardListItem>(std::string(url), NKWebKit::String::FromUtf8(title), std::string(originalURL));
}

template<typename Function>
static bool Throws(Function function, const void* ref) {
    try {
        function(ref);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void TestBackForwardList() {
    using ItemList = std::vector<NKWebKit::Retained<NKWebKit::BackForwardListItem>>;
    ItemList entries;
    for (int index = 0; index < 5; ++index) {
        std::string url = "https://example.com/" + std::to_string(index);
        entries.push_back(MakeBackForwardItem(url.c_str(), url.c_str(), url.c_str()));
    }
    const void* items[5];
    for (int index = 0; index < 5; ++index) items[index] = entries[index].Get();

    auto* list = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::BackForwardList>(entries, 2));
    Require(WKGetTypeID(list) == WKBackForwardListGetTypeID());
    Require(WKBackForwardListGetBackListCount(list) == 2);
    Require(WKBackForwardListGetForwardListCount(list) == 2);
    Require(WKBackForwardListGetCurrentItem(list) == items[2]);
    Require(WKBackForwardListGetBackItem(list) == items[1]);
    Require(WKBackForwardListGetForwardItem(list) == items[3]);
    Require(WKBackForwardListGetItemAtIndex(list, 0) == items[2]);
    Require(WKBackForwardListGetItemAtIndex(list, -2) == items[0]);
    Require(WKBackForwardListGetItemAtIndex(list, 2) == items[4]);
    Require(WKBackForwardListGetItemAtIndex(list, -3) == nullptr);
    Require(WKBackForwardListGetItemAtIndex(list, 3) == nullptr);
    Require(WKBackForwardListGetItemAtIndex(list, INT32_MIN) == nullptr);
    Require(WKBackForwardListGetItemAtIndex(list, INT32_MAX) == nullptr);

    auto* back = WKBackForwardListCopyBackListWithLimit(list, 1);
    Require(WKArrayGetSize(back) == 1);
    Require(WKArrayGetItemAtIndex(back, 0) == items[1]);
    WKRelease(back);
    back = WKBackForwardListCopyBackListWithLimit(list, 10);
    Require(WKArrayGetSize(back) == 2);
    Require(WKArrayGetItemAtIndex(back, 0) == items[0]);
    Require(WKArrayGetItemAtIndex(back, 1) == items[1]);
    WKRelease(back);
    back = WKBackForwardListCopyBackListWithLimit(list, 0);
    Require(WKArrayGetSize(back) == 0);
    WKRelease(back);
    auto* forward = WKBackForwardListCopyForwardListWithLimit(list, 1);
    Require(WKArrayGetSize(forward) == 1);
    Require(WKArrayGetItemAtIndex(forward, 0) == items[3]);
    WKRelease(forward);
    forward = WKBackForwardListCopyForwardListWithLimit(list, 0xFFFFFFFFu);
    Require(WKArrayGetSize(forward) == 2);
    Require(WKArrayGetItemAtIndex(forward, 1) == items[4]);
    WKRelease(forward);

    auto* url = WKBackForwardListItemCopyURL(items[1]);
    auto* urlString = WKURLCopyString(url);
    Require(WKStringIsEqualToUTF8CString(urlString, "https://example.com/1"));
    WKRelease(urlString);
    WKRelease(url);
    auto* title = WKBackForwardListItemCopyTitle(items[1]);
    Require(WKStringIsEqualToUTF8CString(title, "https://example.com/1"));
    WKRelease(title);
    auto* original = WKBackForwardListItemCopyOriginalURL(items[3]);
    auto* originalString = WKURLCopyString(original);
    Require(WKStringIsEqualToUTF8CString(originalString, "https://example.com/3"));
    WKRelease(originalString);
    WKRelease(original);

    WKBackForwardListClear(list);
    Require(WKBackForwardListGetBackListCount(list) == 0);
    Require(WKBackForwardListGetForwardListCount(list) == 0);
    Require(WKBackForwardListGetCurrentItem(list) == items[2]);
    Require(WKBackForwardListGetBackItem(list) == nullptr);
    Require(WKBackForwardListGetForwardItem(list) == nullptr);
    Require(WKBackForwardListGetItemAtIndex(list, 0) == items[2]);
    WKRelease(list);

    ItemList pair{entries[0], entries[1]};
    auto* atStart = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::BackForwardList>(pair, 0));
    Require(WKBackForwardListGetBackItem(atStart) == nullptr);
    Require(WKBackForwardListGetForwardItem(atStart) == items[1]);
    Require(WKBackForwardListGetItemAtIndex(atStart, -1) == nullptr);
    WKRelease(atStart);

    auto* atEnd = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::BackForwardList>(pair, 1));
    Require(WKBackForwardListGetBackItem(atEnd) == items[0]);
    Require(WKBackForwardListGetForwardItem(atEnd) == nullptr);
    Require(WKBackForwardListGetForwardListCount(atEnd) == 0);
    WKRelease(atEnd);

    auto* noCurrent = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::BackForwardList>(pair, std::nullopt));
    Require(WKBackForwardListGetCurrentItem(noCurrent) == nullptr);
    Require(WKBackForwardListGetBackListCount(noCurrent) == 0);
    Require(WKBackForwardListGetForwardListCount(noCurrent) == 0);
    Require(WKBackForwardListGetItemAtIndex(noCurrent, 0) == nullptr);
    auto* none = WKBackForwardListCopyForwardListWithLimit(noCurrent, 5);
    Require(WKArrayGetSize(none) == 0);
    WKRelease(none);
    WKBackForwardListClear(noCurrent);
    Require(NKWebKit::Cast<NKWebKit::BackForwardList>(noCurrent, "test")->entries.empty());
    WKRelease(noCurrent);

    auto* single = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::BackForwardList>(ItemList{entries[0]}, 0));
    WKBackForwardListClear(single);
    Require(WKBackForwardListGetCurrentItem(single) == items[0]);
    WKRelease(single);

    auto detached = NKWebKit::Make<NKWebKit::BackForwardList>(entries, 2);
    detached->attached = false;
    auto* detachedRef = NKWebKit::ToRef(std::move(detached));
    Require(WKBackForwardListGetCurrentItem(detachedRef) == nullptr);
    Require(WKBackForwardListGetBackListCount(detachedRef) == 0);
    Require(WKBackForwardListGetForwardListCount(detachedRef) == 0);
    Require(WKBackForwardListGetItemAtIndex(detachedRef, 1) == nullptr);
    WKBackForwardListClear(detachedRef);
    Require(NKWebKit::Cast<NKWebKit::BackForwardList>(detachedRef, "test")->entries.size() == 5);
    WKRelease(detachedRef);

    auto* empty = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::BackForwardList>());
    Require(WKBackForwardListGetCurrentItem(empty) == nullptr);
    Require(WKBackForwardListGetBackListCount(empty) == 0);
    WKRelease(empty);

    auto* bare = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::BackForwardListItem>());
    Require(WKBackForwardListItemCopyURL(bare) == nullptr);
    Require(WKBackForwardListItemCopyOriginalURL(bare) == nullptr);
    auto* bareTitle = WKBackForwardListItemCopyTitle(bare);
    Require(WKStringIsEmpty(bareTitle));
    WKRelease(bareTitle);
    WKRelease(bare);

    auto* notList = NKWebKit::ToRef(NKWebKit::String::FromUtf8("not a list"));
    Require(Throws(WKBackForwardListGetCurrentItem, notList));
    Require(Throws(WKBackForwardListItemCopyURL, notList));
    Require(Throws(WKBackForwardListGetCurrentItem, nullptr));
    Require(Throws(WKBackForwardListItemCopyTitle, nullptr));
    WKRelease(notList);
}

static void TestBundleScriptWorld() {
    Require(WKBundleScriptWorldGetTypeID() == 127);
    const void* normal = WKBundleScriptWorldNormalWorld();
    Require(normal != nullptr);
    Require(WKGetTypeID(normal) == WKBundleScriptWorldGetTypeID());
    auto* normalName = WKBundleScriptWorldCopyName(normal);
    Require(normalName != nullptr);
    Require(WKStringIsEqualToUTF8CString(normalName, ""));
    WKRelease(normalName);

    auto world = NKWebKit::Make<NKWebKit::BundleScriptWorld>();
    const void* ref = world.Get();
    auto* unsetWorldName = WKBundleScriptWorldCopyName(ref);
    Require(unsetWorldName != nullptr);
    Require(WKStringIsEqualToUTF8CString(unsetWorldName, ""));
    WKRelease(unsetWorldName);

    world->name = NKWebKit::String::FromUtf8("CustomWorld");
    auto* name = WKBundleScriptWorldCopyName(ref);
    Require(name != nullptr);
    Require(WKStringIsEqualToUTF8CString(name, "CustomWorld"));
    WKRelease(name);

    bool threw = false;
    try {
        WKBundleScriptWorldCopyName(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestBundleDOMWindowExtension() {
    Require(WKBundleDOMWindowExtensionGetTypeID() == 118);
    auto frame = NKWebKit::Make<NKWebKit::BundleFrame>();
    auto world = NKWebKit::Make<NKWebKit::BundleScriptWorld>();
    world->name = NKWebKit::String::FromUtf8("CustomExtensionWorld");

    auto* ext = WKBundleDOMWindowExtensionCreate(frame.Get(), world.Get());
    Require(ext != nullptr);
    Require(WKGetTypeID(ext) == WKBundleDOMWindowExtensionGetTypeID());
    Require(WKBundleDOMWindowExtensionGetFrame(ext) == frame.Get());
    Require(WKBundleDOMWindowExtensionGetScriptWorld(ext) == world.Get());
    WKRelease(ext);

    auto* emptyExt = WKBundleDOMWindowExtensionCreate(nullptr, nullptr);
    Require(emptyExt != nullptr);
    Require(WKGetTypeID(emptyExt) == WKBundleDOMWindowExtensionGetTypeID());
    Require(WKBundleDOMWindowExtensionGetFrame(emptyExt) == nullptr);
    Require(WKBundleDOMWindowExtensionGetScriptWorld(emptyExt) == nullptr);
    WKRelease(emptyExt);

    bool threw = false;
    try {
        WKBundleDOMWindowExtensionGetFrame(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKBundleDOMWindowExtensionGetScriptWorld(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestBundleNodeHandle() {
    Require(WKBundleNodeHandleGetTypeID() == 122);
    auto handle = NKWebKit::Make<NKWebKit::BundleNodeHandle>();
    const void* ref = handle.Get();
    Require(WKBundleNodeHandleCopyDocument(ref) == nullptr);
    Require(WKBundleNodeHandleCopyDocumentFrame(ref) == nullptr);
    Require(WKBundleNodeHandleCopyHTMLIFrameElementContentFrame(ref) == nullptr);

    auto doc = NKWebKit::Make<NKWebKit::BundleNodeHandle>();
    auto docFrame = NKWebKit::Make<NKWebKit::BundleFrame>();
    auto iframeContent = NKWebKit::Make<NKWebKit::BundleFrame>();

    handle->document = doc;
    handle->documentFrame = docFrame;
    handle->htmlIFrameElementContentFrame = iframeContent;

    auto* copiedDoc = WKBundleNodeHandleCopyDocument(ref);
    Require(copiedDoc == doc.Get());
    WKRelease(copiedDoc);

    auto* copiedDocFrame = WKBundleNodeHandleCopyDocumentFrame(ref);
    Require(copiedDocFrame == docFrame.Get());
    WKRelease(copiedDocFrame);

    auto* copiedIframe = WKBundleNodeHandleCopyHTMLIFrameElementContentFrame(ref);
    Require(copiedIframe == iframeContent.Get());
    WKRelease(copiedIframe);

    bool threw = false;
    try {
        WKBundleNodeHandleCopyDocument(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
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
    auto* unsetSuggestedFilename = WKBundleHitTestResultCopyLinkSuggestedFilename(ref);
    Require(unsetSuggestedFilename != nullptr);
    Require(WKStringIsEqualToUTF8CString(unsetSuggestedFilename, ""));
    WKRelease(unsetSuggestedFilename);
    Require(!WKBundleHitTestResultGetIsSelected(ref));
    Require(WKBundleHitTestResultGetMediaType(ref) == 0);
    Require(!WKBundleHitTestResultIsDownloadableMedia(ref));
    Require(!WKBundleHitTestResultMediaHasAudio(ref));
    Require(!WKBundleHitTestResultMediaIsInFullscreen(ref));

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

    hit->linkSuggestedFilename = NKWebKit::String::FromUtf8("example_file.pdf");
    hit->isSelected = true;
    hit->mediaType = 1;
    hit->isDownloadableMedia = true;
    hit->mediaHasAudio = true;
    hit->mediaIsInFullscreen = true;

    auto* suggestedFilename = WKBundleHitTestResultCopyLinkSuggestedFilename(ref);
    Require(suggestedFilename != nullptr);
    Require(WKStringIsEqualToUTF8CString(suggestedFilename, "example_file.pdf"));
    WKRelease(suggestedFilename);
    Require(WKBundleHitTestResultGetIsSelected(ref));
    Require(WKBundleHitTestResultGetMediaType(ref) == 1);
    Require(WKBundleHitTestResultIsDownloadableMedia(ref));
    Require(WKBundleHitTestResultMediaHasAudio(ref));
    Require(WKBundleHitTestResultMediaIsInFullscreen(ref));

    bool threw = false;
    try {
        WKBundleHitTestResultCopyLinkLabel(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKBundleHitTestResultCopyLinkSuggestedFilename(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKBundleHitTestResultGetIsSelected(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKBundleHitTestResultGetMediaType(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKBundleHitTestResultIsDownloadableMedia(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKBundleHitTestResultMediaHasAudio(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKBundleHitTestResultMediaIsInFullscreen(nullptr);
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

    Require(!download->isCancelled);
    WKDownloadCancel(ref);
    Require(download->isCancelled);

    threw = false;
    try {
        WKDownloadCancel(nullptr);
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

static const void* MakeProtectionSpace(const char* host, int port, NKWebKit::ProtectionSpaceServerType serverType, const char* realm, NKWebKit::ProtectionSpaceAuthenticationScheme scheme) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProtectionSpace>(NKWebKit::String::FromUtf8(host), port, serverType, NKWebKit::String::FromUtf8(realm), scheme));
}

static void TestProtectionSpace() {
    using Server = NKWebKit::ProtectionSpaceServerType;
    using Scheme = NKWebKit::ProtectionSpaceAuthenticationScheme;

    auto* space = MakeProtectionSpace("example.com", 8443, Server::HTTPS, "Private Area", Scheme::HTTPBasic);
    Require(WKGetTypeID(space) == WKProtectionSpaceGetTypeID());
    auto* host = WKProtectionSpaceCopyHost(space);
    Require(WKStringIsEqualToUTF8CString(host, "example.com"));
    WKRelease(host);
    auto* realm = WKProtectionSpaceCopyRealm(space);
    Require(WKStringIsEqualToUTF8CString(realm, "Private Area"));
    WKRelease(realm);
    Require(WKProtectionSpaceGetPort(space) == 8443);
    Require(WKProtectionSpaceGetServerType(space) == 2);
    Require(WKProtectionSpaceGetAuthenticationScheme(space) == 2);
    Require(!WKProtectionSpaceGetIsProxy(space));
    Require(WKProtectionSpaceGetReceivesCredentialSecurely(space));
    WKRelease(space);

    const Server servers[] = {Server::HTTP, Server::HTTPS, Server::FTP, Server::FTPS, Server::ProxyHTTP, Server::ProxyHTTPS, Server::ProxyFTP, Server::ProxySOCKS};
    const bool proxies[] = {false, false, false, false, true, true, true, true};
    const bool secure[] = {false, true, false, true, false, true, false, false};
    for (int index = 0; index < 8; ++index) {
        auto* typed = MakeProtectionSpace("h", 1, servers[index], "r", Scheme::Default);
        Require(WKProtectionSpaceGetServerType(typed) == static_cast<std::uint32_t>(index + 1));
        Require(WKProtectionSpaceGetIsProxy(typed) == proxies[index]);
        Require(WKProtectionSpaceGetReceivesCredentialSecurely(typed) == secure[index]);
        WKRelease(typed);
    }

    const Scheme schemes[] = {Scheme::Default, Scheme::HTTPBasic, Scheme::HTTPDigest, Scheme::HTMLForm, Scheme::NTLM, Scheme::Negotiate, Scheme::ClientCertificateRequested, Scheme::ServerTrustEvaluationRequested, Scheme::OAuth};
    for (int index = 0; index < 9; ++index) {
        auto* typed = MakeProtectionSpace("h", 1, Server::HTTP, "r", schemes[index]);
        Require(WKProtectionSpaceGetAuthenticationScheme(typed) == static_cast<std::uint32_t>(index + 1));
        Require(WKProtectionSpaceGetReceivesCredentialSecurely(typed) == (schemes[index] == Scheme::HTTPDigest));
        WKRelease(typed);
    }

    auto* unknown = MakeProtectionSpace("h", 1, Server::HTTP, "r", Scheme::Unknown);
    Require(WKProtectionSpaceGetAuthenticationScheme(unknown) == 100);
    WKRelease(unknown);
    auto* outOfRange = MakeProtectionSpace("h", 1, static_cast<Server>(0), "r", static_cast<Scheme>(42));
    Require(WKProtectionSpaceGetServerType(outOfRange) == 1);
    Require(WKProtectionSpaceGetAuthenticationScheme(outOfRange) == 100);
    WKRelease(outOfRange);

    auto* empty = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::ProtectionSpace>());
    auto* emptyHost = WKProtectionSpaceCopyHost(empty);
    Require(WKStringIsEmpty(emptyHost));
    WKRelease(emptyHost);
    auto* emptyRealm = WKProtectionSpaceCopyRealm(empty);
    Require(WKStringIsEmpty(emptyRealm));
    WKRelease(emptyRealm);
    Require(WKProtectionSpaceGetPort(empty) == 0);
    Require(WKProtectionSpaceGetServerType(empty) == 1);
    Require(WKProtectionSpaceGetAuthenticationScheme(empty) == 1);
    Require(!WKProtectionSpaceGetIsProxy(empty));
    Require(!WKProtectionSpaceGetReceivesCredentialSecurely(empty));
    WKRelease(empty);

    auto* notSpace = NKWebKit::ToRef(NKWebKit::String::FromUtf8("not a protection space"));
    bool threw = false;
    try {
        WKProtectionSpaceGetPort(notSpace);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    WKRelease(notSpace);

    threw = false;
    try {
        WKProtectionSpaceCopyHost(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestPermissionRequests() {
    Require(WKGeolocationPermissionRequestGetTypeID() == 61);
    Require(WKNotificationPermissionRequestGetTypeID() == 78);

    int geoCalls = 0;
    bool geoDecision = false;
    auto geoAllow = NKWebKit::Make<NKWebKit::GeolocationPermissionRequest>([&](bool res) {
        geoCalls++;
        geoDecision = res;
    });
    Require(geoAllow->state == NKWebKit::GeolocationPermissionRequest::State::Pending);
    WKGeolocationPermissionRequestAllow(geoAllow.Get());
    Require(geoAllow->state == NKWebKit::GeolocationPermissionRequest::State::Allowed);
    Require(geoCalls == 1);
    Require(geoDecision);

    WKGeolocationPermissionRequestAllow(geoAllow.Get());
    Require(geoCalls == 1);

    geoCalls = 0;
    auto geoDeny = NKWebKit::Make<NKWebKit::GeolocationPermissionRequest>([&](bool res) {
        geoCalls++;
        geoDecision = res;
    });
    WKGeolocationPermissionRequestDeny(geoDeny.Get());
    Require(geoDeny->state == NKWebKit::GeolocationPermissionRequest::State::Denied);
    Require(geoCalls == 1);
    Require(!geoDecision);

    int notifCalls = 0;
    bool notifDecision = false;
    auto notifAllow = NKWebKit::Make<NKWebKit::NotificationPermissionRequest>([&](bool res) {
        notifCalls++;
        notifDecision = res;
    });
    Require(notifAllow->state == NKWebKit::NotificationPermissionRequest::State::Pending);
    WKNotificationPermissionRequestAllow(notifAllow.Get());
    Require(notifAllow->state == NKWebKit::NotificationPermissionRequest::State::Allowed);
    Require(notifCalls == 1);
    Require(notifDecision);

    WKNotificationPermissionRequestAllow(notifAllow.Get());
    Require(notifCalls == 1);

    notifCalls = 0;
    auto notifDeny = NKWebKit::Make<NKWebKit::NotificationPermissionRequest>([&](bool res) {
        notifCalls++;
        notifDecision = res;
    });
    WKNotificationPermissionRequestDeny(notifDeny.Get());
    Require(notifDeny->state == NKWebKit::NotificationPermissionRequest::State::Denied);
    Require(notifCalls == 1);
    Require(!notifDecision);

    bool threw = false;
    try {
        WKGeolocationPermissionRequestAllow(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKGeolocationPermissionRequestDeny(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKNotificationPermissionRequestAllow(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKNotificationPermissionRequestDeny(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    auto dummy = NKWebKit::Make<NKWebKit::URL>("https://example.com");
    threw = false;
    try {
        WKGeolocationPermissionRequestAllow(dummy.Get());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKNotificationPermissionRequestAllow(dummy.Get());
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

static void TestFrameHandle() {
    auto* handle = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::FrameHandle>(0x0123456789ABCDEFull));
    Require(WKGetTypeID(handle) == WKFrameHandleGetTypeID());
    Require(WKFrameHandleGetFrameID(handle) == 0x0123456789ABCDEFull);
    WKRelease(handle);

    auto* zero = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::FrameHandle>());
    Require(WKFrameHandleGetFrameID(zero) == 0);
    WKRelease(zero);

    auto* maximum = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::FrameHandle>(UINT64_MAX));
    Require(WKFrameHandleGetFrameID(maximum) == UINT64_MAX);
    WKRelease(maximum);

    auto* notHandle = NKWebKit::ToRef(NKWebKit::String::FromUtf8("not a frame handle"));
    bool threw = false;
    try {
        WKFrameHandleGetFrameID(notHandle);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    WKRelease(notHandle);

    threw = false;
    try {
        WKFrameHandleGetFrameID(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

template<typename Function>
static bool ThrowsOpenPanel(Function function, const void* ref) {
    try {
        function(ref);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

template<typename Function>
static bool ThrowsOpenPanelCopy(Function function, const void* ref) {
    try {
        function(ref);
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static void TestOpenPanelParameters() {
    using Strings = std::vector<NKWebKit::Retained<NKWebKit::String>>;
    auto* parameters = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::OpenPanelParameters>(
        true, true,
        Strings{NKWebKit::String::FromUtf8("Image/PNG"), NKWebKit::String::FromUtf8("image/png"), NKWebKit::String::FromUtf8("TEXT/*")},
        Strings{NKWebKit::String::FromUtf8(".txt"), NKWebKit::String::FromUtf8("PNG"), NKWebKit::String::FromUtf8(".unknown")},
        Strings{NKWebKit::String::FromUtf8("/tmp/a.txt"), NKWebKit::String::FromUtf8("/tmp/b.txt"), NKWebKit::String::FromUtf8("/tmp/c.txt")}));
    Require(WKGetTypeID(parameters) == WKOpenPanelParametersGetTypeID());
    Require(WKOpenPanelParametersGetAllowsDirectories(parameters));
    Require(WKOpenPanelParametersGetAllowsMultipleFiles(parameters));

    auto* mimeTypes = WKOpenPanelParametersCopyAcceptedMIMETypes(parameters);
    Require(WKGetTypeID(mimeTypes) == WKArrayGetTypeID());
    Require(WKArrayGetSize(mimeTypes) == 3);
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(mimeTypes, 0), "Image/PNG"));
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(mimeTypes, 1), "image/png"));
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(mimeTypes, 2), "TEXT/*"));
    WKRelease(mimeTypes);

    auto* allowedMime = WKOpenPanelParametersCopyAllowedMIMETypes(parameters);
    Require(WKGetTypeID(allowedMime) == WKArrayGetTypeID());
    Require(WKArrayGetSize(allowedMime) == 3);
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(allowedMime, 0), "image/png"));
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(allowedMime, 1), "text/*"));
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(allowedMime, 2), "text/plain"));
    WKRelease(allowedMime);

    auto* extensions = WKOpenPanelParametersCopyAcceptedFileExtensions(parameters);
    Require(WKArrayGetSize(extensions) == 3);
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(extensions, 0), ".txt"));
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(extensions, 1), "PNG"));
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(extensions, 2), ".unknown"));
    WKRelease(extensions);

    auto* selected = WKOpenPanelParametersCopySelectedFileNames(parameters);
    Require(WKArrayGetSize(selected) == 3);
    Require(WKStringIsEqualToUTF8CString(WKArrayGetItemAtIndex(selected, 2), "/tmp/c.txt"));
    WKRelease(selected);
    WKRelease(parameters);

    auto* empty = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::OpenPanelParameters>());
    Require(!WKOpenPanelParametersGetAllowsDirectories(empty));
    Require(!WKOpenPanelParametersGetAllowsMultipleFiles(empty));
    auto* none = WKOpenPanelParametersCopyAcceptedMIMETypes(empty);
    Require(WKArrayGetSize(none) == 0);
    WKRelease(none);
    none = WKOpenPanelParametersCopyAllowedMIMETypes(empty);
    Require(WKArrayGetSize(none) == 0);
    WKRelease(none);
    none = WKOpenPanelParametersCopyAcceptedFileExtensions(empty);
    Require(WKArrayGetSize(none) == 0);
    WKRelease(none);
    none = WKOpenPanelParametersCopySelectedFileNames(empty);
    Require(WKArrayGetSize(none) == 0);
    WKRelease(none);
    WKRelease(empty);

    auto* withNull = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::OpenPanelParameters>(false, false, Strings{nullptr}, Strings{}, Strings{}));
    auto* nullTypes = WKOpenPanelParametersCopyAcceptedMIMETypes(withNull);
    Require(WKArrayGetSize(nullTypes) == 1);
    Require(WKStringIsEmpty(WKArrayGetItemAtIndex(nullTypes, 0)));
    WKRelease(nullTypes);
    WKRelease(withNull);

    auto* notParameters = NKWebKit::ToRef(NKWebKit::String::FromUtf8("not parameters"));
    Require(ThrowsOpenPanel(WKOpenPanelParametersGetAllowsDirectories, notParameters));
    Require(ThrowsOpenPanel(WKOpenPanelParametersGetAllowsMultipleFiles, nullptr));
    Require(ThrowsOpenPanelCopy(WKOpenPanelParametersCopyAcceptedMIMETypes, notParameters));
    Require(ThrowsOpenPanelCopy(WKOpenPanelParametersCopyAllowedMIMETypes, notParameters));
    Require(ThrowsOpenPanelCopy(WKOpenPanelParametersCopyAllowedMIMETypes, nullptr));
    Require(ThrowsOpenPanelCopy(WKOpenPanelParametersCopyAcceptedFileExtensions, nullptr));
    Require(ThrowsOpenPanelCopy(WKOpenPanelParametersCopySelectedFileNames, nullptr));
    WKRelease(notParameters);
}

static void TestNotification() {
    Require(WKNotificationGetTypeID() == 76);

    auto notif = NKWebKit::Make<NKWebKit::Notification>();
    notif->title = u"Alert";
    notif->body = u"Something happened";
    notif->iconURL = u"https://example.com/icon.png";
    notif->tag = u"tag1";
    notif->lang = u"en-US";
    notif->dir = u"ltr";
    notif->notificationID = 42;

    const void* ref = NKWebKit::ToRef(notif);

    auto* title = WKNotificationCopyTitle(ref);
    Require(WKStringIsEqualToUTF8CString(title, "Alert"));
    WKRelease(title);

    auto* body = WKNotificationCopyBody(ref);
    Require(WKStringIsEqualToUTF8CString(body, "Something happened"));
    WKRelease(body);

    auto* icon = WKNotificationCopyIconURL(ref);
    Require(WKGetTypeID(icon) == 21);
    Require(WKStringIsEqualToUTF8CString(icon, "https://example.com/icon.png"));
    WKRelease(icon);

    auto* tag = WKNotificationCopyTag(ref);
    Require(WKStringIsEqualToUTF8CString(tag, "tag1"));
    WKRelease(tag);

    auto* lang = WKNotificationCopyLang(ref);
    Require(WKStringIsEqualToUTF8CString(lang, "en-US"));
    WKRelease(lang);

    auto* dir = WKNotificationCopyDir(ref);
    Require(WKStringIsEqualToUTF8CString(dir, "ltr"));
    WKRelease(dir);

    Require(WKNotificationGetID(ref) == 42);

    bool threw = false;
    try {
        WKNotificationCopyTitle(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    WKRelease(ref);
}

static void TestCertificateInfo() {
    Require(WKCertificateInfoGetTypeID() == 0);
    Require(WKCertificateInfoCreateWithCertficateChain(nullptr) == nullptr);
    Require(WKCertificateInfoGetVerificationError(nullptr) == 0);
    Require(WKCertificateInfoCopyVerificationErrorDescription(nullptr) == nullptr);
    Require(WKCertificateInfoGetCertificateChainSize(nullptr) == 0);
    Require(WKCertificateInfoCopyCertificateAtIndex(nullptr, 0) == nullptr);
}

static void TestEmptyFrameLoaderClient() {
    _ZN7WebCore22EmptyFrameLoaderClient35dispatchDidChangeLocationWithinPageEv();
    _ZN7WebCore22EmptyFrameLoaderClient14didChangeTitleEPNS_14DocumentLoaderE();
    _ZN7WebCore22EmptyFrameLoaderClient15finishedLoadingEPNS_14DocumentLoaderE();
    _ZN7WebCore22EmptyFrameLoaderClient15willChangeTitleEPNS_14DocumentLoaderE();
    _ZN7WebCore22EmptyFrameLoaderClient20setMainDocumentErrorEPNS_14DocumentLoaderERKNS_13ResourceErrorE();
    _ZN7WebCore22EmptyFrameLoaderClient24revertToProvisionalStateEPNS_14DocumentLoaderE();
    _ZN7WebCore22EmptyFrameLoaderClient19saveViewStateToItemERNS_11HistoryItemE();
    _ZN7WebCore22EmptyFrameLoaderClient20redirectDataToPluginERNS_6WidgetE();
    _ZN7WebCore22EmptyFrameLoaderClient21didRunInsecureContentERNS_14SecurityOriginERKN3WTF3URLE();
    _ZN7WebCore22EmptyFrameLoaderClient23dispatchDidReceiveTitleERKNS_19StringWithDirectionE();
    _ZN7WebCore22EmptyFrameLoaderClient36transitionToCommittedFromCachedFrameEPNS_11CachedFrameE();
}

template <typename Function>
static bool HasNamedLoadLifecycleThrow(Function function, const char* name) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find(name) == std::string::npos) {
            throw std::runtime_error(std::string("UNEXPECTED_LOAD_LIFECYCLE_THROW: ") + error.what());
        }
        std::fprintf(stderr, "EXPECTED_LOAD_LIFECYCLE_THROW %s\n", name);
        return true;
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("UNEXPECTED_LOAD_LIFECYCLE_EXCEPTION: ") + error.what());
    } catch (...) {
        throw std::runtime_error("UNEXPECTED_LOAD_LIFECYCLE_EXCEPTION: non-standard exception");
    }
    return false;
}

static void TestEmptyFrameLoaderLifecycleCallbacks() {
#define CheckLoadLifecycleCallback(function) HasNamedLoadLifecycleThrow([] { function(); }, #function)
    const bool willSendRequestThrew = CheckLoadLifecycleCallback(_ZN7WebCore22EmptyFrameLoaderClient23dispatchWillSendRequestEPNS_14DocumentLoaderEmRNS_15ResourceRequestERKNS_16ResourceResponseE);
    const bool authenticationChallengeThrew = CheckLoadLifecycleCallback(_ZN7WebCore22EmptyFrameLoaderClient41dispatchDidReceiveAuthenticationChallengeEPNS_14DocumentLoaderEmRKNS_23AuthenticationChallengeE);
    const bool receiveResponseThrew = CheckLoadLifecycleCallback(_ZN7WebCore22EmptyFrameLoaderClient26dispatchDidReceiveResponseEPNS_14DocumentLoaderEmRKNS_16ResourceResponseE);
    const bool receiveContentLengthThrew = CheckLoadLifecycleCallback(_ZN7WebCore22EmptyFrameLoaderClient31dispatchDidReceiveContentLengthEPNS_14DocumentLoaderEmi);
    const bool finishLoadingThrew = CheckLoadLifecycleCallback(_ZN7WebCore22EmptyFrameLoaderClient24dispatchDidFinishLoadingEPNS_14DocumentLoaderEm);
    const bool failLoadingThrew = CheckLoadLifecycleCallback(_ZN7WebCore22EmptyFrameLoaderClient22dispatchDidFailLoadingEPNS_14DocumentLoaderEmRKNS_13ResourceErrorE);
#undef CheckLoadLifecycleCallback
    if (willSendRequestThrew || authenticationChallengeThrew || receiveResponseThrew || receiveContentLengthThrew || finishLoadingThrew || failLoadingThrew) {
        throw std::runtime_error("EXPECTED_LOAD_LIFECYCLE_RED: one or more callbacks still throw");
    }
}

static void TestSessionState() {
    Require(WKSessionStateCreateFromData(nullptr) == nullptr);
    Require(WKSessionStateCopyData(nullptr) == nullptr);

    const unsigned char raw[] = { 0x12, 0x34, 0x56, 0x78 };
    auto* d = WKDataCreate(raw, sizeof(raw));
    auto* sessionState = WKSessionStateCreateFromData(d);
    Require(sessionState != nullptr);
    Require(WKGetTypeID(sessionState) == WKSessionStateGetTypeID());

    auto* copiedData = WKSessionStateCopyData(sessionState);
    Require(copiedData != nullptr);
    Require(WKGetTypeID(copiedData) == WKDataGetTypeID());
    Require(WKDataGetSize(copiedData) == sizeof(raw));
    Require(std::memcmp(WKDataGetBytes(copiedData), raw, sizeof(raw)) == 0);

    WKRetain(sessionState);
    WKRelease(sessionState);

    WKRelease(copiedData);
    WKRelease(sessionState);
    WKRelease(d);
}

static void TestInspector() {
    Require(WKInspectorGetTypeID() == 67);
    auto inspector = NKWebKit::Make<NKWebKit::Inspector>();
    const void* ref = inspector.Get();

    Require(!WKInspectorIsAttached(ref));
    Require(!WKInspectorIsConnected(ref));
    Require(!WKInspectorIsElementSelectionActive(ref));
    Require(!WKInspectorIsFront(ref));
    Require(!WKInspectorIsProfilingPage(ref));
    Require(!WKInspectorIsVisible(ref));

    inspector->isAttached = true;
    Require(WKInspectorIsAttached(ref));
    Require(!WKInspectorIsConnected(ref));
    Require(!WKInspectorIsElementSelectionActive(ref));
    Require(!WKInspectorIsFront(ref));
    Require(!WKInspectorIsProfilingPage(ref));
    Require(!WKInspectorIsVisible(ref));

    inspector->isAttached = false;
    inspector->isConnected = true;
    Require(!WKInspectorIsAttached(ref));
    Require(WKInspectorIsConnected(ref));
    Require(!WKInspectorIsElementSelectionActive(ref));
    Require(!WKInspectorIsFront(ref));
    Require(!WKInspectorIsProfilingPage(ref));
    Require(!WKInspectorIsVisible(ref));

    inspector->isConnected = false;
    inspector->isElementSelectionActive = true;
    Require(!WKInspectorIsAttached(ref));
    Require(!WKInspectorIsConnected(ref));
    Require(WKInspectorIsElementSelectionActive(ref));
    Require(!WKInspectorIsFront(ref));
    Require(!WKInspectorIsProfilingPage(ref));
    Require(!WKInspectorIsVisible(ref));

    inspector->isElementSelectionActive = false;
    inspector->isFront = true;
    Require(!WKInspectorIsAttached(ref));
    Require(!WKInspectorIsConnected(ref));
    Require(!WKInspectorIsElementSelectionActive(ref));
    Require(WKInspectorIsFront(ref));
    Require(!WKInspectorIsProfilingPage(ref));
    Require(!WKInspectorIsVisible(ref));

    inspector->isFront = false;
    inspector->isProfilingPage = true;
    Require(!WKInspectorIsAttached(ref));
    Require(!WKInspectorIsConnected(ref));
    Require(!WKInspectorIsElementSelectionActive(ref));
    Require(!WKInspectorIsFront(ref));
    Require(WKInspectorIsProfilingPage(ref));
    Require(!WKInspectorIsVisible(ref));

    inspector->isProfilingPage = false;
    inspector->isVisible = true;
    Require(!WKInspectorIsAttached(ref));
    Require(!WKInspectorIsConnected(ref));
    Require(!WKInspectorIsElementSelectionActive(ref));
    Require(!WKInspectorIsFront(ref));
    Require(!WKInspectorIsProfilingPage(ref));
    Require(WKInspectorIsVisible(ref));

    WKInspectorAttach(ref);
    Require(WKInspectorIsAttached(ref));
    WKInspectorDetach(ref);
    Require(!WKInspectorIsAttached(ref));

    WKInspectorConnect(ref);
    Require(WKInspectorIsConnected(ref));

    WKInspectorShow(ref);
    Require(WKInspectorIsVisible(ref));
    Require(WKInspectorIsFront(ref));
    WKInspectorHide(ref);
    Require(!WKInspectorIsVisible(ref));
    Require(!WKInspectorIsFront(ref));

    WKInspectorShowConsole(ref);
    Require(WKInspectorIsVisible(ref));
    Require(WKInspectorIsFront(ref));
    WKInspectorHide(ref);
    Require(!WKInspectorIsVisible(ref));
    Require(!WKInspectorIsFront(ref));

    WKInspectorShowResources(ref);
    Require(WKInspectorIsVisible(ref));
    Require(WKInspectorIsFront(ref));

    Require(!WKInspectorIsElementSelectionActive(ref));
    WKInspectorToggleElementSelection(ref);
    Require(WKInspectorIsElementSelectionActive(ref));
    WKInspectorToggleElementSelection(ref);
    Require(!WKInspectorIsElementSelectionActive(ref));

    Require(!WKInspectorIsProfilingPage(ref));
    WKInspectorTogglePageProfiling(ref);
    Require(WKInspectorIsProfilingPage(ref));
    WKInspectorTogglePageProfiling(ref);
    Require(!WKInspectorIsProfilingPage(ref));

    WKInspectorAttach(ref);
    WKInspectorConnect(ref);
    WKInspectorShow(ref);
    WKInspectorToggleElementSelection(ref);
    WKInspectorTogglePageProfiling(ref);
    Require(WKInspectorIsAttached(ref));
    Require(WKInspectorIsConnected(ref));
    Require(WKInspectorIsVisible(ref));
    Require(WKInspectorIsFront(ref));
    Require(WKInspectorIsElementSelectionActive(ref));
    Require(WKInspectorIsProfilingPage(ref));

    WKInspectorClose(ref);
    Require(!WKInspectorIsAttached(ref));
    Require(!WKInspectorIsConnected(ref));
    Require(!WKInspectorIsVisible(ref));
    Require(!WKInspectorIsFront(ref));
    Require(!WKInspectorIsElementSelectionActive(ref));
    Require(!WKInspectorIsProfilingPage(ref));

    bool threw = false;
    try {
        WKInspectorIsAttached(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorAttach(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorClose(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorConnect(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorDetach(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorHide(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorShow(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorShowConsole(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorShowResources(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorToggleElementSelection(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKInspectorTogglePageProfiling(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}
static void TestFrame() {
    Require(WKFrameGetTypeID() == 55);

    auto frame = NKWebKit::Make<NKWebKit::Frame>();
    const void* ref = frame.Get();

    Require(!WKFrameCanProvideSource(ref));
    Require(!WKFrameIsDisplayingMarkupDocument(ref));
    Require(!WKFrameIsDisplayingStandaloneImageDocument(ref));
    Require(!WKFrameIsMainFrame(ref));

    frame->isDisplayingMarkupDocument = true;
    Require(WKFrameCanProvideSource(ref));
    Require(WKFrameIsDisplayingMarkupDocument(ref));
    Require(!WKFrameIsDisplayingStandaloneImageDocument(ref));
    Require(!WKFrameIsMainFrame(ref));

    frame->isDisplayingMarkupDocument = false;
    frame->isDisplayingStandaloneImageDocument = true;
    Require(!WKFrameCanProvideSource(ref));
    Require(!WKFrameIsDisplayingMarkupDocument(ref));
    Require(WKFrameIsDisplayingStandaloneImageDocument(ref));
    Require(!WKFrameIsMainFrame(ref));

    frame->isDisplayingStandaloneImageDocument = false;
    frame->isMainFrame = true;
    Require(!WKFrameCanProvideSource(ref));
    Require(!WKFrameIsDisplayingMarkupDocument(ref));
    Require(!WKFrameIsDisplayingStandaloneImageDocument(ref));
    Require(WKFrameIsMainFrame(ref));

    bool caught = false;
    try {
        WKFrameIsMainFrame(nullptr);
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught);
}

static void TestImage() {
    Require(WKImageGetTypeID() == 12);
    NKWebKit::SizeValue sz{640.0, 480.0};
    auto* img = WKImageCreate(sz, 0);
    Require(img != nullptr);
    Require(WKGetTypeID(img) == WKImageGetTypeID());
    auto outSz = WKImageGetSize(img);
    Require(outSz.width == 640.0);
    Require(outSz.height == 480.0);
    WKRelease(img);

    NKWebKit::SizeValue fracSz{640.7, 480.2};
    auto* fracImg = WKImageCreate(fracSz, 0);
    Require(fracImg != nullptr);
    auto fracOut = WKImageGetSize(fracImg);
    Require(fracOut.width == 640.0);
    Require(fracOut.height == 480.0);
    WKRelease(fracImg);

    bool threw = false;
    try {
        WKImageGetSize(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    bool threwNan = false;
    try {
        WKImageCreate(NKWebKit::SizeValue{std::numeric_limits<double>::quiet_NaN(), 100.0}, 0);
    } catch (const std::invalid_argument&) {
        threwNan = true;
    }
    Require(threwNan);

    bool threwOutOfRange = false;
    try {
        WKImageCreate(NKWebKit::SizeValue{1e15, 100.0}, 0);
    } catch (const std::invalid_argument&) {
        threwOutOfRange = true;
    }
    Require(threwOutOfRange);
}

static int s_runLoopCallCount = 0;
static void* s_runLoopReceivedContext = nullptr;

static void APS5_VABI RunLoopCallback1(void* ctx) {
    s_runLoopCallCount++;
    s_runLoopReceivedContext = ctx;
    WKRunLoopStopMain();
}

static void APS5_VABI RunLoopCallback2(void* ctx) {
    auto* count = static_cast<int*>(ctx);
    (*count) += 10;
    WKRunLoopStopMain();
}

static void TestRunLoop() {
    WKRunLoopInitializeMain();
    WKRunLoopStopMain();
    WKRunLoopRunMain();

    WKRunLoopInitializeMain();
    s_runLoopCallCount = 0;
    s_runLoopReceivedContext = nullptr;

    WKRunLoopCallOnMainThread(nullptr, nullptr);

    int testContext = 42;
    WKRunLoopCallOnMainThread(RunLoopCallback1, &testContext);

    WKRunLoopRunMain();

    Require(s_runLoopCallCount == 1);
    Require(s_runLoopReceivedContext == &testContext);

    WKRunLoopInitializeMain();

    std::thread backgroundThread([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        WKRunLoopCallOnMainThread(RunLoopCallback2, &s_runLoopCallCount);
    });

    WKRunLoopRunMain();
    backgroundThread.join();

    Require(s_runLoopCallCount == 11);
}

static void TestContext() {
    Require(WKContextGetTypeID() == 85);

    const auto* context = WKContextCreate();
    Require(WKGetTypeID(context) == WKContextGetTypeID());
    Require(!WKContextGetUsesSingleWebProcess(context));
    WKContextSetUsesSingleWebProcess(context, true);
    Require(WKContextGetUsesSingleWebProcess(context));
    WKRelease(context);

    const auto* config = WKContextConfigurationCreate();
    const auto* contextFromConfig = WKContextCreateWithConfiguration(config);
    Require(WKGetTypeID(contextFromConfig) == WKContextGetTypeID());
    Require(!WKContextGetUsesSingleWebProcess(contextFromConfig));
    WKRelease(contextFromConfig);
    WKRelease(config);

    const auto* nullContext = WKContextCreateWithConfiguration(nullptr);
    Require(WKGetTypeID(nullContext) == WKContextGetTypeID());
    WKRelease(nullContext);

    auto* pathStr = WKStringCreateWithUTF8CString("/app0/bundle.js");
    const auto* contextFromPath = WKContextCreateWithInjectedBundlePath(pathStr);
    Require(WKGetTypeID(contextFromPath) == WKContextGetTypeID());
    WKRelease(contextFromPath);
    WKRelease(pathStr);
}

static void TestSerializedScriptValue() {
    Require(WKSerializedScriptValueGetTypeID() == 20);
    Require(WKSerializedScriptValueCreate(nullptr, nullptr, nullptr) == nullptr);
    Require(WKSerializedScriptValueDeserialize(nullptr, nullptr, nullptr) == nullptr);

    auto* ctx = JSGlobalContextCreate(nullptr);
    Require(ctx != nullptr);
    Require(WKSerializedScriptValueCreate(ctx, nullptr, nullptr) == nullptr);

    auto* numVal = JSValueMakeNumber(ctx, 42.0);
    auto* serNum = WKSerializedScriptValueCreate(ctx, numVal, nullptr);
    Require(serNum != nullptr);
    Require(WKGetTypeID(serNum) == WKSerializedScriptValueGetTypeID());
    Require(WKSerializedScriptValueDeserialize(serNum, nullptr, nullptr) == nullptr);
    auto* deNum = WKSerializedScriptValueDeserialize(serNum, ctx, nullptr);
    Require(deNum != nullptr);
    Require(JSValueIsNumber(ctx, deNum));
    Require(JSValueToNumber(ctx, deNum, nullptr) == 42.0);

    auto* retained = WKRetain(serNum);
    Require(retained == serNum);
    WKRelease(retained);
    WKRelease(serNum);

    auto* nanVal = JSValueMakeNumber(ctx, std::numeric_limits<double>::quiet_NaN());
    auto* serNan = WKSerializedScriptValueCreate(ctx, nanVal, nullptr);
    Require(serNan != nullptr);
    auto* deNan = WKSerializedScriptValueDeserialize(serNan, ctx, nullptr);
    Require(deNan != nullptr && JSValueIsNumber(ctx, deNan) && std::isnan(JSValueToNumber(ctx, deNan, nullptr)));
    WKRelease(serNan);

    auto* infVal = JSValueMakeNumber(ctx, std::numeric_limits<double>::infinity());
    auto* serInf = WKSerializedScriptValueCreate(ctx, infVal, nullptr);
    Require(serInf != nullptr);
    auto* deInf = WKSerializedScriptValueDeserialize(serInf, ctx, nullptr);
    Require(deInf != nullptr && JSValueIsNumber(ctx, deInf) && std::isinf(JSValueToNumber(ctx, deInf, nullptr)) && JSValueToNumber(ctx, deInf, nullptr) > 0);
    WKRelease(serInf);

    auto* negInfVal = JSValueMakeNumber(ctx, -std::numeric_limits<double>::infinity());
    auto* serNegInf = WKSerializedScriptValueCreate(ctx, negInfVal, nullptr);
    Require(serNegInf != nullptr);
    auto* deNegInf = WKSerializedScriptValueDeserialize(serNegInf, ctx, nullptr);
    Require(deNegInf != nullptr && JSValueIsNumber(ctx, deNegInf) && std::isinf(JSValueToNumber(ctx, deNegInf, nullptr)) && JSValueToNumber(ctx, deNegInf, nullptr) < 0);
    WKRelease(serNegInf);

    auto* boolVal = JSValueMakeBoolean(ctx, true);
    auto* serBool = WKSerializedScriptValueCreate(ctx, boolVal, nullptr);
    Require(serBool != nullptr);
    auto* deBool = WKSerializedScriptValueDeserialize(serBool, ctx, nullptr);
    Require(deBool != nullptr && JSValueIsBoolean(ctx, deBool) && JSValueToBoolean(ctx, deBool));
    WKRelease(serBool);

    auto* nullVal = JSValueMakeNull(ctx);
    auto* serNull = WKSerializedScriptValueCreate(ctx, nullVal, nullptr);
    Require(serNull != nullptr);
    auto* deNull = WKSerializedScriptValueDeserialize(serNull, ctx, nullptr);
    Require(deNull != nullptr && JSValueIsNull(ctx, deNull));
    WKRelease(serNull);

    auto* undefVal = JSValueMakeUndefined(ctx);
    auto* serUndef = WKSerializedScriptValueCreate(ctx, undefVal, nullptr);
    Require(serUndef != nullptr);
    auto* deUndef = WKSerializedScriptValueDeserialize(serUndef, ctx, nullptr);
    Require(deUndef != nullptr && JSValueIsUndefined(ctx, deUndef));
    WKRelease(serUndef);

    auto* jsJsonStr = JSStringCreateWithUTF8CString_nid_postfix("\"hello AnyPS5\"");
    Require(jsJsonStr != nullptr);
    auto* strVal = JSValueMakeFromJSONString(ctx, jsJsonStr);
    JSStringRelease_nid_postfix(jsJsonStr);
    Require(strVal != nullptr);
    auto* serStr = WKSerializedScriptValueCreate(ctx, strVal, nullptr);
    Require(serStr != nullptr);

    auto* otherCtx = JSGlobalContextCreate(nullptr);
    Require(otherCtx != nullptr);
    auto* deStrOther = WKSerializedScriptValueDeserialize(serStr, otherCtx, nullptr);
    Require(deStrOther != nullptr && JSValueIsString(otherCtx, deStrOther));
    auto* outStr = JSValueToStringCopy(otherCtx, deStrOther, nullptr);
    Require(outStr != nullptr && JSStringIsEqualToUTF8CString(outStr, "hello AnyPS5"));
    JSStringRelease_nid_postfix(outStr);
    JSGlobalContextRelease(otherCtx);

    WKRelease(serStr);

    NKWebKit::JSValueRef exc = nullptr;
    auto* badSer = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SerializedScriptValue>("invalid json {{{", NKWebKit::SerializedScriptValueKind::Json));
    auto* deBad = WKSerializedScriptValueDeserialize(badSer, ctx, &exc);
    Require(deBad == nullptr);
    Require(exc != nullptr);
    WKRelease(badSer);

    JSGlobalContextRelease(ctx);
}

static void TestRenderTree() {
    auto requireThrows = [](auto&& fn) {
        bool caught = false;
        try {
            fn();
        } catch (const std::invalid_argument&) {
            caught = true;
        }
        Require(caught);
    };

    auto obj = NKWebKit::Make<NKWebKit::RenderObject>();
    Require(WKGetTypeID(obj.Get()) == WKRenderObjectGetTypeID());
    Require(WKRenderObjectCopyElementID(obj.Get()) == nullptr);
    Require(WKRenderObjectCopyElementTagName(obj.Get()) == nullptr);
    Require(WKRenderObjectCopyName(obj.Get()) == nullptr);
    Require(WKRenderObjectCopyTextSnippet(obj.Get()) == nullptr);
    Require(WKRenderObjectGetElementClassNames(obj.Get()) == nullptr);
    Require(WKRenderObjectGetChildren(obj.Get()) == nullptr);
    auto defaultObjRect = WKRenderObjectGetFrameRect(obj.Get());
    Require(defaultObjRect.origin.x == 0.0 && defaultObjRect.origin.y == 0.0);
    Require(defaultObjRect.size.width == 0.0 && defaultObjRect.size.height == 0.0);
    auto defaultObjPos = WKRenderObjectGetAbsolutePosition(obj.Get());
    Require(defaultObjPos.x == 0.0 && defaultObjPos.y == 0.0);
    Require(WKRenderObjectGetTextLength(obj.Get()) == 0);

    obj->elementID = NKWebKit::String::FromUtf8("main-content");
    obj->elementTagName = NKWebKit::String::FromUtf8("div");
    obj->name = NKWebKit::String::FromUtf8("RenderBlock");
    obj->textSnippet = NKWebKit::String::FromUtf8("Hello World");
    auto classes = NKWebKit::Make<NKWebKit::Array>();
    classes->elements.push_back(NKWebKit::String::FromUtf8("container"));
    obj->elementClassNames = classes;
    auto children = NKWebKit::Make<NKWebKit::Array>();
    auto childObj = NKWebKit::Make<NKWebKit::RenderObject>();
    children->elements.push_back(std::move(childObj));
    obj->children = children;
    obj->frameRect = {{10.0, 20.0}, {300.0, 400.0}};
    obj->absolutePosition = {10.0, 20.0};
    obj->textLength = 11;

    auto* idStr = WKRenderObjectCopyElementID(obj.Get());
    Require(idStr != nullptr);
    Require(WKGetTypeID(idStr) == WKStringGetTypeID());
    Require(static_cast<const NKWebKit::String*>(idStr)->Utf8() == "main-content");
    WKRelease(idStr);

    auto* tagStr = WKRenderObjectCopyElementTagName(obj.Get());
    Require(tagStr != nullptr);
    Require(static_cast<const NKWebKit::String*>(tagStr)->Utf8() == "div");
    WKRelease(tagStr);

    auto* nameStr = WKRenderObjectCopyName(obj.Get());
    Require(nameStr != nullptr);
    Require(static_cast<const NKWebKit::String*>(nameStr)->Utf8() == "RenderBlock");
    WKRelease(nameStr);

    auto* snippetStr = WKRenderObjectCopyTextSnippet(obj.Get());
    Require(snippetStr != nullptr);
    Require(static_cast<const NKWebKit::String*>(snippetStr)->Utf8() == "Hello World");
    WKRelease(snippetStr);

    Require(WKRenderObjectGetElementClassNames(obj.Get()) == classes.Get());
    Require(WKRenderObjectGetChildren(obj.Get()) == children.Get());
    auto objRect = WKRenderObjectGetFrameRect(obj.Get());
    Require(objRect.origin.x == 10.0 && objRect.origin.y == 20.0);
    Require(objRect.size.width == 300.0 && objRect.size.height == 400.0);
    auto objPos = WKRenderObjectGetAbsolutePosition(obj.Get());
    Require(objPos.x == 10.0 && objPos.y == 20.0);
    Require(WKRenderObjectGetTextLength(obj.Get()) == 11);

    auto layer = NKWebKit::Make<NKWebKit::RenderLayer>();
    Require(WKGetTypeID(layer.Get()) == WKRenderLayerGetTypeID());
    Require(WKRenderLayerGetRenderer(layer.Get()) == nullptr);
    Require(WKRenderLayerGetFrameContentsLayer(layer.Get()) == nullptr);
    Require(WKRenderLayerCopyRendererName(layer.Get()) == nullptr);
    Require(WKRenderLayerCopyElementTagName(layer.Get()) == nullptr);
    Require(WKRenderLayerCopyElementID(layer.Get()) == nullptr);
    Require(WKRenderLayerGetElementClassNames(layer.Get()) == nullptr);
    Require(WKRenderLayerGetNegativeZOrderList(layer.Get()) == nullptr);
    Require(WKRenderLayerGetPositiveZOrderList(layer.Get()) == nullptr);
    Require(WKRenderLayerGetNormalFlowList(layer.Get()) == nullptr);
    Require(!WKRenderLayerIsClipping(layer.Get()));
    Require(!WKRenderLayerIsClipped(layer.Get()));
    Require(!WKRenderLayerIsReflection(layer.Get()));
    Require(WKRenderLayerGetCompositingLayerType(layer.Get()) == 0);
    Require(WKRenderLayerGetBackingStoreMemoryEstimate(layer.Get()) == 0);
    auto defaultLayerBounds = WKRenderLayerGetAbsoluteBounds(layer.Get());
    Require(defaultLayerBounds.origin.x == 0.0 && defaultLayerBounds.origin.y == 0.0);
    Require(defaultLayerBounds.size.width == 0.0 && defaultLayerBounds.size.height == 0.0);

    auto childLayer = NKWebKit::Make<NKWebKit::RenderLayer>();
    auto negList = NKWebKit::Make<NKWebKit::Array>();
    auto posList = NKWebKit::Make<NKWebKit::Array>();
    auto flowList = NKWebKit::Make<NKWebKit::Array>();
    layer->renderer = obj;
    layer->frameContentsLayer = childLayer;
    layer->rendererName = NKWebKit::String::FromUtf8("RenderLayerModel");
    layer->elementTagName = NKWebKit::String::FromUtf8("body");
    layer->elementID = NKWebKit::String::FromUtf8("page-body");
    layer->elementClassNames = classes;
    layer->negativeZOrderList = negList;
    layer->positiveZOrderList = posList;
    layer->normalFlowList = flowList;
    layer->isClipping = true;
    layer->isClipped = true;
    layer->isReflection = true;
    layer->compositingLayerType = 2;
    layer->backingStoreMemoryEstimate = 1048576;
    layer->absoluteBounds = {{5.0, 15.0}, {800.0, 600.0}};

    Require(WKRenderLayerGetRenderer(layer.Get()) == obj.Get());
    Require(WKRenderLayerGetFrameContentsLayer(layer.Get()) == childLayer.Get());
    auto* rName = WKRenderLayerCopyRendererName(layer.Get());
    Require(rName != nullptr);
    Require(static_cast<const NKWebKit::String*>(rName)->Utf8() == "RenderLayerModel");
    WKRelease(rName);
    auto* eTag = WKRenderLayerCopyElementTagName(layer.Get());
    Require(eTag != nullptr);
    Require(static_cast<const NKWebKit::String*>(eTag)->Utf8() == "body");
    WKRelease(eTag);
    auto* eId = WKRenderLayerCopyElementID(layer.Get());
    Require(eId != nullptr);
    Require(static_cast<const NKWebKit::String*>(eId)->Utf8() == "page-body");
    WKRelease(eId);
    Require(WKRenderLayerGetElementClassNames(layer.Get()) == classes.Get());
    Require(WKRenderLayerGetNegativeZOrderList(layer.Get()) == negList.Get());
    Require(WKRenderLayerGetPositiveZOrderList(layer.Get()) == posList.Get());
    Require(WKRenderLayerGetNormalFlowList(layer.Get()) == flowList.Get());
    Require(WKRenderLayerIsClipping(layer.Get()));
    Require(WKRenderLayerIsClipped(layer.Get()));
    Require(WKRenderLayerIsReflection(layer.Get()));
    Require(WKRenderLayerGetCompositingLayerType(layer.Get()) == 2);
    Require(WKRenderLayerGetBackingStoreMemoryEstimate(layer.Get()) == 1048576);
    auto bounds = WKRenderLayerGetAbsoluteBounds(layer.Get());
    Require(bounds.origin.x == 5.0 && bounds.origin.y == 15.0);
    Require(bounds.size.width == 800.0 && bounds.size.height == 600.0);

    requireThrows([] { WKRenderObjectCopyElementID(nullptr); });
    requireThrows([] { WKRenderObjectCopyElementTagName(nullptr); });
    requireThrows([] { WKRenderObjectCopyName(nullptr); });
    requireThrows([] { WKRenderObjectCopyTextSnippet(nullptr); });
    requireThrows([] { WKRenderObjectGetAbsolutePosition(nullptr); });
    requireThrows([] { WKRenderObjectGetChildren(nullptr); });
    requireThrows([] { WKRenderObjectGetElementClassNames(nullptr); });
    requireThrows([] { WKRenderObjectGetFrameRect(nullptr); });
    requireThrows([] { WKRenderObjectGetTextLength(nullptr); });

    requireThrows([] { WKRenderLayerCopyElementID(nullptr); });
    requireThrows([] { WKRenderLayerCopyElementTagName(nullptr); });
    requireThrows([] { WKRenderLayerCopyRendererName(nullptr); });
    requireThrows([] { WKRenderLayerGetAbsoluteBounds(nullptr); });
    requireThrows([] { WKRenderLayerGetBackingStoreMemoryEstimate(nullptr); });
    requireThrows([] { WKRenderLayerGetCompositingLayerType(nullptr); });
    requireThrows([] { WKRenderLayerGetElementClassNames(nullptr); });
    requireThrows([] { WKRenderLayerGetFrameContentsLayer(nullptr); });
    requireThrows([] { WKRenderLayerGetNegativeZOrderList(nullptr); });
    requireThrows([] { WKRenderLayerGetNormalFlowList(nullptr); });
    requireThrows([] { WKRenderLayerGetPositiveZOrderList(nullptr); });
    requireThrows([] { WKRenderLayerGetRenderer(nullptr); });
    requireThrows([] { WKRenderLayerIsClipped(nullptr); });
    requireThrows([] { WKRenderLayerIsClipping(nullptr); });
    requireThrows([] { WKRenderLayerIsReflection(nullptr); });

    requireThrows([&] { WKRenderLayerGetRenderer(obj.Get()); });
    requireThrows([&] { WKRenderObjectGetChildren(layer.Get()); });
}

static void TestView() {
    auto view = NKWebKit::Make<NKWebKit::View>();
    const void* ref = view.Get();

    Require(WKViewGetPageScaleFactor(ref) == 1.0);
    Require(WKViewGetViewScaleFactor(ref) == 1.0);
    Require(!WKViewIsFullScreen(ref));
    Require(view->drawsBackground);

    WKViewSetPageScaleFactor(ref, 2.5);
    Require(WKViewGetPageScaleFactor(ref) == 2.5);

    WKViewSetViewScaleFactor(ref, 1.75);
    Require(WKViewGetViewScaleFactor(ref) == 1.75);

    WKViewSetDrawsBackground(ref, false);
    Require(!view->drawsBackground);
    WKViewSetDrawsBackground(ref, true);
    Require(view->drawsBackground);

    WKViewWillEnterFullScreen(ref);
    Require(!WKViewIsFullScreen(ref));
    WKViewDidEnterFullScreen(ref);
    Require(WKViewIsFullScreen(ref));
    WKViewWillExitFullScreen(ref);
    Require(WKViewIsFullScreen(ref));
    WKViewDidExitFullScreen(ref);
    Require(!WKViewIsFullScreen(ref));

    WKViewDidEnterFullScreen(ref);
    Require(WKViewIsFullScreen(ref));
    WKViewRequestExitFullScreen(ref);
    Require(!WKViewIsFullScreen(ref));

    bool threw = false;
    try {
        WKViewSetPageScaleFactor(ref, 0.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewSetPageScaleFactor(ref, -1.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewSetViewScaleFactor(ref, 0.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewSetViewScaleFactor(ref, -0.5);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewDidEnterFullScreen(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewDidExitFullScreen(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewRequestExitFullScreen(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewWillEnterFullScreen(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewWillExitFullScreen(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewGetPageScaleFactor(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewGetViewScaleFactor(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewIsFullScreen(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewSetDrawsBackground(nullptr, true);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewSetPageScaleFactor(nullptr, 1.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        WKViewSetViewScaleFactor(nullptr, 1.0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
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
    TestNavigationData();
    TestHitTestResult();
    TestBackForwardList();
    TestBundleScriptWorld();
    TestBundleDOMWindowExtension();
    TestBundleHitTestResult();
    TestBundleNodeHandle();
    TestDownload();
    TestAXObject();
    TestPopupMenuItem();
    TestOpenPanelResultListener();
    TestProtectionSpace();
    TestPermissionRequests();
    TestFrameHandle();
    TestOpenPanelParameters();
    TestNotification();
    TestImage();
    TestCertificateInfo();
    try {
        TestEmptyFrameLoaderClient();
    } catch (const std::runtime_error& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }

    TestInspector();
    TestContext();
    TestSessionState();
    TestSerializedScriptValue();
    TestRunLoop();
    try {
        TestEmptyFrameLoaderLifecycleCallbacks();
    } catch (const std::runtime_error& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
    TestFrame();
    TestRenderTree();
    TestView();
    std::printf("All libSceNKWebKit Base tests passed!\n");
    return 0;
}

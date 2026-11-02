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
#include <stdexcept>
#include <string>
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

    auto* invalidPat = WKUserContentURLPatternCreate(WKStringCreateWithUTF8CString("not_a_valid_pattern"));
    Require(!WKUserContentURLPatternIsValid(invalidPat));
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
}

static void TestError() {
    Require(WKErrorGetTypeID() == 10);
    auto* domain = WKErrorCopyWKErrorDomain();
    Require(WKStringIsEqualToUTF8CString(domain, "WebKitErrorDomain"));
    WKRelease(domain);
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
    TestBackForwardList();
    TestCertificateInfo();
    std::printf("All libSceNKWebKit Base tests passed!\n");
    return 0;
}

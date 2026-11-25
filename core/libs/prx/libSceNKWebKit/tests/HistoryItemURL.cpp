#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>

using NKWebKit::WebCoreAbi::HistoryItem;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::URL;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "HistoryItemURL check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

String Text(std::string_view text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text.data()), static_cast<unsigned>(text.size()));
}

std::u16string Units(const StringImpl* impl) {
    std::u16string result;
    if (!impl) return result;
    for (unsigned i = 0; i < impl->length; ++i) {
        result.push_back((impl->hashAndFlags & 4) ? static_cast<const unsigned char*>(impl->data)[i] : static_cast<const char16_t*>(impl->data)[i]);
    }
    return result;
}

std::u16string Widen(std::string_view text) {
    return std::u16string(text.begin(), text.end());
}

URL Parse(const String& input) {
    const URL base{};
    URL url{};
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(&url, &base, &input, nullptr);
    return url;
}

bool Valid(const URL& url) {
    return (url.bits & 1) != 0;
}

bool SameURL(const URL& left, const URL& right) {
    if (!left.impl != !right.impl) return false;
    if (left.impl && Units(left.impl) != Units(right.impl)) return false;
    return left.bits == right.bits && left.userStart == right.userStart && left.userEnd == right.userEnd
        && left.passwordEnd == right.passwordEnd && left.hostEnd == right.hostEnd
        && left.pathAfterLastSlash == right.pathAfterLastSlash && left.pathEnd == right.pathEnd && left.queryEnd == right.queryEnd;
}

void ReleaseURL(URL& url) {
    NKWebKit::WtfAbi::Release(url.impl);
    url.impl = nullptr;
}

void SetStrings(HistoryItem& item, const String& url, const String& original) {
    _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(&item, &url);
    _ZN7WebCore11HistoryItem20setOriginalURLStringERKN3WTF6StringE(&item, &original);
}

URL ItemURL(const HistoryItem& item) {
    return _ZNK7WebCore11HistoryItem3urlEv(&item);
}

URL ItemOriginalURL(const HistoryItem& item) {
    return _ZNK7WebCore11HistoryItem11originalURLEv(&item);
}

void RequireMatchesParser(const HistoryItem& item) {
    unsigned urlRefs = item.urlString.impl ? item.urlString.impl->refCount : 0;
    unsigned originalRefs = item.originalURLString.impl ? item.originalURLString.impl->refCount : 0;

    URL fromItem = ItemURL(item);
    URL direct = Parse(item.urlString);
    Require(SameURL(fromItem, direct));
    ReleaseURL(fromItem);
    ReleaseURL(direct);

    URL originalFromItem = ItemOriginalURL(item);
    URL originalDirect = Parse(item.originalURLString);
    Require(SameURL(originalFromItem, originalDirect));
    ReleaseURL(originalFromItem);
    ReleaseURL(originalDirect);

    if (item.urlString.impl) Require(item.urlString.impl->refCount == urlRefs);
    if (item.originalURLString.impl) Require(item.originalURLString.impl->refCount == originalRefs);
}

void TestCanonicalizesEachField() {
    HistoryItem item;
    SetStrings(item, Text("HTTP://Example.COM:80/a/../b?q#f"), Text("https://user:pw@Origin.test:8443/x/./y"));

    URL url = ItemURL(item);
    Require(Valid(url));
    Require(Units(url.impl) == u"http://example.com/b?q#f");
    URL original = ItemOriginalURL(item);
    Require(Valid(original));
    Require(Units(original.impl) == u"https://user:pw@origin.test:8443/x/y");
    Require(original.userStart == 8 && original.userEnd == 12 && original.passwordEnd == 15);
    ReleaseURL(url);
    ReleaseURL(original);

    RequireMatchesParser(item);
}

void TestStringsStayUntouched() {
    HistoryItem item;
    SetStrings(item, Text("http://a/"), Text("http://b/"));
    StringImpl* urlImpl = item.urlString.impl;
    StringImpl* originalImpl = item.originalURLString.impl;
    unsigned urlRefs = urlImpl->refCount;

    URL url = ItemURL(item);
    Require(Valid(url) && Units(url.impl) == u"http://a/");
    Require(item.urlString.impl == urlImpl && item.originalURLString.impl == originalImpl);
    ReleaseURL(url);
    Require(urlImpl->refCount == urlRefs);
    Require(Units(item.urlString.impl) == u"http://a/");

    URL first = ItemOriginalURL(item);
    URL second = ItemOriginalURL(item);
    Require(SameURL(first, second));
    ReleaseURL(first);
    ReleaseURL(second);
    RequireMatchesParser(item);
}

void TestInvalidAndEmpty() {
    HistoryItem empty;
    RequireMatchesParser(empty);
    URL none = ItemURL(empty);
    Require(!Valid(none));
    ReleaseURL(none);

    HistoryItem relative;
    SetStrings(relative, Text("foo/bar"), Text(""));
    URL url = ItemURL(relative);
    Require(!Valid(url));
    ReleaseURL(url);
    URL original = ItemOriginalURL(relative);
    Require(!Valid(original));
    ReleaseURL(original);
    RequireMatchesParser(relative);

    HistoryItem spaces;
    SetStrings(spaces, Text("  http://a/b c  "), Text("http://[::1"));
    URL trimmed = ItemURL(spaces);
    Require(Valid(trimmed) && Units(trimmed.impl) == u"http://a/b%20c");
    ReleaseURL(trimmed);
    URL broken = ItemOriginalURL(spaces);
    Require(!Valid(broken));
    ReleaseURL(broken);
    RequireMatchesParser(spaces);
}

void TestOtherSchemes() {
    for (const char* text : {"about:blank", "data:text/plain,hi", "file:///C:/dir/file.html", "javascript:void(0)", "blob:http://a/uuid"}) {
        HistoryItem item;
        SetStrings(item, Text(text), Text(text));
        URL url = ItemURL(item);
        Require(Valid(url));
        Require(Units(url.impl) == Widen(text));
        ReleaseURL(url);
        RequireMatchesParser(item);
    }
}

void TestNullItem() {
    bool urlThrew = false;
    try {
        _ZNK7WebCore11HistoryItem3urlEv(nullptr);
    } catch (const std::invalid_argument&) {
        urlThrew = true;
    }
    bool originalThrew = false;
    try {
        _ZNK7WebCore11HistoryItem11originalURLEv(nullptr);
    } catch (const std::invalid_argument&) {
        originalThrew = true;
    }
    Require(urlThrew && originalThrew);
}

}

int main() {
    TestCanonicalizesEachField();
    TestStringsStayUntouched();
    TestInvalidAndEmpty();
    TestOtherSchemes();
    TestNullItem();
    return 0;
}

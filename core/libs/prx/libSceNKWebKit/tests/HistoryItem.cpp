#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using NKWebKit::WebCoreAbi::HistoryItem;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using WebCore::IntPoint;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "HistoryItem check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static StringImpl Make(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

static void Clear(HistoryItem& item) {
    String empty;
    _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(&item, &empty);
    _ZN7WebCore11HistoryItem20setOriginalURLStringERKN3WTF6StringE(&item, &empty);
    _ZN7WebCore11HistoryItem11setReferrerERKN3WTF6StringE(&item, &empty);
    _ZN7WebCore11HistoryItem8setTitleERKN3WTF6StringE(&item, &empty);
    _ZN7WebCore11HistoryItem17setAlternateTitleERKN3WTF6StringE(&item, &empty);
    _ZN7WebCore11HistoryItem18setFormContentTypeERKN3WTF6StringE(&item, &empty);
}

static void TestStrings() {
    HistoryItem item;
    auto url = Make("http://a/");
    auto original = Make("http://o/");
    auto referrer = Make("http://r/");
    auto title = Make("t");
    auto alternate = Make("alt");
    auto contentType = Make("text/plain");
    String urlString(&url);
    String originalString(&original);
    String referrerString(&referrer);
    String titleString(&title);
    String alternateString(&alternate);
    String contentTypeString(&contentType);
    url.refCount = original.refCount = referrer.refCount = title.refCount = alternate.refCount = contentType.refCount = 4;

    _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(&item, &urlString);
    _ZN7WebCore11HistoryItem20setOriginalURLStringERKN3WTF6StringE(&item, &originalString);
    _ZN7WebCore11HistoryItem11setReferrerERKN3WTF6StringE(&item, &referrerString);
    _ZN7WebCore11HistoryItem8setTitleERKN3WTF6StringE(&item, &titleString);
    _ZN7WebCore11HistoryItem17setAlternateTitleERKN3WTF6StringE(&item, &alternateString);
    _ZN7WebCore11HistoryItem18setFormContentTypeERKN3WTF6StringE(&item, &contentTypeString);
    Require(url.refCount == 6 && title.refCount == 6 && contentType.refCount == 6);

    Require(_ZNK7WebCore11HistoryItem9urlStringEv(&item).impl == &url);
    Require(&_ZNK7WebCore11HistoryItem9urlStringEv(&item) == &item.urlString);
    Require(_ZNK7WebCore11HistoryItem17originalURLStringEv(&item).impl == &original);
    Require(_ZNK7WebCore11HistoryItem8referrerEv(&item).impl == &referrer);
    Require(_ZNK7WebCore11HistoryItem5titleEv(&item).impl == &title);
    Require(_ZNK7WebCore11HistoryItem14alternateTitleEv(&item).impl == &alternate);
    Require(&_ZNK7WebCore11HistoryItem14alternateTitleEv(&item) == &item.displayTitle);
    Require(!_ZNK7WebCore11HistoryItem6targetEv(&item).impl);
    {
        String copy = _ZNK7WebCore11HistoryItem15formContentTypeEv(&item);
        Require(copy.impl == &contentType && contentType.refCount == 8);
    }
    Require(contentType.refCount == 6);

    _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(&item, &item.urlString);
    Require(url.refCount == 6);

    auto next = Make("http://b/");
    String nextString(&next);
    next.refCount = 4;
    _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(&item, &nextString);
    Require(url.refCount == 4 && next.refCount == 6);
    Require(_ZNK7WebCore11HistoryItem9urlStringEv(&item).impl == &next);

    Clear(item);
    Require(url.refCount == 4 && original.refCount == 4 && referrer.refCount == 4);
    Require(title.refCount == 4 && alternate.refCount == 4 && contentType.refCount == 4 && next.refCount == 4);

    for (auto* impl : {&url, &original, &referrer, &title, &alternate, &contentType, &next}) impl->refCount = 2;
    urlString.impl = originalString.impl = referrerString.impl = titleString.impl = alternateString.impl = contentTypeString.impl = nextString.impl = nullptr;
}

static void TestScalars() {
    HistoryItem item;
    item.shouldRestoreScrollPosition = true;

    Require(!_ZNK7WebCore11HistoryItem12isTargetItemEv(&item));
    _ZN7WebCore11HistoryItem15setIsTargetItemEb(&item, true);
    Require(_ZNK7WebCore11HistoryItem12isTargetItemEv(&item));
    Require(!item.lastVisitWasFailure && !item.wasRestoredFromSession && item.shouldRestoreScrollPosition);
    _ZN7WebCore11HistoryItem15setIsTargetItemEb(&item, false);
    Require(!_ZNK7WebCore11HistoryItem12isTargetItemEv(&item));

    Require(_ZNK7WebCore11HistoryItem27shouldRestoreScrollPositionEv(&item));
    _ZN7WebCore11HistoryItem30setShouldRestoreScrollPositionEb(&item, false);
    Require(!_ZNK7WebCore11HistoryItem27shouldRestoreScrollPositionEv(&item));
    Require(!item.isTargetItem);

    Require(_ZNK7WebCore11HistoryItem15pageScaleFactorEv(&item) == 0);
    _ZN7WebCore11HistoryItem18setPageScaleFactorEf(&item, 1.5f);
    Require(_ZNK7WebCore11HistoryItem15pageScaleFactorEv(&item) == 1.5f);

    IntPoint point{-3, 9};
    _ZN7WebCore11HistoryItem17setScrollPositionERKNS_8IntPointE(&item, &point);
    const IntPoint& stored = _ZNK7WebCore11HistoryItem14scrollPositionEv(&item);
    Require(&stored == &item.scrollPosition && stored.m_x == -3 && stored.m_y == 9);

    Require(_ZNK7WebCore11HistoryItem28shouldOpenExternalURLsPolicyEv(&item) == 0);
    _ZN7WebCore11HistoryItem31setShouldOpenExternalURLsPolicyENS_28ShouldOpenExternalURLsPolicyE(&item, 2);
    Require(_ZNK7WebCore11HistoryItem28shouldOpenExternalURLsPolicyEv(&item) == 2);
}

static void TestContainers() {
    HistoryItem item;
    int child = 0;
    Require(!_ZNK7WebCore11HistoryItem11hasChildrenEv(&item));
    Require(&_ZNK7WebCore11HistoryItem8childrenEv(&item) == &item.children);
    Require(&_ZNK7WebCore11HistoryItem13documentStateEv(&item) == &item.documentState);
    item.children = {&child, 1, 1};
    Require(_ZNK7WebCore11HistoryItem11hasChildrenEv(&item));
    Require(_ZNK7WebCore11HistoryItem8childrenEv(&item).size == 1);
    item.children = {nullptr, 4, 0};
    Require(!_ZNK7WebCore11HistoryItem11hasChildrenEv(&item));

    Require(!_ZN7WebCore11HistoryItem8formDataEv(&item));
    item.formData = &child;
    Require(_ZN7WebCore11HistoryItem8formDataEv(&item) == &child);
    item.formData = nullptr;

    Require(!_ZNK7WebCore11HistoryItem20hasCachedPageExpiredEv(&item));
}

static void TestNull() {
    bool threw = false;
    try {
        _ZNK7WebCore11HistoryItem9urlStringEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    HistoryItem item;
    threw = false;
    try {
        _ZN7WebCore11HistoryItem11setReferrerERKN3WTF6StringE(&item, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    TestStrings();
    TestScalars();
    TestContainers();
    TestNull();
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using NKWebKit::WebCoreAbi::HistoryItem;
using NKWebKit::WebCoreAbi::CachedPage;
using NKWebKit::WebCoreAbi::HistoryItemChangedHook;
using NKWebKit::WebCoreAbi::HistoryItemVectorStorage;
using NKWebKit::WebCoreAbi::historyItemChangedHook;
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

static void TestCachedPageExpiry() {
    HistoryItem item;
    const double now = _ZN3WTF13MonotonicTime3nowEv().m_value;
    CachedPage page{nullptr, now + 60};
    item.cachedPage = &page;
    Require(!_ZNK7WebCore11HistoryItem20hasCachedPageExpiredEv(&item));
    page.expirationTime = now - 1;
    Require(_ZNK7WebCore11HistoryItem20hasCachedPageExpiredEv(&item));
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

struct ScopedChangedHook {
    HistoryItemChangedHook saved = nullptr;
    explicit ScopedChangedHook(HistoryItemChangedHook hook) : saved(historyItemChangedHook) {
        historyItemChangedHook = hook;
    }
    ~ScopedChangedHook() {
        historyItemChangedHook = saved;
    }
};

static void TestChangedHook() {
    HistoryItem item;
    auto impl = Make("http://hook/");
    String value(&impl);
    impl.refCount = 4;

    _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(&item, &value);
    Require(_ZNK7WebCore11HistoryItem9urlStringEv(&item).impl == &impl);
    Require(impl.refCount == 6);

    static HistoryItem* hooked = nullptr;
    static int calls = 0;
    hooked = nullptr;
    calls = 0;
    {
        ScopedChangedHook guard(+[](HistoryItem* seen) { hooked = seen; ++calls; });

        _ZN7WebCore11HistoryItem12setURLStringERKN3WTF6StringE(&item, &value);
        Require(calls == 1 && hooked == &item);
        _ZN7WebCore11HistoryItem20setOriginalURLStringERKN3WTF6StringE(&item, &value);
        Require(calls == 2 && hooked == &item);
        _ZN7WebCore11HistoryItem11setReferrerERKN3WTF6StringE(&item, &value);
        Require(calls == 3 && hooked == &item);
        _ZN7WebCore11HistoryItem8setTitleERKN3WTF6StringE(&item, &value);
        Require(calls == 4 && hooked == &item);
        _ZN7WebCore11HistoryItem17setAlternateTitleERKN3WTF6StringE(&item, &value);
        Require(calls == 5 && hooked == &item);
        _ZN7WebCore11HistoryItem30setShouldRestoreScrollPositionEb(&item, true);
        Require(calls == 6 && hooked == &item);
        Require(_ZNK7WebCore11HistoryItem27shouldRestoreScrollPositionEv(&item));

        String empty;
        _ZN7WebCore11HistoryItem18setFormContentTypeERKN3WTF6StringE(&item, &empty);
        IntPoint point{1, 2};
        _ZN7WebCore11HistoryItem17setScrollPositionERKNS_8IntPointE(&item, &point);
        _ZN7WebCore11HistoryItem18setPageScaleFactorEf(&item, 2.0f);
        _ZN7WebCore11HistoryItem15setIsTargetItemEb(&item, true);
        _ZN7WebCore11HistoryItem31setShouldOpenExternalURLsPolicyENS_28ShouldOpenExternalURLsPolicyE(&item, 1);
        Require(calls == 6);
        Require(_ZNK7WebCore11HistoryItem15pageScaleFactorEv(&item) == 2.0f);
        Require(_ZNK7WebCore11HistoryItem12isTargetItemEv(&item));
    }
    Clear(item);
    Require(impl.refCount == 4);
    impl.refCount = 2;
    value.impl = nullptr;
}

static void TestTarget() {
    HistoryItem item;
    Require(!_ZNK7WebCore11HistoryItem6targetEv(&item).impl);

    auto target = Make("_blank");
    String targetString(&target);
    target.refCount = 4;
    _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(&item, &targetString);
    Require(_ZNK7WebCore11HistoryItem6targetEv(&item).impl == &target);
    Require(target.refCount == 6);

    static int calls = 0;
    calls = 0;
    {
        ScopedChangedHook guard(+[](HistoryItem*) { ++calls; });
        _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(&item, &targetString);
        Require(calls == 1);
    }

    HistoryItem first;
    HistoryItem second;
    auto firstName = Make("main");
    auto secondName = Make("side");
    String firstString(&firstName);
    String secondString(&secondName);
    firstName.refCount = secondName.refCount = 4;
    _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(&first, &firstString);
    _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(&second, &secondString);

    HistoryItem* storage[2] = {&first, &second};
    HistoryItem parent;
    parent.children = {storage, 2, 2};

    auto lookupMain = Make("main");
    auto lookupSide = Make("side");
    auto lookupMissing = Make("gone");
    String mainString(&lookupMain);
    String sideString(&lookupSide);
    String missingString(&lookupMissing);
    lookupMain.refCount = lookupSide.refCount = lookupMissing.refCount = 4;

    Require(_ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(&parent, &mainString) == &first);
    Require(_ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(&parent, &sideString) == &second);
    Require(!_ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(&parent, &missingString));

    HistoryItem empty;
    empty.children = {nullptr, 0, 0};
    Require(!_ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(&empty, &mainString));

    bool threw = false;
    try { _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(&item, nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    try { _ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(&parent, nullptr); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    try { _ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(nullptr, &mainString); }
    catch (const std::invalid_argument&) { threw = true; }
    Require(threw);

    String emptyString;
    _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(&item, &emptyString);
    _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(&first, &emptyString);
    _ZN7WebCore11HistoryItem9setTargetERKN3WTF6StringE(&second, &emptyString);
    for (auto* impl : {&target, &firstName, &secondName, &lookupMain, &lookupSide, &lookupMissing}) impl->refCount = 2;
    targetString.impl = firstString.impl = secondString.impl = mainString.impl = sideString.impl = missingString.impl = nullptr;
}

static void TestConstructors() {
    static int hookCalls = 0;
    ScopedChangedHook guard(+[](HistoryItem*) { ++hookCalls; });
    auto url = Make("http://x/");
    auto title = Make("T");
    auto alternate = Make("A");
    String urlString(&url);
    String titleString(&title);
    String alternateString(&alternate);
    url.refCount = title.refCount = alternate.refCount = 4;
    NKWebKit::WebCoreAbi::BackForwardItemIdentifier identifier{7, 3};

    HistoryItem item;
    _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(&item, &urlString, &titleString, &alternateString, identifier);
    Require(item.refCount == 1);
    Require(item.urlString.impl == &url && url.refCount == 8);
    Require(item.originalURLString.impl == &url && url.refCount == 8);
    Require(item.title.impl == &title && title.refCount == 6);
    Require(item.displayTitle.impl == &alternate && alternate.refCount == 6);
    Require(!item.referrer.impl);
    Require(!item.target.impl);
    Require(!item.formContentType.impl);
    Require(item.scrollPosition.m_x == 0 && item.scrollPosition.m_y == 0);
    Require(item.pageScaleFactor == 0);
    Require(!item.documentState.buffer && !item.documentState.capacity && !item.documentState.size);
    Require(!item.children.buffer && !item.children.capacity && !item.children.size);
    Require(!item.lastVisitWasFailure && !item.isTargetItem);
    Require(!item.wasRestoredFromSession && !item.wasCreatedByJSWithoutUserInteraction);
    Require(item.shouldRestoreScrollPosition);
    Require(item.shouldOpenExternalURLsPolicy == 0);
    Require(!item.stateObject && !item.formData && !item.cachedPage);
    Require(item.pruningReason == 0);
    Require(sizeof(item.pruningReason) == 4);
    item.pruningReason = 0x12345678;
    Require(item.pruningReason == 0x12345678);
    item.pruningReason = 0;
    Require(item.identifier.objectIdentifier == 7 && item.identifier.processIdentifier == 3);
    Require(!item.policyContainer.engaged);
    Require(item.documentSequenceNumber == item.itemSequenceNumber + 1);

    HistoryItem second;
    _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(&second, &urlString, &titleString, &alternateString, identifier);
    Require(second.itemSequenceNumber == item.documentSequenceNumber + 1);
    Require(second.documentSequenceNumber == second.itemSequenceNumber + 1);
    Require(second.urlString.impl == &url && url.refCount == 12);

    HistoryItem complete;
    _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(&complete, &urlString, &titleString, &alternateString, identifier);
    Require(complete.refCount == 1);
    Require(complete.urlString.impl == &url && complete.originalURLString.impl == &url);
    Require(complete.title.impl == &title && complete.displayTitle.impl == &alternate);
    Require(complete.shouldRestoreScrollPosition);
    Require(complete.identifier.objectIdentifier == 7);

    HistoryItem bad;
    bool threw = false;
    try {
        _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(&bad, nullptr, &titleString, &alternateString, identifier);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(bad.refCount == 0 && !bad.urlString.impl);

    threw = false;
    try {
        _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(&bad, &urlString, nullptr, &alternateString, identifier);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(bad.refCount == 0 && !bad.urlString.impl && !bad.title.impl);

    threw = false;
    try {
        _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_NS_25BackForwardItemIdentifierE(&bad, &urlString, &titleString, nullptr, identifier);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(bad.refCount == 0 && !bad.urlString.impl && !bad.title.impl && !bad.displayTitle.impl);
    Require(hookCalls == 0);

    item.urlString.impl = nullptr;
    item.originalURLString.impl = nullptr;
    item.title.impl = nullptr;
    item.displayTitle.impl = nullptr;
    second.urlString.impl = nullptr;
    second.originalURLString.impl = nullptr;
    second.title.impl = nullptr;
    second.displayTitle.impl = nullptr;
    complete.urlString.impl = nullptr;
    complete.originalURLString.impl = nullptr;
    complete.title.impl = nullptr;
    complete.displayTitle.impl = nullptr;
}

static void TestStateObjectAndFormData() {
    HistoryItem item;
    static HistoryItem* hooked = nullptr;
    static int calls = 0;
    hooked = nullptr;
    calls = 0;
    int value = 0;
    int other = 0;
    {
        ScopedChangedHook guard(+[](HistoryItem* seen) { hooked = seen; ++calls; });

        void* object = &value;
        _ZN7WebCore11HistoryItem14setStateObjectEON3WTF6RefPtrINS_21SerializedScriptValueENS1_13DumbPtrTraitsIS3_EEEE(&item, &object);
        Require(item.stateObject == &value);
        Require(object == nullptr);
        Require(calls == 1 && hooked == &item);

        void* form = &value;
        _ZN7WebCore11HistoryItem11setFormDataEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&item, &form);
        Require(item.formData == &value);
        Require(form == nullptr);
        Require(calls == 1);

        item.stateObject = nullptr;
        void* nullObject = nullptr;
        _ZN7WebCore11HistoryItem14setStateObjectEON3WTF6RefPtrINS_21SerializedScriptValueENS1_13DumbPtrTraitsIS3_EEEE(&item, &nullObject);
        Require(!item.stateObject && nullObject == nullptr);
        Require(calls == 2);

        item.formData = nullptr;
        void* nullForm = nullptr;
        _ZN7WebCore11HistoryItem11setFormDataEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&item, &nullForm);
        Require(!item.formData);
        Require(calls == 2);

        item.stateObject = &value;
        _ZN7WebCore11HistoryItem14setStateObjectEON3WTF6RefPtrINS_21SerializedScriptValueENS1_13DumbPtrTraitsIS3_EEEE(&item, &item.stateObject);
        Require(item.stateObject == &value);
        item.formData = &value;
        _ZN7WebCore11HistoryItem11setFormDataEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&item, &item.formData);
        Require(item.formData == &value);
        Require(calls == 3);

        bool threw = false;
        try {
            _ZN7WebCore11HistoryItem14setStateObjectEON3WTF6RefPtrINS_21SerializedScriptValueENS1_13DumbPtrTraitsIS3_EEEE(&item, nullptr);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);

        threw = false;
        try {
            _ZN7WebCore11HistoryItem11setFormDataEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&item, nullptr);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);

        void* replacement = &other;
        threw = false;
        try {
            _ZN7WebCore11HistoryItem14setStateObjectEON3WTF6RefPtrINS_21SerializedScriptValueENS1_13DumbPtrTraitsIS3_EEEE(&item, &replacement);
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw);
        Require(item.stateObject == &value && replacement == &other);

        void* nullReplacement = nullptr;
        threw = false;
        try {
            _ZN7WebCore11HistoryItem14setStateObjectEON3WTF6RefPtrINS_21SerializedScriptValueENS1_13DumbPtrTraitsIS3_EEEE(&item, &nullReplacement);
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw);
        Require(item.stateObject == &value && nullReplacement == nullptr);

        threw = false;
        try {
            _ZN7WebCore11HistoryItem11setFormDataEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&item, &replacement);
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw);
        Require(item.formData == &value && replacement == &other);

        threw = false;
        try {
            _ZN7WebCore11HistoryItem11setFormDataEON3WTF6RefPtrINS_8FormDataENS1_13DumbPtrTraitsIS3_EEEE(&item, &nullReplacement);
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw);
        Require(item.formData == &value && nullReplacement == nullptr);

        Require(calls == 3);
    }
}

static void TestSetDocumentState() {
    static int hookCalls = 0;
    ScopedChangedHook guard(+[](HistoryItem*) { ++hookCalls; });
    HistoryItem item;
    auto a = Make("a");
    auto b = Make("b");
    auto c = Make("c");
    String elements[3] = {String(&a), String(&b), String(&c)};
    a.refCount = b.refCount = c.refCount = 6;
    HistoryItemVectorStorage source{elements, 3, 3};

    _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&item, &source);
    Require(item.documentState.size == 3 && item.documentState.capacity == 3);
    void* firstBuffer = item.documentState.buffer;
    Require(firstBuffer);
    const String* stored = static_cast<const String*>(item.documentState.buffer);
    Require(stored[0].impl == &a && stored[1].impl == &b && stored[2].impl == &c);
    Require(a.refCount == 8 && b.refCount == 8 && c.refCount == 8);

    String one[1] = {String(&c)};
    c.refCount = 10;
    HistoryItemVectorStorage shrinkSource{one, 1, 1};
    _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&item, &shrinkSource);
    Require(item.documentState.size == 1 && item.documentState.capacity == 3 && item.documentState.buffer == firstBuffer);
    stored = static_cast<const String*>(item.documentState.buffer);
    Require(stored[0].impl == &c);
    Require(a.refCount == 6 && b.refCount == 6 && c.refCount == 10);

    _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&item, &source);
    Require(item.documentState.size == 3 && item.documentState.buffer == firstBuffer);
    stored = static_cast<const String*>(item.documentState.buffer);
    Require(stored[0].impl == &a && stored[1].impl == &b && stored[2].impl == &c);
    Require(a.refCount == 8 && b.refCount == 8 && c.refCount == 10);

    _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&item, &item.documentState);
    Require(item.documentState.size == 3 && item.documentState.buffer == firstBuffer);
    Require(a.refCount == 8 && b.refCount == 8 && c.refCount == 10);

    String four[4] = {String(&a), String(&b), String(&c), String(&b)};
    a.refCount = 10;
    b.refCount = 12;
    c.refCount = 12;
    HistoryItemVectorStorage growSource{four, 4, 4};
    _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&item, &growSource);
    Require(item.documentState.size == 4 && item.documentState.capacity == 4);
    Require(item.documentState.buffer != firstBuffer);
    stored = static_cast<const String*>(item.documentState.buffer);
    Require(stored[0].impl == &a && stored[1].impl == &b && stored[2].impl == &c && stored[3].impl == &b);
    Require(a.refCount == 10 && b.refCount == 14 && c.refCount == 12);

    HistoryItemVectorStorage emptySource{};
    _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&item, &emptySource);
    Require(!item.documentState.size && item.documentState.capacity == 4 && item.documentState.buffer);
    Require(a.refCount == 8 && b.refCount == 10 && c.refCount == 10);

    bool threw = false;
    try {
        _ZN7WebCore11HistoryItem16setDocumentStateERKN3WTF6VectorINS1_6StringELm0ENS1_15CrashOnOverflowELm16ENS1_10FastMallocEEE(&item, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    Require(hookCalls == 0);
    _ZN3WTF8fastFreeEPv(item.documentState.buffer);
    item.documentState = {};
}

static void TestAddChildItem() {
    static int hookCalls = 0;
    ScopedChangedHook guard(+[](HistoryItem*) { ++hookCalls; });
    HistoryItem parent;
    HistoryItem parent2;
    HistoryItem children[17];
    HistoryItem nullChild;
    static char texts[17][8];
    StringImpl targetImpls[17];
    for (unsigned i = 0; i < 17; ++i) {
        std::snprintf(texts[i], sizeof(texts[i]), "f%u", i);
        targetImpls[i] = Make(texts[i]);
        targetImpls[i].refCount = 4;
        children[i].target.impl = &targetImpls[i];
        targetImpls[i].refCount += 2;
        children[i].refCount = 5;
    }

    for (unsigned i = 0; i < 17; ++i) {
        HistoryItem* ref = &children[i];
        _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(&parent, &ref);
        Require(ref == nullptr);
        Require(children[i].refCount == 5);
    }
    Require(parent.children.size == 17 && parent.children.capacity == 21);
    const HistoryItem* const* stored = static_cast<const HistoryItem* const*>(parent.children.buffer);
    for (unsigned i = 0; i < 17; ++i) Require(stored[i] == &children[i]);
    Require(_ZNK7WebCore11HistoryItem11hasChildrenEv(&parent));

    String first(&targetImpls[0]);
    Require(_ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(&parent, &first) == &children[0]);

    {
        HistoryItem parent3;
        HistoryItem* ref = &children[0];
        _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(&parent3, &ref);
        Require(ref == nullptr);
        ref = &children[1];
        _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(&parent3, &ref);
        Require(ref == nullptr);
        String second(&targetImpls[1]);
        Require(_ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(&parent3, &second) == &children[1]);
        children[16].target.impl = &targetImpls[1];
        ref = &children[16];
        _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(&parent3, &ref);
        Require(ref == nullptr);
        Require(_ZN7WebCore11HistoryItem19childItemWithTargetERKN3WTF6StringE(&parent3, &second) == &children[1]);
        _ZN3WTF8fastFreeEPv(parent3.children.buffer);
        parent3.children = {};
        children[16].target.impl = nullptr;
        second.impl = nullptr;
    }

    {
        HistoryItem* ref = &nullChild;
        _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(&parent2, &ref);
        Require(ref == nullptr);
    }
    Require(_ZNK7WebCore11HistoryItem11hasChildrenEv(&parent2));

    bool threw = false;
    try {
        _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(&parent, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        HistoryItem* dummy = &children[0];
        _ZN7WebCore11HistoryItem12addChildItemEON3WTF3RefIS0_NS1_13DumbPtrTraitsIS0_EEEE(nullptr, &dummy);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    Require(hookCalls == 0);
    _ZN3WTF8fastFreeEPv(parent.children.buffer);
    _ZN3WTF8fastFreeEPv(parent2.children.buffer);
    parent.children = {};
    parent2.children = {};
    for (auto& child : children) child.target.impl = nullptr;
    first.impl = nullptr;
}

int main() {
    TestStrings();
    TestScalars();
    TestContainers();
    TestCachedPageExpiry();
    TestNull();
    TestChangedHook();
    TestTarget();
    TestConstructors();
    TestStateObjectAndFormData();
    TestSetDocumentState();
    TestAddChildItem();
    return 0;
}

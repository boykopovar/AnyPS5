#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WtfObjectIdentifier.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <stdexcept>
#include <unordered_set>

using NKWebKit::WebCoreAbi::HistoryItem;
using NKWebKit::WebCoreAbi::CachedPage;
using NKWebKit::WebCoreAbi::HistoryItemChangedHook;
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

struct Text {
    StringImpl impl;
    String owner;
    explicit Text(const char* text, unsigned flags = 4) : impl{4, static_cast<unsigned>(std::strlen(text)), text, flags}, owner(&impl) {}
    ~Text() {
        owner.impl = nullptr;
    }
    Text(const Text&) = delete;
    Text& operator=(const Text&) = delete;
    unsigned refs() const {
        return impl.refCount;
    }
};

struct Storage {
    alignas(HistoryItem) unsigned char bytes[sizeof(HistoryItem)];
    Storage() {
        std::memset(bytes, 0xAB, sizeof(bytes));
    }
    HistoryItem* get() {
        return reinterpret_cast<HistoryItem*>(bytes);
    }
};

template <typename TException, typename TFunction>
static bool Throws(TFunction&& function) {
    try {
        function();
    } catch (const TException&) {
        return true;
    }
    return false;
}

static HistoryItem* NewItem() {
    return static_cast<HistoryItem*>(_Znwm_nid_postfix(sizeof(HistoryItem)));
}

static HistoryItem* NewChild(Text& url, Text& title) {
    auto* child = NewItem();
    _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(child, &url.owner, &title.owner);
    return child;
}

static void SetChildren(HistoryItem& parent, std::initializer_list<HistoryItem*> children) {
    auto* buffer = static_cast<HistoryItem**>(_ZN3WTF10fastMallocEm(children.size() * sizeof(HistoryItem*)));
    unsigned index = 0;
    for (auto* child : children) buffer[index++] = child;
    parent.children = {buffer, index, index};
}

static void SetDocumentState(HistoryItem& item, std::initializer_list<Text*> entries) {
    auto* buffer = static_cast<String*>(_ZN3WTF10fastMallocEm(entries.size() * sizeof(String)));
    unsigned index = 0;
    for (auto* entry : entries) {
        entry->impl.refCount += 2;
        new (&buffer[index++]) String(&entry->impl);
    }
    item.documentState = {buffer, index, index};
}

static void Snapshot(const HistoryItem& item, unsigned char (&bytes)[sizeof(HistoryItem)]) {
    std::memcpy(bytes, &item, sizeof(HistoryItem));
}

static bool Unchanged(const HistoryItem& item, const unsigned char (&bytes)[sizeof(HistoryItem)]) {
    return std::memcmp(bytes, &item, sizeof(HistoryItem)) == 0;
}

static void RequireDefaults(const HistoryItem& item) {
    Require(item.refCount == 1);
    Require(item.scrollPosition.m_x == 0 && item.scrollPosition.m_y == 0);
    Require(item.pageScaleFactor == 0);
    Require(!item.documentState.buffer && !item.documentState.capacity && !item.documentState.size);
    Require(item.shouldOpenExternalURLsPolicy == 0);
    Require(!item.children.buffer && !item.children.capacity && !item.children.size);
    Require(!item.lastVisitWasFailure && !item.isTargetItem && !item.wasRestoredFromSession && !item.wasCreatedByJSWithoutUserInteraction);
    Require(item.shouldRestoreScrollPosition);
    Require(!item.stateObject && !item.formData && !item.cachedPage);
    Require(!item.referrer.impl && !item.target.impl && !item.formContentType.impl);
    Require(item.pruningReason == 0);
    Require(!item.policyContainerEngaged);
    const auto& policy = item.policyContainer;
    Require(!policy.contentSecurityPolicyResponseHeaders.headers.buffer && !policy.contentSecurityPolicyResponseHeaders.headers.size);
    Require(policy.contentSecurityPolicyResponseHeaders.httpStatusCode == 0 && !policy.contentSecurityPolicyResponseHeaders.emptyForMarkable);
    Require(!policy.crossOriginEmbedderPolicy.value && !policy.crossOriginEmbedderPolicy.reportOnlyValue);
    Require(!policy.crossOriginEmbedderPolicy.reportingEndpoint.impl && !policy.crossOriginEmbedderPolicy.reportOnlyReportingEndpoint.impl);
    Require(policy.crossOriginOpenerPolicy.value == 0 && policy.crossOriginOpenerPolicy.reportOnlyValue == 0);
    Require(!policy.crossOriginOpenerPolicy.reportingEndpoint.impl && !policy.crossOriginOpenerPolicy.reportOnlyReportingEndpoint.impl);
    Require(policy.referrerPolicy == 0);
}

using NoStringConstructor = void (APS5_VABI *)(HistoryItem*);
using TwoStringConstructor = void (APS5_VABI *)(HistoryItem*, const String*, const String*);
using ThreeStringConstructor = void (APS5_VABI *)(HistoryItem*, const String*, const String*, const String*);

static void TestNoStringConstructor(NoStringConstructor construct) {
    _ZN7WebCore7Process10identifierEv();
    const std::uint64_t before = NKWebKit::WtfAbi::GenerateObjectIdentifier();
    Storage storage;
    HistoryItem& item = *storage.get();
    construct(&item);
    RequireDefaults(item);
    Require(!item.urlString.impl && !item.originalURLString.impl && !item.title.impl && !item.displayTitle.impl);
    Require(item.identifier.object == before + 1);
    Require(item.identifier.processIdentifier == _ZN7WebCore7Process10identifierEv());
    Require(NKWebKit::WtfAbi::GenerateObjectIdentifier() == before + 2);
    Require(item.documentSequenceNumber == item.itemSequenceNumber + 1);
    _ZN7WebCore11HistoryItemD1Ev(&item);
}

static void TestTwoStringConstructor(TwoStringConstructor construct) {
    Text url("http://a/");
    Text title("title");
    Storage storage;
    HistoryItem& item = *storage.get();
    const std::uint64_t before = NKWebKit::WtfAbi::GenerateObjectIdentifier();
    construct(&item, &url.owner, &title.owner);
    RequireDefaults(item);
    Require(item.urlString.impl == &url.impl && item.originalURLString.impl == &url.impl);
    Require(item.title.impl == &title.impl && !item.displayTitle.impl);
    Require(url.refs() == 8 && title.refs() == 6);
    Require(item.identifier.object == before + 1);
    Require(item.identifier.processIdentifier == _ZN7WebCore7Process10identifierEv());
    Require(item.documentSequenceNumber == item.itemSequenceNumber + 1);
    _ZN7WebCore11HistoryItemD1Ev(&item);
    Require(url.refs() == 4 && title.refs() == 4);
}

static void TestThreeStringConstructor(ThreeStringConstructor construct) {
    Text url("http://a/");
    Text title("title");
    Text alternate("alternate");
    Storage storage;
    HistoryItem& item = *storage.get();
    const std::uint64_t before = NKWebKit::WtfAbi::GenerateObjectIdentifier();
    construct(&item, &url.owner, &title.owner, &alternate.owner);
    RequireDefaults(item);
    Require(item.urlString.impl == &url.impl && item.originalURLString.impl == &url.impl);
    Require(item.title.impl == &title.impl && item.displayTitle.impl == &alternate.impl);
    Require(url.refs() == 8 && title.refs() == 6 && alternate.refs() == 6);
    Require(item.identifier.object == before + 1);
    Require(item.identifier.processIdentifier == _ZN7WebCore7Process10identifierEv());
    Require(item.documentSequenceNumber == item.itemSequenceNumber + 1);
    _ZN7WebCore11HistoryItemD1Ev(&item);
    Require(url.refs() == 4 && title.refs() == 4 && alternate.refs() == 4);
}

static void TestConstructors() {
    TestNoStringConstructor(&_ZN7WebCore11HistoryItemC1Ev);
    TestNoStringConstructor(&_ZN7WebCore11HistoryItemC2Ev);
    TestTwoStringConstructor(&_ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_);
    TestTwoStringConstructor(&_ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_);
    TestThreeStringConstructor(&_ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_);
    TestThreeStringConstructor(&_ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_);

    Storage first;
    Storage second;
    _ZN7WebCore11HistoryItemC1Ev(first.get());
    _ZN7WebCore11HistoryItemC1Ev(second.get());
    Require(second.get()->itemSequenceNumber == first.get()->documentSequenceNumber + 1);
    Require(second.get()->identifier.object == first.get()->identifier.object + 1);
    Require(second.get()->identifier.processIdentifier == first.get()->identifier.processIdentifier);
    _ZN7WebCore11HistoryItemD1Ev(first.get());
    _ZN7WebCore11HistoryItemD1Ev(second.get());
}

static void TestDestructor() {
    Text url("http://a/");
    Text title("title");
    Text referrer("referrer");
    Text target("target");
    Text contentType("text/plain");
    Text stateOne("one");
    Text stateTwo("two");
    Text childUrl("http://child/");
    Text childTitle("child");
    Text grandUrl("http://grand/");
    Text grandTitle("grand");
    Text sharedUrl("http://shared/");
    Text sharedTitle("shared");

    Storage storage;
    HistoryItem& item = *storage.get();
    _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(&item, &url.owner, &title.owner);
    _ZN7WebCore11HistoryItem11setReferrerERKN3WTF6StringE(&item, &referrer.owner);
    item.target = String(target.owner);
    _ZN7WebCore11HistoryItem18setFormContentTypeERKN3WTF6StringE(&item, &contentType.owner);
    SetDocumentState(item, {&stateOne, &stateTwo, &stateOne});

    auto* child = NewChild(childUrl, childTitle);
    auto* grandchild = NewChild(grandUrl, grandTitle);
    SetChildren(*child, {grandchild});
    auto* survivor = NewChild(sharedUrl, sharedTitle);
    survivor->refCount = 2;
    SetChildren(item, {child, survivor});

    Require(url.refs() == 8 && referrer.refs() == 6 && target.refs() == 6 && contentType.refs() == 6);
    Require(stateOne.refs() == 8 && stateTwo.refs() == 6);
    Require(childUrl.refs() == 8 && grandUrl.refs() == 8 && sharedUrl.refs() == 8);

    _ZN7WebCore11HistoryItemD1Ev(&item);
    Require(url.refs() == 4 && title.refs() == 4 && referrer.refs() == 4 && target.refs() == 4 && contentType.refs() == 4);
    Require(stateOne.refs() == 4 && stateTwo.refs() == 4);
    Require(childUrl.refs() == 4 && childTitle.refs() == 4 && grandUrl.refs() == 4 && grandTitle.refs() == 4);
    Require(sharedUrl.refs() == 8 && sharedTitle.refs() == 6);
    Require(survivor->refCount == 1);
    Require(!item.documentState.buffer && !item.documentState.size && !item.children.buffer && !item.children.size);
    Require(!item.urlString.impl && !item.originalURLString.impl && !item.title.impl && !item.referrer.impl && !item.target.impl && !item.formContentType.impl);

    _ZN7WebCore11HistoryItemD2Ev(survivor);
    _ZdlPv_nid_postfix(survivor);
    Require(sharedUrl.refs() == 4 && sharedTitle.refs() == 4);

    Storage twice;
    _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(twice.get(), &url.owner, &title.owner);
    auto* repeated = NewChild(childUrl, childTitle);
    repeated->refCount = 2;
    SetChildren(*twice.get(), {repeated, repeated});
    _ZN7WebCore11HistoryItemD2Ev(twice.get());
    Require(url.refs() == 4 && title.refs() == 4 && childUrl.refs() == 4 && childTitle.refs() == 4);
}

static void TestDestructorPreconditions() {
    Text url("http://a/");
    Text title("title");
    Text childUrl("http://child/");
    Text childTitle("child");
    int marker = 0;
    CachedPage page{nullptr, 0};

    auto expectRefusal = [&](HistoryItem& item, auto&& unrefuse) {
        unsigned char before[sizeof(HistoryItem)];
        Snapshot(item, before);
        const unsigned urlRefs = url.refs();
        const unsigned childRefs = childUrl.refs();
        Require(Throws<std::logic_error>([&] { _ZN7WebCore11HistoryItemD1Ev(&item); }));
        Require(Throws<std::logic_error>([&] { _ZN7WebCore11HistoryItemD2Ev(&item); }));
        Require(Unchanged(item, before));
        Require(url.refs() == urlRefs && childUrl.refs() == childRefs);
        unrefuse();
        _ZN7WebCore11HistoryItemD1Ev(&item);
        Require(url.refs() == 4 && title.refs() == 4 && childUrl.refs() == 4 && childTitle.refs() == 4);
    };

    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        storage.get()->stateObject = &marker;
        expectRefusal(*storage.get(), [&] { storage.get()->stateObject = nullptr; });
    }
    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        storage.get()->formData = &marker;
        expectRefusal(*storage.get(), [&] { storage.get()->formData = nullptr; });
    }
    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        storage.get()->cachedPage = &page;
        expectRefusal(*storage.get(), [&] { storage.get()->cachedPage = nullptr; });
    }
    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        storage.get()->policyContainerEngaged = true;
        expectRefusal(*storage.get(), [&] { storage.get()->policyContainerEngaged = false; });
    }
    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        auto* child = NewChild(childUrl, childTitle);
        child->formData = &marker;
        SetChildren(*storage.get(), {child});
        expectRefusal(*storage.get(), [&] { child->formData = nullptr; });
    }
    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        auto* child = NewChild(childUrl, childTitle);
        auto* grandchild = NewChild(childUrl, childTitle);
        grandchild->stateObject = &marker;
        SetChildren(*child, {grandchild});
        SetChildren(*storage.get(), {child});
        expectRefusal(*storage.get(), [&] { grandchild->stateObject = nullptr; });
    }
    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        auto* child = NewChild(childUrl, childTitle);
        child->formData = &marker;
        child->refCount = 2;
        SetChildren(*storage.get(), {child, child});
        expectRefusal(*storage.get(), [&] { child->formData = nullptr; });
    }
    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        auto* survivor = NewChild(childUrl, childTitle);
        survivor->refCount = 2;
        survivor->formData = &marker;
        SetChildren(*storage.get(), {survivor});
        _ZN7WebCore11HistoryItemD1Ev(storage.get());
        Require(survivor->refCount == 1 && survivor->formData == &marker);
        survivor->formData = nullptr;
        _ZN7WebCore11HistoryItemD1Ev(survivor);
        _ZdlPv_nid_postfix(survivor);
        Require(url.refs() == 4 && childUrl.refs() == 4);
    }
}

static void TestDestructorAtoms() {
    Text atom("atom", 4 | 32);
    Text symbol("symbol", 4 | 16);

    auto refusesAtom = [&](Text& held, auto&& attach) {
        held.impl.refCount = 2;
        Storage storage;
        _ZN7WebCore11HistoryItemC1Ev(storage.get());
        attach(*storage.get());
        unsigned char before[sizeof(HistoryItem)];
        Snapshot(*storage.get(), before);
        Require(Throws<std::logic_error>([&] { _ZN7WebCore11HistoryItemD1Ev(storage.get()); }));
        Require(Unchanged(*storage.get(), before));
        Require(held.refs() == 2);
        held.impl.refCount = 4;
        _ZN7WebCore11HistoryItemD1Ev(storage.get());
        Require(held.refs() == 2);
        held.impl.refCount = 4;
    };

    refusesAtom(atom, [&](HistoryItem& item) { item.referrer.impl = &atom.impl; });
    refusesAtom(symbol, [&](HistoryItem& item) { item.target.impl = &symbol.impl; });
    refusesAtom(atom, [&](HistoryItem& item) { SetDocumentState(item, {&atom}); atom.impl.refCount = 2; });

    struct Substring {
        StringImpl impl;
        StringImpl* owner;
    };
    Substring substring{{2, 3, "sub", 4 | 2}, &atom.impl};
    atom.impl.refCount = 2;
    Storage storage;
    _ZN7WebCore11HistoryItemC1Ev(storage.get());
    storage.get()->title.impl = &substring.impl;
    unsigned char before[sizeof(HistoryItem)];
    Snapshot(*storage.get(), before);
    Require(Throws<std::logic_error>([&] { _ZN7WebCore11HistoryItemD1Ev(storage.get()); }));
    Require(Unchanged(*storage.get(), before) && substring.impl.refCount == 2 && atom.refs() == 2);
    storage.get()->title.impl = nullptr;
    atom.impl.refCount = 4;

    Storage released;
    _ZN7WebCore11HistoryItemC1Ev(released.get());
    released.get()->referrer.impl = &atom.impl;
    _ZN7WebCore11HistoryItemD1Ev(released.get());
    Require(atom.refs() == 2);
    atom.impl.refCount = 4;
}

static void TestCopy() {
    Text url("http://a/");
    Text original("http://o/");
    Text title("title");
    Text alternate("alternate");
    Text referrer("referrer");
    Text target("target");
    Text contentType("text/plain");
    Text state("state");
    Text childUrl("http://child/");
    Text childTitle("child");
    Text grandUrl("http://grand/");
    Text grandTitle("grand");
    int marker = 0;
    CachedPage page{nullptr, 0};

    Storage storage;
    HistoryItem& source = *storage.get();
    _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_(&source, &url.owner, &title.owner, &alternate.owner);
    _ZN7WebCore11HistoryItem20setOriginalURLStringERKN3WTF6StringE(&source, &original.owner);
    _ZN7WebCore11HistoryItem11setReferrerERKN3WTF6StringE(&source, &referrer.owner);
    source.target = String(target.owner);
    _ZN7WebCore11HistoryItem18setFormContentTypeERKN3WTF6StringE(&source, &contentType.owner);
    IntPoint point{-4, 11};
    _ZN7WebCore11HistoryItem17setScrollPositionERKNS_8IntPointE(&source, &point);
    _ZN7WebCore11HistoryItem18setPageScaleFactorEf(&source, 1.25f);
    source.lastVisitWasFailure = true;
    source.isTargetItem = true;
    source.itemSequenceNumber = 41;
    source.documentSequenceNumber = 77;
    SetDocumentState(source, {&state});
    source.shouldOpenExternalURLsPolicy = 2;
    source.wasRestoredFromSession = true;
    source.wasCreatedByJSWithoutUserInteraction = true;
    source.shouldRestoreScrollPosition = false;
    source.pruningReason = 3;
    source.stateObject = &marker;
    source.cachedPage = &page;
    source.policyContainerEngaged = true;

    auto* child = NewChild(childUrl, childTitle);
    auto* grandchild = NewChild(grandUrl, grandTitle);
    SetChildren(*child, {grandchild});
    SetChildren(source, {child});
    child->itemSequenceNumber = 5;
    child->documentSequenceNumber = 6;
    grandchild->itemSequenceNumber = 7;
    grandchild->documentSequenceNumber = 8;

    unsigned char before[sizeof(HistoryItem)];
    Snapshot(source, before);
    const unsigned urlRefs = url.refs();
    const std::size_t live = guestAllocations.size();
    {
        auto copy = _ZNK7WebCore11HistoryItem4copyEv(&source);
        Require(guestAllocations.size() == live + 3);
        const HistoryItem& result = *copy;
        Require(Unchanged(source, before));
        Require(&result != &source && result.refCount == 1);
        Require(result.urlString.impl == &url.impl && result.originalURLString.impl == &original.impl);
        Require(result.referrer.impl == &referrer.impl && result.target.impl == &target.impl);
        Require(result.title.impl == &title.impl && result.displayTitle.impl == &alternate.impl);
        Require(result.formContentType.impl == &contentType.impl);
        Require(url.refs() == urlRefs + 2 && original.refs() == 8 && title.refs() == 8 && alternate.refs() == 8);
        Require(referrer.refs() == 8 && target.refs() == 8 && contentType.refs() == 8);
        Require(result.scrollPosition.m_x == -4 && result.scrollPosition.m_y == 11 && result.pageScaleFactor == 1.25f);
        Require(result.lastVisitWasFailure && result.isTargetItem);
        Require(result.itemSequenceNumber == 41 && result.documentSequenceNumber == 77);
        Require(result.identifier.object == source.identifier.object && result.identifier.processIdentifier == source.identifier.processIdentifier);

        Require(!result.documentState.buffer && !result.documentState.size && state.refs() == 6);
        Require(result.shouldOpenExternalURLsPolicy == 0 && !result.wasRestoredFromSession && !result.wasCreatedByJSWithoutUserInteraction);
        Require(result.shouldRestoreScrollPosition);
        Require(result.pruningReason == 0);
        Require(!result.stateObject && !result.formData && !result.cachedPage && !result.policyContainerEngaged);

        Require(result.children.size == 1 && result.children.capacity == 1);
        auto* copiedChild = static_cast<HistoryItem**>(result.children.buffer)[0];
        Require(copiedChild != child && copiedChild->refCount == 1 && child->refCount == 1);
        Require(copiedChild->urlString.impl == &childUrl.impl && copiedChild->title.impl == &childTitle.impl);
        Require(copiedChild->itemSequenceNumber == 5 && copiedChild->documentSequenceNumber == 6);
        Require(childUrl.refs() == 12);
        Require(copiedChild->children.size == 1);
        auto* copiedGrandchild = static_cast<HistoryItem**>(copiedChild->children.buffer)[0];
        Require(copiedGrandchild != grandchild && copiedGrandchild->refCount == 1);
        Require(copiedGrandchild->urlString.impl == &grandUrl.impl && copiedGrandchild->itemSequenceNumber == 7 && copiedGrandchild->documentSequenceNumber == 8);
        Require(!copiedGrandchild->children.buffer);

        {
            auto shared = copy;
            Require(shared.get() == copy.get() && copy->refCount == 2);
        }
        Require(copy->refCount == 1);
    }
    Require(url.refs() == urlRefs && original.refs() == 6 && title.refs() == 6 && alternate.refs() == 6);
    Require(referrer.refs() == 6 && target.refs() == 6 && contentType.refs() == 6 && state.refs() == 6);
    Require(childUrl.refs() == 8 && childTitle.refs() == 6 && grandUrl.refs() == 8 && grandTitle.refs() == 6);
    Require(Unchanged(source, before));
    Require(guestAllocations.size() == live);

    source.formData = &marker;
    Snapshot(source, before);
    Require(Throws<std::logic_error>([&] { _ZNK7WebCore11HistoryItem4copyEv(&source); }));
    Require(Unchanged(source, before) && url.refs() == urlRefs && guestAllocations.size() == live);
    source.formData = nullptr;

    grandchild->formData = &marker;
    Snapshot(source, before);
    Require(Throws<std::logic_error>([&] { _ZNK7WebCore11HistoryItem4copyEv(&source); }));
    Require(Unchanged(source, before));
    Require(url.refs() == urlRefs && childUrl.refs() == 8 && childTitle.refs() == 6 && grandUrl.refs() == 8);
    Require(guestAllocations.size() == live);
    grandchild->formData = nullptr;

    source.stateObject = nullptr;
    source.cachedPage = nullptr;
    source.policyContainerEngaged = false;
    _ZN7WebCore11HistoryItemD1Ev(&source);
    Require(url.refs() == 4 && state.refs() == 4 && childUrl.refs() == 4 && grandUrl.refs() == 4);
}

static void FreeChildren(HistoryItem& item) {
    if (item.children.buffer) _ZN3WTF8fastFreeEPv(item.children.buffer);
    item.children = {};
}

static HistoryItem* BuildChain(Text& url, Text& title, unsigned length) {
    HistoryItem* leaf = NewChild(url, title);
    HistoryItem* head = leaf;
    for (unsigned index = 1; index < length; ++index) {
        HistoryItem* parent = NewChild(url, title);
        SetChildren(*parent, {head});
        head = parent;
    }
    return head;
}

static unsigned ChainLength(const HistoryItem* head) {
    unsigned length = 1;
    while (head->children.size) {
        head = static_cast<HistoryItem* const*>(head->children.buffer)[0];
        ++length;
    }
    return length;
}

static void TestCopyBounds() {
    Text url("http://a/");
    Text title("title");
    const std::size_t live = guestAllocations.size();

    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        SetChildren(*storage.get(), {storage.get()});
        unsigned char before[sizeof(HistoryItem)];
        Snapshot(*storage.get(), before);
        Require(Throws<std::logic_error>([&] { _ZNK7WebCore11HistoryItem4copyEv(storage.get()); }));
        Require(Unchanged(*storage.get(), before) && guestAllocations.size() == live && url.refs() == 8 && title.refs() == 6);
        FreeChildren(*storage.get());
        _ZN7WebCore11HistoryItemD1Ev(storage.get());
    }
    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        auto* child = NewChild(url, title);
        child->refCount = 2;
        SetChildren(*storage.get(), {child, child});
        Require(Throws<std::logic_error>([&] { _ZNK7WebCore11HistoryItem4copyEv(storage.get()); }));
        Require(guestAllocations.size() == live + 1 && child->refCount == 2);
        _ZN7WebCore11HistoryItemD1Ev(storage.get());
        Require(guestAllocations.size() == live && url.refs() == 4 && title.refs() == 4);
    }
    {
        HistoryItem* wide = NewChild(url, title);
        HistoryItem* level = NewChild(url, title);
        SetChildren(*wide, {level});
        for (unsigned index = 0; index < 40; ++index) {
            auto* next = NewChild(url, title);
            next->refCount = 2;
            SetChildren(*level, {next, next});
            level = next;
        }
        Require(Throws<std::logic_error>([&] { _ZNK7WebCore11HistoryItem4copyEv(wide); }));
        Require(guestAllocations.size() == live + 42);
        HistoryItem* node = wide;
        while (node->children.size) {
            auto* next = static_cast<HistoryItem**>(node->children.buffer)[0];
            FreeChildren(*node);
            if (node != wide) node->refCount = 1;
            _ZN7WebCore11HistoryItemD1Ev(node);
            _ZdlPv_nid_postfix(node);
            node = next;
        }
        _ZN7WebCore11HistoryItemD1Ev(node);
        _ZdlPv_nid_postfix(node);
        Require(guestAllocations.size() == live);
        Require(url.refs() == 4 && title.refs() == 4);
    }
    {
        HistoryItem* chain = BuildChain(url, title, 4097);
        Require(ChainLength(chain) == 4097);
        {
            auto copy = _ZNK7WebCore11HistoryItem4copyEv(chain);
            Require(ChainLength(copy.get()) == 4097 && guestAllocations.size() == live + 2 * 4097);
        }
        Require(guestAllocations.size() == live + 4097);
        _ZN7WebCore11HistoryItemD1Ev(chain);
        _ZdlPv_nid_postfix(chain);
        Require(guestAllocations.size() == live && url.refs() == 4);
    }
    {
        HistoryItem* chain = BuildChain(url, title, 4098);
        Require(Throws<std::logic_error>([&] { _ZNK7WebCore11HistoryItem4copyEv(chain); }));
        Require(guestAllocations.size() == live + 4098);
        _ZN7WebCore11HistoryItemD1Ev(chain);
        _ZdlPv_nid_postfix(chain);
        Require(guestAllocations.size() == live && url.refs() == 4 && title.refs() == 4);
    }
}

static void TestCopyFailureCleanup() {
    Text url("http://a/");
    Text title("title");
    const std::size_t live = guestAllocations.size();
    static int marker;

    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        auto* first = NewChild(url, title);
        auto* firstLeaf = NewChild(url, title);
        SetChildren(*first, {firstLeaf});
        auto* second = NewChild(url, title);
        auto* secondLeaf = NewChild(url, title);
        auto* failing = NewChild(url, title);
        SetChildren(*second, {secondLeaf, failing});
        SetChildren(*storage.get(), {first, second});
        const std::size_t built = guestAllocations.size();
        const unsigned urlRefs = url.refs();
        const unsigned titleRefs = title.refs();
        unsigned char before[sizeof(HistoryItem)];
        Snapshot(*storage.get(), before);

        failing->formData = &marker;
        Require(Throws<std::logic_error>([&] { _ZNK7WebCore11HistoryItem4copyEv(storage.get()); }));
        Require(guestAllocations.size() == built && url.refs() == urlRefs && title.refs() == titleRefs);
        Require(Unchanged(*storage.get(), before));

        failing->formData = nullptr;
        _ZN7WebCore11HistoryItemD1Ev(storage.get());
        Require(guestAllocations.size() == live && url.refs() == 4 && title.refs() == 4);
    }
    {
        Storage storage;
        _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(storage.get(), &url.owner, &title.owner);
        auto* child = NewChild(url, title);
        auto* failing = NewChild(url, title);
        SetChildren(*child, {failing});
        SetChildren(*storage.get(), {child});
        const std::size_t built = guestAllocations.size();
        const unsigned urlRefs = url.refs();

        failing->formData = &marker;
        Require(Throws<std::logic_error>([&] { _ZNK7WebCore11HistoryItem4copyEv(storage.get()); }));
        Require(guestAllocations.size() == built && url.refs() == urlRefs);

        failing->formData = nullptr;
        _ZN7WebCore11HistoryItemD1Ev(storage.get());
        Require(guestAllocations.size() == live && url.refs() == 4 && title.refs() == 4);
    }
}

static void TestLifecycleNull() {
    Text text("text");
    Storage storage;
    unsigned char pattern[sizeof(HistoryItem)];
    Snapshot(*storage.get(), pattern);

    Require(Throws<std::invalid_argument>([] { _ZN7WebCore11HistoryItemC1Ev(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore11HistoryItemC2Ev(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(nullptr, &text.owner, &text.owner); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_(nullptr, &text.owner, &text.owner); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_(nullptr, &text.owner, &text.owner, &text.owner); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_(nullptr, &text.owner, &text.owner, &text.owner); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore11HistoryItemD1Ev(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN7WebCore11HistoryItemD2Ev(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZNK7WebCore11HistoryItem4copyEv(nullptr); }));

    HistoryItem* item = storage.get();
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_(item, nullptr, &text.owner); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_(item, &text.owner, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_(item, nullptr, &text.owner, &text.owner); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_(item, &text.owner, nullptr, &text.owner); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC1ERKN3WTF6StringES4_S4_(item, &text.owner, &text.owner, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { _ZN7WebCore11HistoryItemC2ERKN3WTF6StringES4_S4_(item, &text.owner, &text.owner, nullptr); }));
    Require(Unchanged(*item, pattern));
    Require(text.refs() == 4);
}

int main() {
    void* api[10] = {reinterpret_cast<void*>(&Allocate_nid_no_patch), reinterpret_cast<void*>(&Free_nid_no_patch),
        reinterpret_cast<void*>(&Calloc_nid_no_patch), reinterpret_cast<void*>(&Reallocate_nid_no_patch),
        reinterpret_cast<void*>(&Align_nid_no_patch), reinterpret_cast<void*>(&Realign_nid_no_patch), reinterpret_cast<void*>(&PosixAlign_nid_no_patch)};
    ApplicationHeapRegister_nid_no_patch(api);
    TestStrings();
    TestScalars();
    TestContainers();
    TestCachedPageExpiry();
    TestNull();
    TestChangedHook();
    TestTarget();
    TestConstructors();
    TestDestructor();
    TestDestructorPreconditions();
    TestDestructorAtoms();
    TestCopy();
    TestCopyBounds();
    TestCopyFailureCleanup();
    TestLifecycleNull();
    Require(guestAllocations.empty());
    return 0;
}

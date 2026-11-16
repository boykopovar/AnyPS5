#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <new>
#include <source_location>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringVector;

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WTF language check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireThrows(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw);
}

using AddObserver = void (APS5_VABI *)(void*, void (APS5_VABI *)(void*));
using RemoveObserver = void (APS5_VABI *)(void*);
using LanguageDidChange = void (APS5_VABI *)();
using DisplayName = String (APS5_VABI *)(const String*);
using OverrideLanguages = void (APS5_VABI *)(const StringVector*);
using PreferredLanguagesOverride = StringVector (APS5_VABI *)();
using IndexOfBestMatch = std::size_t (APS5_VABI *)(const String*, const StringVector*, bool*);

static_assert(std::is_same_v<decltype(&_ZN3WTF17languageDidChangeEv), LanguageDidChange>);
static_assert(std::is_same_v<decltype(&_ZN3WTF25addLanguageChangeObserverEPvPFvS0_E), AddObserver>);
static_assert(std::is_same_v<decltype(&_ZN3WTF28displayNameForLanguageLocaleERKNS_6StringE), DisplayName>);
static_assert(std::is_same_v<decltype(&_ZN3WTF28removeLanguageChangeObserverEPv), RemoveObserver>);
static_assert(std::is_same_v<decltype(&_ZN3WTF30overrideUserPreferredLanguagesERKNS_6VectorINS_6StringELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE), OverrideLanguages>);
static_assert(std::is_same_v<decltype(&_ZN3WTF30userPreferredLanguagesOverrideEv), PreferredLanguagesOverride>);
static_assert(std::is_same_v<decltype(&_ZN3WTF33indexOfBestMatchingLanguageInListERKNS_6StringERKNS_6VectorIS0_Lm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEERb), IndexOfBestMatch>);

struct Text {
    const void* data;
    unsigned length;
    bool is8Bit;
};

Text Text8(const char* text) {
    return {text, static_cast<unsigned>(std::strlen(text)), true};
}

Text Text16(const char16_t* text, unsigned length) {
    return {text, length, false};
}

StringImpl Make(const Text& text) {
    return StringImpl{2, text.length, text.data, text.is8Bit ? 4U : 0U};
}

String MakeString(StringImpl& impl) {
    impl.refCount += 2;
    return String(&impl);
}

StringVector MakeVector(const std::vector<StringImpl*>& impls) {
    StringVector vector;
    if (impls.empty()) return vector;
    vector.buffer = static_cast<String*>(_ZN3WTF10fastMallocEm(impls.size() * sizeof(String)));
    vector.capacity = static_cast<unsigned>(impls.size());
    while (vector.size < vector.capacity) {
        StringImpl* impl = impls[vector.size];
        if (impl) impl->refCount += 2;
        new (&vector.buffer[vector.size]) String(impl);
        ++vector.size;
    }
    return vector;
}

int firstCalls;
int secondCalls;
int thirdCalls;
int replacementCalls;
int removingCalls;
int addingCalls;
int replacingCalls;
void* firstContext;
void* secondContext;
void* thirdContext;
void* contextToRemove;
void* contextToAdd;
void* contextToReplace;
StringImpl* firstStoredLanguage;
StringImpl* secondStoredLanguage;
bool observedStoredLanguages;
int overrideObserverCalls;

void APS5_VABI First(void* context) {
    ++firstCalls;
    firstContext = context;
}

void APS5_VABI Second(void* context) {
    ++secondCalls;
    secondContext = context;
}

void APS5_VABI Third(void* context) {
    ++thirdCalls;
    thirdContext = context;
}

void APS5_VABI Replacement(void*) {
    ++replacementCalls;
}

void APS5_VABI RemoveDuringDispatch(void*) {
    ++removingCalls;
    _ZN3WTF28removeLanguageChangeObserverEPv(contextToRemove);
}

void APS5_VABI AddDuringDispatch(void*) {
    ++addingCalls;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(contextToAdd, Third);
}

void APS5_VABI ReplaceDuringDispatch(void*) {
    ++replacingCalls;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(contextToReplace, Replacement);
}

void APS5_VABI ObserveStoredOverride(void*) {
    ++overrideObserverCalls;
    StringVector copy = _ZN3WTF30userPreferredLanguagesOverrideEv();
    observedStoredLanguages = copy.size == 2 && copy.buffer[0].impl == firstStoredLanguage && copy.buffer[1].impl == secondStoredLanguage;
}

void ClearCounters() {
    firstCalls = secondCalls = thirdCalls = replacementCalls = removingCalls = addingCalls = replacingCalls = 0;
    firstContext = secondContext = thirdContext = nullptr;
}

void ClearOverride() {
    StringVector empty;
    _ZN3WTF30overrideUserPreferredLanguagesERKNS_6VectorINS_6StringELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(&empty);
}

void TestTwoObservers() {
    ClearCounters();
    int a = 0;
    int b = 0;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, First);
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&b, Second);
    _ZN3WTF17languageDidChangeEv();
    Require(firstCalls == 1 && firstContext == &a);
    Require(secondCalls == 1 && secondContext == &b);
    _ZN3WTF28removeLanguageChangeObserverEPv(&a);
    _ZN3WTF28removeLanguageChangeObserverEPv(&b);
    ClearOverride();
    std::printf("WTF language observers: two contexts called once each with their own context\n");
}

void TestReplacedRegistration() {
    ClearCounters();
    int a = 0;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, First);
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, Second);
    _ZN3WTF17languageDidChangeEv();
    Require(firstCalls == 0);
    Require(secondCalls == 1 && secondContext == &a);
    _ZN3WTF28removeLanguageChangeObserverEPv(&a);
    ClearOverride();
    std::printf("WTF language observers: adding a registered context replaces its function\n");
}

void TestRemovedRegistration() {
    ClearCounters();
    int a = 0;
    int b = 0;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, First);
    _ZN3WTF28removeLanguageChangeObserverEPv(&a);
    _ZN3WTF17languageDidChangeEv();
    Require(firstCalls == 0);
    _ZN3WTF28removeLanguageChangeObserverEPv(&b);
    ClearOverride();
    std::printf("WTF language observers: removed contexts are not called and removing an absent context returns\n");
}

void TestRemovedDuringDispatch() {
    ClearCounters();
    int a = 0;
    int b = 0;
    contextToRemove = &b;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, RemoveDuringDispatch);
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&b, Second);
    _ZN3WTF17languageDidChangeEv();
    Require(removingCalls == 1);
    Require(secondCalls == 0);
    _ZN3WTF28removeLanguageChangeObserverEPv(&a);
    ClearOverride();
    std::printf("WTF language observers: an observer removed during dispatch is not called\n");
}

void TestAddedDuringDispatch() {
    ClearCounters();
    int a = 0;
    int c = 0;
    contextToAdd = &c;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, AddDuringDispatch);
    _ZN3WTF17languageDidChangeEv();
    Require(addingCalls == 1);
    Require(thirdCalls == 0);
    _ZN3WTF17languageDidChangeEv();
    Require(thirdCalls == 1 && thirdContext == &c);
    _ZN3WTF28removeLanguageChangeObserverEPv(&a);
    _ZN3WTF28removeLanguageChangeObserverEPv(&c);
    ClearOverride();
    std::printf("WTF language observers: an observer added during dispatch is called from the next dispatch\n");
}

void TestReplacedDuringDispatch() {
    ClearCounters();
    int a = 0;
    int b = 0;
    contextToReplace = &b;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, ReplaceDuringDispatch);
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&b, Second);
    _ZN3WTF17languageDidChangeEv();
    Require(secondCalls == 1);
    Require(replacementCalls == 0);
    _ZN3WTF17languageDidChangeEv();
    Require(secondCalls == 1);
    Require(replacementCalls == 1);
    _ZN3WTF28removeLanguageChangeObserverEPv(&a);
    _ZN3WTF28removeLanguageChangeObserverEPv(&b);
    ClearOverride();
    std::printf("WTF language observers: a replaced observer keeps its old function for the running dispatch\n");
}

void TestInvalidContexts() {
    ClearCounters();
    RequireThrows([] { _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(nullptr, First); });
    RequireThrows([] { _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(reinterpret_cast<void*>(-1), First); });
    RequireThrows([] { _ZN3WTF28removeLanguageChangeObserverEPv(nullptr); });
    RequireThrows([] { _ZN3WTF28removeLanguageChangeObserverEPv(reinterpret_cast<void*>(-1)); });
    _ZN3WTF17languageDidChangeEv();
    Require(firstCalls == 0);
    ClearOverride();
    std::printf("WTF language observers: null and -1 contexts throw on add and remove and register nothing\n");
}

void TestNullObserver() {
    ClearCounters();
    int a = 0;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, nullptr);
    RequireThrows([] { _ZN3WTF17languageDidChangeEv(); });
    _ZN3WTF28removeLanguageChangeObserverEPv(&a);
    _ZN3WTF17languageDidChangeEv();
    Require(firstCalls == 0);
    ClearOverride();
    std::printf("WTF language observers: a null function throws when dispatched\n");
}

void TestOverrideNotifies() {
    ClearCounters();
    int a = 0;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, First);
    ClearOverride();
    Require(firstCalls == 1 && firstContext == &a);
    _ZN3WTF28removeLanguageChangeObserverEPv(&a);
    ClearOverride();
    std::printf("WTF language observers: overriding the preferred languages runs observers once\n");
}

void TestOverrideNotifiesAfterStore() {
    ClearCounters();
    overrideObserverCalls = 0;
    observedStoredLanguages = false;
    StringImpl en = Make(Text8("en-US"));
    StringImpl fr = Make(Text8("fr-FR"));
    firstStoredLanguage = &en;
    secondStoredLanguage = &fr;
    int a = 0;
    _ZN3WTF25addLanguageChangeObserverEPvPFvS0_E(&a, ObserveStoredOverride);
    {
        std::vector<StringImpl*> impls{&en, &fr};
        StringVector list = MakeVector(impls);
        _ZN3WTF30overrideUserPreferredLanguagesERKNS_6VectorINS_6StringELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(&list);
        Require(overrideObserverCalls == 1);
        Require(observedStoredLanguages);
    }
    _ZN3WTF28removeLanguageChangeObserverEPv(&a);
    ClearOverride();
    firstStoredLanguage = nullptr;
    secondStoredLanguage = nullptr;
    std::printf("WTF language observers: the override is stored before observers run, outside the lock\n");
}

void TestInitialOverride() {
    StringVector copy = _ZN3WTF30userPreferredLanguagesOverrideEv();
    Require(copy.buffer == nullptr);
    Require(copy.capacity == 0);
    Require(copy.size == 0);
    std::printf("WTF language override: no override is stored before the first call\n");
}

void TestOverride() {
    StringImpl en = Make(Text8("en-US"));
    StringImpl fr = Make(Text8("fr-FR"));
    {
        std::vector<StringImpl*> impls{&en, &fr};
        StringVector list = MakeVector(impls);
        Require(en.refCount == 4 && fr.refCount == 4);
        _ZN3WTF30overrideUserPreferredLanguagesERKNS_6VectorINS_6StringELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(&list);
        Require(en.refCount == 6 && fr.refCount == 6);
        {
            StringVector copy = _ZN3WTF30userPreferredLanguagesOverrideEv();
            Require(copy.size == 2);
            Require(copy.capacity == 2);
            Require(copy.buffer[0].impl == &en);
            Require(copy.buffer[1].impl == &fr);
            Require(en.refCount == 8 && fr.refCount == 8);
        }
        Require(en.refCount == 6 && fr.refCount == 6);
    }
    Require(en.refCount == 4 && fr.refCount == 4);

    ClearOverride();
    Require(en.refCount == 2 && fr.refCount == 2);
    {
        StringVector copy = _ZN3WTF30userPreferredLanguagesOverrideEv();
        Require(copy.buffer == nullptr);
        Require(copy.size == 0);
    }
    std::printf("WTF language override: copies retain and release the stored strings\n");
}

void CheckMatchResult(const String& language, const StringVector& list, std::size_t expectedIndex, bool expectedExact) {
    bool exactMatch = !expectedExact;
    std::size_t index = _ZN3WTF33indexOfBestMatchingLanguageInListERKNS_6StringERKNS_6VectorIS0_Lm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEERb(&language, &list, &exactMatch);
    Require(index == expectedIndex);
    Require(exactMatch == expectedExact);
}

void CheckMatch(const Text& language, std::initializer_list<Text> list, std::size_t expectedIndex, bool expectedExact) {
    StringImpl languageImpl = Make(language);
    std::vector<StringImpl> impls;
    impls.reserve(list.size());
    for (const auto& entry : list) impls.push_back(Make(entry));
    std::vector<StringImpl*> pointers;
    pointers.reserve(impls.size());
    for (auto& impl : impls) pointers.push_back(&impl);
    StringVector vector = MakeVector(pointers);
    String value = MakeString(languageImpl);
    CheckMatchResult(value, vector, expectedIndex, expectedExact);
}

void TestBestMatch() {
    CheckMatch(Text8("en-US"), {Text8("en-GB"), Text8("en"), Text8("fr")}, 1, false);
    CheckMatch(Text8("en-US"), {Text8("fr"), Text8("en-US")}, 1, true);
    CheckMatch(Text8("EN-us"), {Text8("en_US")}, 0, true);
    CheckMatch(Text8("en_US"), {Text8("en_US")}, 1, false);
    CheckMatch(Text8("en"), {Text8("en-GB"), Text8("en-AU")}, 0, false);
    CheckMatch(Text8("en"), {Text8("fr"), Text8("en")}, 1, true);
    CheckMatch(Text8("FR"), {Text8("fr-CA")}, 0, false);
    CheckMatch(Text8("en-US"), {Text8("en-GB"), Text8("fr"), Text8("en")}, 2, false);
    CheckMatch(Text8("en-US"), {Text8("en"), Text8("en")}, 0, false);
    CheckMatch(Text8("english"), {Text8("en")}, 1, false);
    CheckMatch(Text8("en-US"), {Text8("e")}, 1, false);
    CheckMatch(Text8("en-US"), {}, 0, false);
    CheckMatch(Text16(u"en-US", 5), {Text8("en-us")}, 0, true);
    CheckMatch(Text8("en-us"), {Text16(u"en_US", 5)}, 0, true);
    CheckMatch(Text16(u"\u00C9", 1), {Text16(u"\u00E9", 1)}, 1, false);

    {
        StringImpl empty = Make(Text8(""));
        std::vector<StringImpl*> impls{&empty};
        StringVector list = MakeVector(impls);
        String nullLanguage;
        CheckMatchResult(nullLanguage, list, 1, false);
        String emptyLanguage = MakeString(empty);
        CheckMatchResult(emptyLanguage, list, 0, true);
        {
            std::vector<StringImpl*> nulls{nullptr};
            StringVector nullList = MakeVector(nulls);
            CheckMatchResult(nullLanguage, nullList, 0, true);
            CheckMatchResult(emptyLanguage, nullList, 1, false);
        }
    }
    std::printf("WTF language matching: 19 language lists return the documented index and exact flag\n");
}

void TestDisplayName() {
    StringImpl name = Make(Text8("en-US"));
    String value = MakeString(name);
    Require(name.refCount == 4);
    {
        String display = _ZN3WTF28displayNameForLanguageLocaleERKNS_6StringE(&value);
        Require(display.impl == &name);
        Require(name.refCount == 6);
    }
    Require(name.refCount == 4);
    {
        String none;
        String display = _ZN3WTF28displayNameForLanguageLocaleERKNS_6StringE(&none);
        Require(display.impl == nullptr);
    }
    std::printf("WTF language display name: returns the retained argument and null for null\n");
}

void TestNullPointers() {
    StringVector list;
    String language;
    bool exactMatch = false;
    RequireThrows([&] { _ZN3WTF30overrideUserPreferredLanguagesERKNS_6VectorINS_6StringELm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEE(nullptr); });
    RequireThrows([&] { _ZN3WTF28displayNameForLanguageLocaleERKNS_6StringE(nullptr); });
    RequireThrows([&] { _ZN3WTF33indexOfBestMatchingLanguageInListERKNS_6StringERKNS_6VectorIS0_Lm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEERb(nullptr, &list, &exactMatch); });
    RequireThrows([&] { _ZN3WTF33indexOfBestMatchingLanguageInListERKNS_6StringERKNS_6VectorIS0_Lm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEERb(&language, nullptr, &exactMatch); });
    RequireThrows([&] { _ZN3WTF33indexOfBestMatchingLanguageInListERKNS_6StringERKNS_6VectorIS0_Lm0ENS_15CrashOnOverflowELm16ENS_10FastMallocEEERb(&language, &list, nullptr); });
    std::printf("WTF language: null pointers throw for override, matching and display name\n");
}

}

int main() {
    TestInitialOverride();
    TestTwoObservers();
    TestReplacedRegistration();
    TestRemovedRegistration();
    TestRemovedDuringDispatch();
    TestAddedDuringDispatch();
    TestReplacedDuringDispatch();
    TestInvalidContexts();
    TestNullObserver();
    TestOverrideNotifies();
    TestOverrideNotifiesAfterStore();
    TestOverride();
    TestBestMatch();
    TestDisplayName();
    TestNullPointers();
}

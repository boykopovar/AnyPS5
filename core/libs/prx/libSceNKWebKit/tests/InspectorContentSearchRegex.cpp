#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <stdexcept>
#include <string>

using Jsc::WtfAbi::StringImpl;
using Jsc::YarrAbi::RegularExpression;
using Jsc::YarrAbi::StringReference;

namespace {

const auto Create = &_ZN9Inspector22ContentSearchUtilities38createRegularExpressionForSearchStringERKN3WTF6StringEbNS0_16SearchStringTypeE;
const auto Count = &_ZN9Inspector22ContentSearchUtilities29countRegularExpressionMatchesERKN3JSC4Yarr17RegularExpressionERKN3WTF6StringE;
const auto Match = &_ZNK3JSC4Yarr17RegularExpression5matchERKN3WTF6StringEiPi;
const auto IsValid = &_ZNK3JSC4Yarr17RegularExpression7isValidEv;
const auto Destroy = &_ZN3JSC4Yarr17RegularExpressionD1Ev;

constexpr int Regex = NKWebKit::InspectorAbi::SearchStringTypeRegex;
constexpr int ExactString = NKWebKit::InspectorAbi::SearchStringTypeExactString;
constexpr int ContainsString = NKWebKit::InspectorAbi::SearchStringTypeContainsString;

std::set<void*> live;

void* APS5_VABI TestAllocate(std::size_t size) {
    void* pointer = std::malloc(size ? size : 1);
    if (pointer) live.insert(pointer);
    return pointer;
}

void APS5_VABI TestFree(void* pointer) {
    if (!pointer) return;
    if (live.erase(pointer) != 1) std::abort();
    std::free(pointer);
}

void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) {
    void* pointer = TestAllocate(count * size);
    if (pointer) std::memset(pointer, 0, count * size);
    return pointer;
}

void* APS5_VABI TestReallocate(void*, std::size_t) { std::abort(); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return TestAllocate(size); }
void* APS5_VABI TestRealign(void*, std::size_t, std::size_t) { std::abort(); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = TestAllocate(size);
    return *pointer ? 0 : 12;
}

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Inspector content search regex check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename TCall>
bool ThrowsInvalid(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

StringImpl Latin1(const std::string& text) {
    return StringImpl{2, static_cast<unsigned>(text.size()), text.data(), 4};
}

StringImpl Utf16(const std::u16string& text) {
    return StringImpl{2, static_cast<unsigned>(text.size()), text.data(), 0};
}

RegularExpression CreateFrom(const StringImpl* impl, bool caseSensitive, int type) {
    const StringReference reference{const_cast<StringImpl*>(impl)};
    RegularExpression result{};
    Require(Create(&result, &reference, caseSensitive, type) == &result);
    Require(result.d != nullptr);
    return result;
}

RegularExpression Make(const std::string& search, bool caseSensitive, int type) {
    const StringImpl impl = Latin1(search);
    return CreateFrom(&impl, caseSensitive, type);
}

int MatchIn(const RegularExpression& expression, const StringImpl& impl) {
    const StringReference reference{const_cast<StringImpl*>(&impl)};
    return Match(&expression, &reference, 0, nullptr);
}

int MatchIn(const RegularExpression& expression, const std::string& text) {
    return MatchIn(expression, Latin1(text));
}

int CountIn(const RegularExpression& expression, const StringImpl& impl) {
    const StringReference reference{const_cast<StringImpl*>(&impl)};
    return Count(&expression, &reference);
}

int CountIn(const RegularExpression& expression, const std::string& text) {
    return CountIn(expression, Latin1(text));
}

void TestSearchStringTypes() {
    RegularExpression contains = Make("a.b", true, ContainsString);
    Require(MatchIn(contains, "axb") == -1);
    Require(MatchIn(contains, "za.b") == 1);
    Require(MatchIn(contains, "a.bz") == 0);
    Destroy(&contains);

    RegularExpression exact = Make("a.b", true, ExactString);
    Require(MatchIn(exact, "a.b") == 0);
    Require(MatchIn(exact, "xa.b") == -1);
    Require(MatchIn(exact, "a.bx") == -1);
    Require(MatchIn(exact, "axb") == -1);
    Destroy(&exact);

    RegularExpression regex = Make("a.b", true, Regex);
    Require(MatchIn(regex, "zaxb") == 1);
    Require(MatchIn(regex, "a.b") == 0);
    Destroy(&regex);

    RegularExpression anchored = Make("^b+$", true, Regex);
    Require(MatchIn(anchored, "bbb") == 0);
    Require(MatchIn(anchored, "abbb") == -1);
    Destroy(&anchored);
}

void TestCaseSensitivity() {
    RegularExpression insensitive = Make("ABC", false, ContainsString);
    Require(MatchIn(insensitive, "xabc") == 1);
    Require(MatchIn(insensitive, "xAbC") == 1);
    Destroy(&insensitive);

    RegularExpression sensitive = Make("ABC", true, ContainsString);
    Require(MatchIn(sensitive, "xabc") == -1);
    Require(MatchIn(sensitive, "xABC") == 1);
    Destroy(&sensitive);

    RegularExpression exact = Make("Ab", false, ExactString);
    Require(MatchIn(exact, "aB") == 0);
    Require(MatchIn(exact, "aBc") == -1);
    Destroy(&exact);

    RegularExpression regex = Make("a+", false, Regex);
    Require(MatchIn(regex, "xxAA") == 2);
    Destroy(&regex);
}

void TestEscaping() {
    const std::string specials = "[](){}+-*.,?\\^$|";
    RegularExpression all = Make(specials, true, ContainsString);
    Require(IsValid(&all));
    Require(MatchIn(all, "x" + specials) == 1);
    Require(MatchIn(all, specials.substr(1)) == -1);
    Destroy(&all);

    RegularExpression exactAll = Make(specials, true, ExactString);
    Require(MatchIn(exactAll, specials) == 0);
    Require(MatchIn(exactAll, specials + "x") == -1);
    Destroy(&exactAll);

    for (const char special : specials) {
        RegularExpression single = Make(std::string(1, special), true, ContainsString);
        Require(IsValid(&single));
        Require(MatchIn(single, std::string("ab") + special) == 2);
        Require(MatchIn(single, "ab") == -1);
        Destroy(&single);
    }

    RegularExpression plain = Make("a/b:c'd\"e", true, ExactString);
    Require(MatchIn(plain, "a/b:c'd\"e") == 0);
    Destroy(&plain);

    RegularExpression unescaped = Make("(", true, Regex);
    Require(!IsValid(&unescaped));
    Require(MatchIn(unescaped, "(") == -1);
    Destroy(&unescaped);
}

void TestWideAndNullSearchStrings() {
    const std::u16string search = u"é.中";
    const StringImpl wide = Utf16(search);
    RegularExpression contains = CreateFrom(&wide, true, ContainsString);
    Require(MatchIn(contains, Utf16(u"xé.中")) == 1);
    Require(MatchIn(contains, Utf16(u"xéz中")) == -1);
    Destroy(&contains);

    const std::string latin = "\xe9.";
    RegularExpression latinExact = Make(latin, false, ExactString);
    Require(MatchIn(latinExact, Utf16(u"é.")) == 0);
    Require(MatchIn(latinExact, Utf16(u"éx")) == -1);
    Destroy(&latinExact);

    RegularExpression nullContains = CreateFrom(nullptr, true, ContainsString);
    Require(IsValid(&nullContains));
    Require(MatchIn(nullContains, "abc") == 0);
    Destroy(&nullContains);

    RegularExpression nullExact = CreateFrom(nullptr, true, ExactString);
    Require(MatchIn(nullExact, "") == 0);
    Require(MatchIn(nullExact, "abc") == -1);
    Destroy(&nullExact);

    RegularExpression nullRegex = CreateFrom(nullptr, true, Regex);
    Require(IsValid(&nullRegex));
    Require(MatchIn(nullRegex, "abc") == 0);
    Destroy(&nullRegex);

    const StringImpl broken{2, 3, nullptr, 4};
    for (const int type : {ExactString, ContainsString}) {
        Require(ThrowsInvalid([&] {
            const StringReference reference{const_cast<StringImpl*>(&broken)};
            RegularExpression result{};
            Create(&result, &reference, true, type);
        }));
    }
}

void TestCreateRejectsBadArguments() {
    const StringImpl impl = Latin1("a");
    const StringReference reference{const_cast<StringImpl*>(&impl)};
    for (const int type : {-1, 3, 255}) {
        RegularExpression result{};
        Require(ThrowsInvalid([&] { Create(&result, &reference, true, type); }));
        Require(result.d == nullptr);
    }
    RegularExpression result{};
    Require(ThrowsInvalid([&] { Create(nullptr, &reference, true, ContainsString); }));
    Require(ThrowsInvalid([&] { Create(&result, nullptr, true, ContainsString); }));
    Require(result.d == nullptr);
    Require(live.empty());
}

void TestCount() {
    RegularExpression a = Make("a", true, Regex);
    Require(CountIn(a, "banana") == 3);
    Require(CountIn(a, "bnn") == 0);
    Require(CountIn(a, "a") == 1);
    Destroy(&a);

    RegularExpression pair = Make("aa", true, Regex);
    Require(CountIn(pair, "aaa") == 2);
    Require(CountIn(pair, "aaaa") == 3);
    Require(CountIn(pair, "aba") == 0);
    Destroy(&pair);

    RegularExpression star = Make("a*", true, Regex);
    Require(CountIn(star, "baaa") == 3);
    Require(CountIn(star, "bbb") == 0);
    Require(CountIn(star, "") == 0);
    Destroy(&star);

    RegularExpression end = Make("$", true, Regex);
    Require(CountIn(end, "ab") == 0);
    Destroy(&end);

    RegularExpression an = Make("AN", false, ContainsString);
    Require(CountIn(an, "banana") == 2);
    Require(CountIn(an, "BANANA") == 2);
    Destroy(&an);

    RegularExpression dot = Make(".", true, ContainsString);
    Require(CountIn(dot, "a.b.c") == 2);
    Destroy(&dot);

    const StringImpl widePattern = Utf16(u"中");
    RegularExpression wide = CreateFrom(&widePattern, true, Regex);
    Require(CountIn(wide, Utf16(u"中中x中")) == 3);
    Destroy(&wide);

    RegularExpression invalid = Make("(", true, Regex);
    Require(CountIn(invalid, "((") == 0);
    Destroy(&invalid);
}

void TestCountEmptyAndBadArguments() {
    RegularExpression star = Make("a*", true, Regex);
    const StringReference nullContent{nullptr};
    Require(Count(&star, &nullContent) == 0);
    const StringImpl empty = Latin1("");
    Require(CountIn(star, empty) == 0);

    RegularExpression unset{};
    Require(Count(&unset, &nullContent) == 0);
    Require(CountIn(unset, empty) == 0);
    Require(ThrowsInvalid([&] { CountIn(unset, "a"); }));
    Require(ThrowsInvalid([&] { Count(nullptr, &nullContent); }));
    Require(ThrowsInvalid([&] { Count(&star, nullptr); }));
    Destroy(&star);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestSearchStringTypes();
    TestCaseSensitivity();
    TestEscaping();
    TestWideAndNullSearchStrings();
    TestCreateRejectsBadArguments();
    TestCount();
    TestCountEmptyAndBadArguments();
    Require(live.empty());
    return 0;
}

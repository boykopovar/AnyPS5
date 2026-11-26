#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <typeinfo>

using Jsc::WtfAbi::StringImpl;
using Jsc::YarrAbi::RegularExpression;
using Jsc::YarrAbi::RegularExpressionPrivate;
using Jsc::YarrAbi::StringReference;

namespace {

const auto Construct = &_ZN3JSC4Yarr17RegularExpressionC1ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE;
const auto ConstructBase = &_ZN3JSC4Yarr17RegularExpressionC2ERKN3WTF6StringENS0_19TextCaseSensitivityENS0_13MultilineModeENS0_11UnicodeModeE;
const auto Match = &_ZNK3JSC4Yarr17RegularExpression5matchERKN3WTF6StringEiPi;
const auto SearchRev = &_ZNK3JSC4Yarr17RegularExpression9searchRevERKN3WTF6StringE;
const auto IsValid = &_ZNK3JSC4Yarr17RegularExpression7isValidEv;
const auto ReplaceIn = &_ZN3JSC4Yarr7replaceERN3WTF6StringERKNS0_17RegularExpressionERKS2_;
const auto Release = &JscRegularExpressionRelease_nid_no_patch;

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

void* AllocatorAllocate(std::size_t size) { return TestAllocate(size); }

void AllocatorDeallocate(void* pointer) { TestFree(pointer); }

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
        std::fprintf(stderr, "Yarr RegularExpression check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

struct Text {
    std::string narrow;
    std::u16string wide;
    StringImpl impl;
    StringReference reference;

    Text(const std::string& text) : narrow(text), impl{2, static_cast<unsigned>(narrow.size()), narrow.data(), Jsc::WtfAbi::StringImplFlag8Bit}, reference{&impl} {}
    Text(const std::u16string& text) : wide(text), impl{2, static_cast<unsigned>(wide.size()), wide.data(), 0}, reference{&impl} {}
    Text(const Text&) = delete;
};

std::u16string Widen(const std::string& text) {
    std::u16string wide;
    for (const unsigned char unit : text) wide.push_back(unit);
    return wide;
}

StringImpl* NewText(const std::string& text) {
    constexpr auto charactersOffset = offsetof(StringImpl, hashAndFlags) + sizeof(StringImpl::hashAndFlags);
    void* storage = TestAllocate(std::max<std::size_t>(sizeof(StringImpl), charactersOffset + text.size()));
    auto* characters = static_cast<std::uint8_t*>(storage) + charactersOffset;
    auto* impl = new (storage) StringImpl{2, static_cast<unsigned>(text.size()), characters, Jsc::WtfAbi::StringImplFlag8Bit};
    std::memcpy(characters, text.data(), text.size());
    return impl;
}

StringImpl* NewText(const std::u16string& text) {
    constexpr auto charactersOffset = offsetof(StringImpl, hashAndFlags) + sizeof(StringImpl::hashAndFlags);
    void* storage = TestAllocate(std::max<std::size_t>(sizeof(StringImpl), charactersOffset + text.size() * 2));
    auto* characters = static_cast<std::uint8_t*>(storage) + charactersOffset;
    auto* impl = new (storage) StringImpl{2, static_cast<unsigned>(text.size()), characters, 0};
    std::memcpy(characters, text.data(), text.size() * 2);
    return impl;
}

void FreeText(StringImpl* impl) {
    TestFree(const_cast<void*>(static_cast<const void*>(impl)));
}

std::u16string ReadText(const StringReference& reference) {
    const StringImpl* impl = reference.impl;
    std::u16string text;
    if (!impl) return text;
    Require(impl->data || !impl->length);
    for (unsigned index = 0; index < impl->length; ++index) {
        text.push_back((impl->hashAndFlags & Jsc::WtfAbi::StringImplFlag8Bit) != 0
            ? static_cast<char16_t>(static_cast<const std::uint8_t*>(impl->data)[index])
            : static_cast<const char16_t*>(impl->data)[index]);
    }
    return text;
}

struct Expression {
    RegularExpression value{};

    Expression(const StringReference& pattern, int caseSensitivity = 0, int multilineMode = 0, int unicodeMode = 0) {
        Construct(&value, &pattern, caseSensitivity, multilineMode, unicodeMode);
    }
    Expression(const std::string& pattern, int caseSensitivity = 0, int multilineMode = 0, int unicodeMode = 0) {
        const Text text(pattern);
        Construct(&value, &text.reference, caseSensitivity, multilineMode, unicodeMode);
    }
    Expression(const Expression&) = delete;
    ~Expression() { Release(value.d); }

    int MatchIn(const StringReference& text, int startFrom = 0, int* length = nullptr) const { return Match(&value, &text, startFrom, length); }
};

bool Valid(const std::string& pattern, int caseSensitivity = 0, int multilineMode = 0, int unicodeMode = 0) {
    Expression expression(pattern, caseSensitivity, multilineMode, unicodeMode);
    return IsValid(&expression.value);
}

template<typename TException, typename TCall>
bool ThrowsExactly(TCall call) {
    try {
        call();
    } catch (const TException& error) {
        return typeid(error) == typeid(TException);
    } catch (...) {
        return false;
    }
    return false;
}

template<typename TException, typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const TException&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

void TestLayoutAndMatch() {
    for (const bool wide : {false, true}) {
        Expression expression("b+");
        const RegularExpressionPrivate* d = expression.value.d;
        Require(d != nullptr && live.count(const_cast<RegularExpressionPrivate*>(d)) == 1);
        const auto* bytes = reinterpret_cast<const unsigned char*>(d);
        std::uint32_t references = 0;
        std::int32_t lastLength = 0;
        std::memcpy(&references, bytes, 4);
        std::memcpy(&lastLength, bytes + 0x10, 4);
        Require(references == 1 && lastLength == -1);
        Require(d->numSubpatterns == 0 && d->regExpByteCode != nullptr);
        Require(IsValid(&expression.value));

        const Text narrowInput(std::string("aabbbcbb"));
        const Text wideInput(Widen("aabbbcbb"));
        const StringReference& input = wide ? wideInput.reference : narrowInput.reference;
        int length = -5;
        Require(expression.MatchIn(input, 0, &length) == 2 && length == 3);
        std::memcpy(&lastLength, bytes + 0x10, 4);
        Require(lastLength == 3);
        Require(expression.MatchIn(input, 2, &length) == 2 && length == 3);
        Require(expression.MatchIn(input, 3, &length) == 3 && length == 2);
        Require(expression.MatchIn(input, 5, &length) == 6 && length == 2);
        Require(expression.MatchIn(input, 7) == 7 && d->lastMatchLength == 1);
        length = -5;
        Require(expression.MatchIn(input, 8, &length) == -1 && length == -5 && d->lastMatchLength == -1);
        Require(expression.MatchIn(input, 9) == -1);
        Require(expression.MatchIn(input, -1) == -1);
        Require(expression.MatchIn(input, 0) == 2 && d->lastMatchLength == 3);

        Require(SearchRev(&expression.value, &input) == 6 && d->lastMatchLength == 2);
        const Text none(std::string("xyz"));
        Require(SearchRev(&expression.value, &none.reference) == -1 && d->lastMatchLength == -1);
    }
    Require(live.empty());
}

void TestEmptyAndNull() {
    {
        Expression empty("");
        Require(IsValid(&empty.value) && empty.value.d->numSubpatterns == 0);
        const Text input(std::string("abc"));
        int length = -5;
        Require(empty.MatchIn(input.reference, 0, &length) == 0 && length == 0);
        Require(empty.MatchIn(input.reference, 3, &length) == 3 && length == 0);
        Require(empty.MatchIn(input.reference, 4) == -1);
        Require(SearchRev(&empty.value, &input.reference) == 3 && empty.value.d->lastMatchLength == 0);
        StringImpl emptyImpl{2, 0, nullptr, Jsc::WtfAbi::StringImplFlag8Bit};
        const StringReference emptyString{&emptyImpl};
        Require(empty.MatchIn(emptyString, 0, &length) == 0 && length == 0);
        const StringReference nullString{nullptr};
        Require(empty.MatchIn(nullString) == -1 && empty.value.d->lastMatchLength == 0);
        Require(SearchRev(&empty.value, &nullString) == -1 && empty.value.d->lastMatchLength == -1);
    }
    {
        const StringReference nullPattern{nullptr};
        Expression fromNull(nullPattern);
        Require(IsValid(&fromNull.value));
        const Text input(std::string("abc"));
        Require(fromNull.MatchIn(input.reference) == 0);
    }
    {
        RegularExpression base{};
        const Text pattern(std::string("c"));
        ConstructBase(&base, &pattern.reference, 0, 0, 0);
        const Text input(std::string("abc"));
        Require(Match(&base, &input.reference, 0, nullptr) == 2);
        Release(base.d);
    }
    Require(live.empty());
}

void TestFlags() {
    const Text lower(std::string("xa"));
    Require(Expression("A", 1).MatchIn(lower.reference) == 1);
    Require(Expression("A", 0).MatchIn(lower.reference) == -1);
    Require(Expression("A", 2).MatchIn(lower.reference) == -1);
    const Text accented(std::string("caf\xE9"));
    Require(Expression(std::string("\xC9"), 1).MatchIn(accented.reference) == 3);
    Require(Expression(std::string("\xC9"), 0).MatchIn(accented.reference) == -1);

    const Text lines(std::string("a\nb"));
    Require(Expression("^b", 0, 1).MatchIn(lines.reference) == 2);
    Require(Expression("^b", 0, 0).MatchIn(lines.reference) == -1);
    Require(Expression("a$", 0, 1).MatchIn(lines.reference) == 0);
    Require(Expression("a$", 0, 0).MatchIn(lines.reference) == -1);

    const Text pair(std::u16string(u"x\xD83D\xDE00y"));
    int length = 0;
    Require(Expression("[^x]", 0, 0, 1).MatchIn(pair.reference, 0, &length) == 1 && length == 2);
    Require(Expression("[^x]", 0, 0, 0).MatchIn(pair.reference, 0, &length) == 1 && length == 1);
    Require(Expression("\\u{1F600}", 0, 0, 1).MatchIn(pair.reference, 0, &length) == 1 && length == 2);
    Require(Expression("y", 0, 0, 1).MatchIn(pair.reference, 3, &length) == 3 && length == 1);
    Require(Expression("y", 0, 0, 1).MatchIn(pair.reference, 4) == -1);
    Require(Expression("", 0, 0, 1).MatchIn(pair.reference, 4, &length) == 4 && length == 0);
    Require(Expression(".", 0, 0, 1).MatchIn(pair.reference, 2, &length) == 1 && length == 2);
    Require(Expression(".", 0, 0, 0).MatchIn(pair.reference, 2, &length) == 2 && length == 1);
    const Text astral(std::u16string(u"\xD83D\xDE00"));
    Require(Expression(astral.reference, 0, 0, 1).MatchIn(pair.reference, 0, &length) == 1 && length == 2);
    Require(Expression(astral.reference, 0, 0, 0).MatchIn(pair.reference, 0, &length) == 1 && length == 2);
    Require(Expression("\\p{Lu}+", 0, 0, 1).MatchIn(Text(std::string("abCDe")).reference, 0, &length) == 2 && length == 2);
    Require(Expression("\\p{Lu}", 0, 0, 0).MatchIn(Text(std::string("xp{Lu}")).reference, 0, &length) == 1 && length == 5);
    Require(live.empty());
}

void TestGroups() {
    {
        Expression groups("(a)(b)?");
        Require(groups.value.d->numSubpatterns == 2);
        int length = 0;
        Require(groups.MatchIn(Text(std::string("xxab")).reference, 0, &length) == 2 && length == 2);
    }
    {
        Expression named("(?<n>a)\\k<n>");
        Require(IsValid(&named.value) && named.value.d->numSubpatterns == 1);
        int length = 0;
        Require(named.MatchIn(Text(std::string("baa")).reference, 0, &length) == 1 && length == 2);
    }
    {
        for (const char* pattern : {"(?<a>x)|(?<\\u0061>y)", "(?<\\u0061>x)|(?<a>y)", "(?<a>x)|(?<\\u{61}>y)", "(?<a>x)|(?<\\u{000061}>y)",
                 "(?<\\u0061\\u0062>x)|(?<ab>y)", "(?<\\u{1D4D0}>x)|(?<\\uD835\\uDCD0>y)"}) {
            Expression duplicate(pattern);
            Require(!IsValid(&duplicate.value));
        }
        Expression escaped("(?<a>x)|(?<\\u0062>y)");
        Require(IsValid(&escaped.value) && escaped.value.d->numSubpatterns == 2);
        Require(escaped.MatchIn(Text(std::string("zy")).reference) == 1);
        Expression afterName("(?<n>a)>(?:b)");
        int length = 0;
        Require(IsValid(&afterName.value) && afterName.MatchIn(Text(std::string("xa>b")).reference, 0, &length) == 1 && length == 3);
        Expression unterminated("(?<n(?:a)");
        Require(!IsValid(&unterminated.value));
    }
    {
        Expression lookahead("a(?=b)|c(?!d)");
        int length = 0;
        Require(lookahead.MatchIn(Text(std::string("acdab")).reference, 0, &length) == 3 && length == 1);
        Require(lookahead.MatchIn(Text(std::string("cd")).reference) == -1);
    }
    {
        Expression escaped("\\(?<=a");
        Require(IsValid(&escaped.value));
        Require(escaped.MatchIn(Text(std::string("x<=a")).reference) == 1);
        Expression inClass("[(?<=]+");
        int length = 0;
        Require(IsValid(&inClass.value) && inClass.MatchIn(Text(std::string("a(?<=b")).reference, 0, &length) == 1 && length == 4);
    }
    Require(live.empty());
}

void TestInvalidPatterns() {
    for (const char* pattern : {"(", ")", "[b-a]", "a{2,1}", "*", "a**", "\\", "[a",
             "(?<n>a)(?<n>b)", "(?<n>a)|(?<n>b)", "(?<=a)b", "(?<!a)b", "(?i:a)", "(?-i:a)", "(?m-i:a)", "(?", "(?#a)", "(?<>a)"}) {
        Expression expression(pattern);
        Require(expression.value.d != nullptr && expression.value.d->refCount == 1);
        Require(!IsValid(&expression.value));
        Require(expression.value.d->regExpByteCode == nullptr && expression.value.d->numSubpatterns == 0 && expression.value.d->lastMatchLength == -1);
        const Text input(std::string("ab"));
        int length = -5;
        Require(expression.MatchIn(input.reference, 0, &length) == -1 && length == -5);
        Require(SearchRev(&expression.value, &input.reference) == -1);
    }
    Require(!Valid("\\p{Nope}", 0, 0, 1));
    Require(!Valid("\\u{110000}", 0, 0, 1));
    Require(!Valid("a{", 0, 0, 1));
    Require(Valid("a{", 0, 0, 0));
    Require(Valid("]", 0, 0, 0));
    Require(!Valid("]", 0, 0, 1));
    Require(live.empty());
}

void TestLimits() {
    {
        Expression slow("(x+x+)+y");
        const Text input(std::string(40, 'x'));
        Require(slow.MatchIn(input.reference) == -1 && slow.value.d->lastMatchLength == -1);
        const Text matching(std::string(40, 'x') + "y");
        int length = 0;
        Require(slow.MatchIn(matching.reference, 0, &length) == 0 && length == 41);
    }
    std::string manyGroups;
    for (int index = 0; index < 300; ++index) manyGroups += "(a)";
    RegularExpression expression{};
    expression.d = reinterpret_cast<RegularExpressionPrivate*>(std::uintptr_t{0x1234});
    const Text pattern(manyGroups);
    Require(ThrowsExactly<std::logic_error>([&] { Construct(&expression, &pattern.reference, 0, 0, 0); }));
    Require(expression.d == reinterpret_cast<RegularExpressionPrivate*>(std::uintptr_t{0x1234}));
    std::string nested;
    for (int index = 0; index < 20000; ++index) nested += "(?:";
    nested += "a";
    for (int index = 0; index < 20000; ++index) nested += ")";
    const Text deep(nested);
    Require(ThrowsExactly<std::logic_error>([&] { Construct(&expression, &deep.reference, 0, 0, 0); }));
    Require(expression.d == reinterpret_cast<RegularExpressionPrivate*>(std::uintptr_t{0x1234}));
    Require(live.empty());
}

void TestReferencesAndGuards() {
    RegularExpression expression{};
    const Text pattern(std::string("a"));
    Construct(&expression, &pattern.reference, 0, 0, 0);
    RegularExpressionPrivate* d = expression.d;
    d->refCount = 3;
    Release(d);
    Require(d->refCount == 2 && live.count(d) == 1);
    Release(d);
    Require(d->refCount == 1 && live.count(d) == 1);
    Release(d);
    Require(live.empty());
    Release(nullptr);

    const Text input(std::string("a"));
    RegularExpression null{};
    Require(Throws<std::invalid_argument>([&] { Construct(nullptr, &pattern.reference, 0, 0, 0); }));
    Require(Throws<std::invalid_argument>([&] { Construct(&null, nullptr, 0, 0, 0); }));
    StringImpl broken{2, 3, nullptr, Jsc::WtfAbi::StringImplFlag8Bit};
    const StringReference brokenReference{&broken};
    Require(Throws<std::invalid_argument>([&] { Construct(&null, &brokenReference, 0, 0, 0); }));
    Require(null.d == nullptr);
    Require(Throws<std::invalid_argument>([&] { Match(nullptr, &input.reference, 0, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { Match(&null, &input.reference, 0, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { SearchRev(&null, &input.reference); }));
    Require(Throws<std::invalid_argument>([&] { IsValid(nullptr); }));
    Require(Throws<std::invalid_argument>([&] { IsValid(&null); }));
    Expression valid("a");
    Require(Throws<std::invalid_argument>([&] { Match(&valid.value, nullptr, 0, nullptr); }));
    Require(Throws<std::invalid_argument>([&] { Match(&valid.value, &brokenReference, 0, nullptr); }));
}

void TestReplace() {
    const Jsc::WtfAbi::PersistenceAllocator allocator{&AllocatorAllocate, &AllocatorDeallocate};
    RegisterPersistenceAllocator_nid_no_patch(&allocator);
    {
        Expression expression("a");
        StringReference text{NewText(std::string("abcabc"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string("X"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(text.impl != original && original->refCount == 0 && live.count(original) == 1);
        Require(text.impl->refCount == 2 && text.impl->length == 6);
        Require((text.impl->hashAndFlags & Jsc::WtfAbi::StringImplFlag8Bit) != 0);
        Require(ReadText(text) == u"XbcXbc");
        FreeText(original);
        FreeText(text.impl);
    }
    {
        Expression expression("z");
        StringReference text{NewText(std::string("abc"))};
        StringImpl* const original = text.impl;
        const std::size_t allocations = live.size();
        const Text replacement(std::string("X"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(text.impl == original && live.size() == allocations && original->refCount == 2);
        FreeText(original);
    }
    {
        Expression expression("a");
        StringReference text{NewText(std::string("a"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string("$&x\\n"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(ReadText(text) == u"$&x\\n");
        FreeText(original);
        FreeText(text.impl);
    }
    {
        Expression expression("a*");
        StringReference text{NewText(std::string("ba"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string("-"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(ReadText(text) == u"-ba");
        FreeText(original);
        FreeText(text.impl);
    }
    {
        Expression expression("a*");
        StringReference text{NewText(std::string("aa"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string("-"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(ReadText(text) == u"-");
        FreeText(original);
        FreeText(text.impl);
    }
    {
        Expression expression("a*");
        StringReference text{NewText(std::string("ba"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string(""));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(text.impl == original && original->refCount == 2 && ReadText(text) == u"ba");
        FreeText(original);
    }
    {
        Expression expression("a");
        StringReference text{NewText(std::string("a"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string("aa"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(ReadText(text) == u"aa");
        FreeText(original);
        FreeText(text.impl);
    }
    {
        Expression expression("a");
        StringReference text{NewText(std::string("abcabc"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string(""));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(ReadText(text) == u"bcbc");
        FreeText(original);
        FreeText(text.impl);
    }
    {
        Expression expression("a");
        StringReference text{NewText(std::string("a"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string(""));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(text.impl->length == 0 && text.impl->refCount == 2 && ReadText(text).empty());
        Require((text.impl->hashAndFlags & Jsc::WtfAbi::StringImplFlag8Bit) != 0);
        FreeText(original);
        FreeText(text.impl);
    }
    {
        Expression expression("a");
        StringReference text{NewText(std::string("a"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::u16string(u"\u00e9"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require((text.impl->hashAndFlags & Jsc::WtfAbi::StringImplFlag8Bit) == 0);
        Require(ReadText(text) == u"\u00e9");
        FreeText(original);
        FreeText(text.impl);
    }
    {
        Expression expression("a");
        StringReference text{NewText(std::u16string(u"a\u00e9"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string("Z"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require((text.impl->hashAndFlags & Jsc::WtfAbi::StringImplFlag8Bit) == 0);
        Require(ReadText(text) == u"Z\u00e9");
        FreeText(original);
        FreeText(text.impl);
    }
    {
        Expression expression("a");
        const Text replacement(std::string("X"));
        StringReference text{NewText(std::string("a"))};
        StringImpl* const original = text.impl;
        Require(Throws<std::invalid_argument>([&] { ReplaceIn(nullptr, &expression.value, &replacement.reference); }));
        Require(Throws<std::invalid_argument>([&] { ReplaceIn(&text, nullptr, &replacement.reference); }));
        RegularExpression none{};
        Require(Throws<std::invalid_argument>([&] { ReplaceIn(&text, &none, &replacement.reference); }));
        Require(Throws<std::invalid_argument>([&] { ReplaceIn(&text, &expression.value, nullptr); }));
        Require(text.impl == original && original->refCount == 2);
        FreeText(original);
        StringReference empty{};
        ReplaceIn(&empty, &expression.value, &replacement.reference);
        Require(empty.impl == nullptr);
    }
    {
        Expression expression("(");
        Require(!IsValid(&expression.value));
        StringReference text{NewText(std::string("a"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string("X"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(text.impl == original && original->refCount == 2);
        FreeText(original);
    }
    {
        Expression expression("a");
        StringReference text{NewText(std::string("a"))};
        StringImpl* const original = text.impl;
        original->refCount = 4;
        const Text replacement(std::string("XY"));
        ReplaceIn(&text, &expression.value, &replacement.reference);
        Require(text.impl != original && original->refCount == 2 && live.count(original) == 1);
        Require(ReadText(text) == u"XY");
        FreeText(original);
        FreeText(text.impl);
    }
    {
        RegisterPersistenceAllocator_nid_no_patch(nullptr);
        Expression expression("a");
        StringReference text{NewText(std::string("a"))};
        StringImpl* const original = text.impl;
        const Text replacement(std::string("X"));
        Require(Throws<std::runtime_error>([&] { ReplaceIn(&text, &expression.value, &replacement.reference); }));
        Require(text.impl == original && original->refCount == 2 && live.size() == 2);
        FreeText(original);
        RegisterPersistenceAllocator_nid_no_patch(&allocator);
    }
    RegisterPersistenceAllocator_nid_no_patch(nullptr);
    Require(live.empty());
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    TestLayoutAndMatch();
    TestEmptyAndNull();
    TestFlags();
    TestGroups();
    TestInvalidPatterns();
    TestLimits();
    TestReferencesAndGuards();
    TestReplace();
    Require(live.empty());
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using Jsc::WtfAbi::ASCIILiteral;
using Jsc::WtfAbi::String;
using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringVector;

namespace {

constexpr unsigned MaxLength = 0x7FFFFFFFu;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF string create check failed at line %d\n", line);
        std::abort();
    }
}

bool Is8Bit(const StringImpl* impl) {
    return impl->hashAndFlags & Jsc::WtfAbi::StringImplFlag8Bit;
}

bool IsStatic(const StringImpl* impl) {
    return impl->refCount & 1;
}

bool Equals8(const StringImpl* impl, const char* expected) {
    const std::size_t length = std::strlen(expected);
    return impl && Is8Bit(impl) && impl->length == length && !std::memcmp(impl->data, expected, length);
}

bool Equals16(const StringImpl* impl, const std::u16string& expected) {
    return impl && !Is8Bit(impl) && impl->length == expected.size() && !std::memcmp(impl->data, expected.data(), expected.size() * 2);
}

bool Equals8(const String& string, const char* expected) {
    return Equals8(string.impl, expected);
}

bool Equals16(const String& string, const std::u16string& expected) {
    return Equals16(string.impl, expected);
}

bool IsEmptyStatic(const String& string) {
    return string.impl && !string.impl->length && IsStatic(string.impl) && Is8Bit(string.impl);
}

const unsigned char* Latin1(const char* text) {
    return reinterpret_cast<const unsigned char*>(text);
}

String Make8(const char* text) {
    return _ZN3WTF10StringImpl6createEPKh(Latin1(text));
}

String Make16(const char16_t* text) {
    String result;
    _ZN3WTF6StringC1EPKDs(&result, text);
    return result;
}

bool APS5_VABI IsDigit(char16_t character) {
    return character >= '0' && character <= '9';
}

template<typename Exception, typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

void TestCreate() {
    const char* source = "hello";
    String hello = _ZN3WTF10StringImpl6createEPKh(Latin1(source));
    Check(Equals8(hello, "hello"), __LINE__);
    Check(hello.impl->refCount == 2, __LINE__);
    Check(hello.impl->data != source, __LINE__);
    Check((hello.impl->hashAndFlags & 3) == 0, __LINE__);
    Check(IsEmptyStatic(_ZN3WTF10StringImpl6createEPKh(nullptr)), __LINE__);
    Check(IsEmptyStatic(_ZN3WTF10StringImpl6createEPKh(Latin1(""))), __LINE__);

    String narrow = _ZN3WTF10StringImpl20create8BitIfPossibleEPKDs(u"abé");
    Check(Equals8(narrow, "ab\xE9"), __LINE__);
    String wide = _ZN3WTF10StringImpl20create8BitIfPossibleEPKDs(u"aĀ");
    Check(Equals16(wide, u"aĀ"), __LINE__);
    Check(IsEmptyStatic(_ZN3WTF10StringImpl20create8BitIfPossibleEPKDs(u"")), __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZN3WTF10StringImpl20create8BitIfPossibleEPKDs(nullptr); }), __LINE__);

    static const char literal[] = "literal";
    String fromLiteral = _ZN3WTF10StringImpl17createFromLiteralEPKc(literal);
    Check(fromLiteral.impl->data == literal && fromLiteral.impl->length == 7 && Is8Bit(fromLiteral.impl), __LINE__);
    Check(fromLiteral.impl->refCount == 2, __LINE__);
    String prefix = _ZN3WTF10StringImpl17createFromLiteralEPKcj(literal, 3);
    Check(prefix.impl->data == literal && Equals8(prefix, "lit"), __LINE__);
    String zeroLiteral = _ZN3WTF10StringImpl17createFromLiteralEPKcj(literal, 0);
    Check(zeroLiteral.impl->length == 0 && zeroLiteral.impl->refCount == 2 && zeroLiteral.impl->data == literal, __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZN3WTF10StringImpl17createFromLiteralEPKc(nullptr); }), __LINE__);

    static const char16_t wideText[] = u"Āxyz";
    String wideNoCopy = _ZN3WTF10StringImpl20createWithoutCopyingEPKDsj(wideText, 4);
    Check(wideNoCopy.impl->data == wideText && Equals16(wideNoCopy, u"Āxyz"), __LINE__);
    String narrowNoCopy = _ZN3WTF10StringImpl20createWithoutCopyingEPKhj(Latin1(literal), 2);
    Check(narrowNoCopy.impl->data == literal && Equals8(narrowNoCopy, "li"), __LINE__);
    Check(IsEmptyStatic(_ZN3WTF10StringImpl20createWithoutCopyingEPKDsj(wideText, 0)), __LINE__);
    Check(IsEmptyStatic(_ZN3WTF10StringImpl20createWithoutCopyingEPKhj(Latin1(literal), 0)), __LINE__);
}

void TestReplace() {
    String dashed = Make8("a-b-c");
    String doubleDash = Make8("--");
    String replaced = _ZN3WTF10StringImpl7replaceEDsPS0_(dashed.impl, u'-', doubleDash.impl);
    Check(Equals8(replaced, "a--b--c"), __LINE__);
    String emptyReplacement = Make8("");
    Check(Equals8(_ZN3WTF10StringImpl7replaceEDsPS0_(dashed.impl, u'-', emptyReplacement.impl), "abc"), __LINE__);
    String wideReplacement = Make16(u"Ā");
    Check(Equals16(_ZN3WTF10StringImpl7replaceEDsPS0_(dashed.impl, u'-', wideReplacement.impl), u"aĀbĀc"), __LINE__);
    String wideSource = Make16(u"xāx");
    Check(Equals16(_ZN3WTF10StringImpl7replaceEDsPS0_(wideSource.impl, u'x', doubleDash.impl), u"--ā--"), __LINE__);
    {
        String same = _ZN3WTF10StringImpl7replaceEDsPS0_(dashed.impl, u'Ā', doubleDash.impl);
        Check(same.impl == dashed.impl && dashed.impl->refCount == 4, __LINE__);
        String unchanged = _ZN3WTF10StringImpl7replaceEDsPS0_(dashed.impl, u'-', nullptr);
        Check(unchanged.impl == dashed.impl && dashed.impl->refCount == 6, __LINE__);
    }
    Check(dashed.impl->refCount == 2, __LINE__);

    String sentence = Make8("foo bar foo");
    String foo = Make8("foo");
    String x = Make8("x");
    Check(Equals8(_ZN3WTF10StringImpl7replaceEPS0_S1_(sentence.impl, foo.impl, x.impl), "x bar x"), __LINE__);
    String run = Make8("aaaaa");
    String pair = Make8("aa");
    String b = Make8("b");
    Check(Equals8(_ZN3WTF10StringImpl7replaceEPS0_S1_(run.impl, pair.impl, b.impl), "bba"), __LINE__);
    String wideFoo = Make16(u"fooĀfoo");
    Check(Equals16(_ZN3WTF10StringImpl7replaceEPS0_S1_(wideFoo.impl, foo.impl, x.impl), u"xĀx"), __LINE__);
    Check(Equals16(_ZN3WTF10StringImpl7replaceEPS0_S1_(sentence.impl, foo.impl, wideReplacement.impl), u"Ā bar Ā"), __LINE__);
    {
        String emptyPattern = Make8("");
        String same = _ZN3WTF10StringImpl7replaceEPS0_S1_(sentence.impl, emptyPattern.impl, x.impl);
        Check(same.impl == sentence.impl, __LINE__);
        String absent = Make8("baz");
        String noMatch = _ZN3WTF10StringImpl7replaceEPS0_S1_(sentence.impl, absent.impl, x.impl);
        Check(noMatch.impl == sentence.impl, __LINE__);
        String nullPattern = _ZN3WTF10StringImpl7replaceEPS0_S1_(sentence.impl, nullptr, x.impl);
        Check(nullPattern.impl == sentence.impl && sentence.impl->refCount == 8, __LINE__);
    }
    Check(Throws<std::invalid_argument>([&] { _ZN3WTF10StringImpl7replaceEPS0_S1_(nullptr, foo.impl, x.impl); }), __LINE__);

    String hello = Make8("Hello World");
    String there = Make8("There");
    Check(Equals8(_ZN3WTF10StringImpl7replaceEjjPS0_(hello.impl, 6, 5, there.impl), "Hello There"), __LINE__);
    String bang = Make8("!");
    Check(Equals8(_ZN3WTF10StringImpl7replaceEjjPS0_(hello.impl, 100, 3, bang.impl), "Hello World!"), __LINE__);
    Check(Equals8(_ZN3WTF10StringImpl7replaceEjjPS0_(hello.impl, 5, 100, nullptr), "Hello"), __LINE__);
    Check(Equals16(_ZN3WTF10StringImpl7replaceEjjPS0_(hello.impl, 0, 5, wideReplacement.impl), u"Ā World"), __LINE__);
    {
        String same = _ZN3WTF10StringImpl7replaceEjjPS0_(hello.impl, 3, 0, nullptr);
        Check(same.impl == hello.impl, __LINE__);
    }
}

void TestSimplifyWhiteSpace() {
    String messy = Make8("  a \t b\n ");
    String simplified = _ZN3WTF10StringImpl18simplifyWhiteSpaceEv(messy.impl);
    Check(Equals8(simplified, "a b"), __LINE__);
    Check((simplified.impl->hashAndFlags & 3) == 1, __LINE__);
    String clean = Make8("a b");
    {
        String same = _ZN3WTF10StringImpl18simplifyWhiteSpaceEv(clean.impl);
        Check(same.impl == clean.impl && clean.impl->refCount == 4, __LINE__);
    }
    String tab = Make8("a\tb");
    Check(Equals8(_ZN3WTF10StringImpl18simplifyWhiteSpaceEv(tab.impl), "a b"), __LINE__);
    String doubled = Make8("a  b");
    Check(Equals8(_ZN3WTF10StringImpl18simplifyWhiteSpaceEv(doubled.impl), "a b"), __LINE__);
    String blank = Make8(" \n ");
    Check(IsEmptyStatic(_ZN3WTF10StringImpl18simplifyWhiteSpaceEv(blank.impl)), __LINE__);
    String ideographic = Make16(u"\u3000a\u2003\u2028b\u00A0");
    Check(Equals16(_ZN3WTF10StringImpl18simplifyWhiteSpaceEv(ideographic.impl), u"a b\u00A0"), __LINE__);
    String nonSpace = Make16(u"a\u200Bb\u0085");
    {
        String same = _ZN3WTF10StringImpl18simplifyWhiteSpaceEv(nonSpace.impl);
        Check(same.impl == nonSpace.impl, __LINE__);
    }
    const String nullString;
    Check(!_ZNK3WTF6String18simplifyWhiteSpaceEv(&nullString).impl, __LINE__);
    Check(Equals8(_ZNK3WTF6String18simplifyWhiteSpaceEv(&messy), "a b"), __LINE__);
}

void TestConstructors() {
    static const char literal[] = "abc";
    String fromLiteral(reinterpret_cast<StringImpl*>(0x1));
    _ZN3WTF6StringC1ENS_12ASCIILiteralE(&fromLiteral, ASCIILiteral{literal});
    Check(fromLiteral.impl->data == literal && Equals8(fromLiteral, "abc"), __LINE__);
    String fromLiteral2;
    _ZN3WTF6StringC2ENS_12ASCIILiteralE(&fromLiteral2, ASCIILiteral{literal});
    Check(fromLiteral2.impl->data == literal, __LINE__);
    Check(Throws<std::invalid_argument>([] {
        String target;
        _ZN3WTF6StringC1ENS_12ASCIILiteralE(&target, ASCIILiteral{nullptr});
    }), __LINE__);

    String fromWide(reinterpret_cast<StringImpl*>(0x1));
    _ZN3WTF6StringC1EPKDs(&fromWide, nullptr);
    Check(!fromWide.impl, __LINE__);
    const char16_t* wideSource = u"hé";
    _ZN3WTF6StringC2EPKDs(&fromWide, wideSource);
    Check(Equals16(fromWide, u"hé") && fromWide.impl->data != wideSource, __LINE__);
    String emptyWide;
    _ZN3WTF6StringC1EPKDs(&emptyWide, u"");
    Check(IsEmptyStatic(emptyWide), __LINE__);

    String fromLatin1(reinterpret_cast<StringImpl*>(0x1));
    _ZN3WTF6StringC1EPKh(&fromLatin1, nullptr);
    Check(!fromLatin1.impl, __LINE__);
    _ZN3WTF6StringC2EPKh(&fromLatin1, Latin1("xyz"));
    Check(Equals8(fromLatin1, "xyz"), __LINE__);

    Check(Equals16(_ZN3WTF6String23make16BitFrom8BitSourceEPKhm(Latin1("ab\xFF"), 3), u"abÿ"), __LINE__);
    Check(!_ZN3WTF6String23make16BitFrom8BitSourceEPKhm(nullptr, 0).impl, __LINE__);
    Check(Equals8(_ZN3WTF6String23make8BitFrom16BitSourceEPKDsm(u"Łbc", 2), "Ab"), __LINE__);
    Check(!_ZN3WTF6String23make8BitFrom16BitSourceEPKDsm(u"x", 0).impl, __LINE__);
    Check(Throws<std::length_error>([] { _ZN3WTF6String23make8BitFrom16BitSourceEPKDsm(u"x", std::size_t{MaxLength} + 1); }), __LINE__);
}

void TestAppend() {
    String text;
    _ZN3WTF6String6appendEDs(&text, u'x');
    Check(Equals16(text, u"x"), __LINE__);
    String narrow = Make8("ab");
    _ZN3WTF6String6appendEDs(&narrow, u'c');
    Check(Equals8(narrow, "abc"), __LINE__);
    _ZN3WTF6String6appendEDs(&narrow, u'Ā');
    Check(Equals16(narrow, u"abcĀ"), __LINE__);
    _ZN3WTF6String6appendEh(&narrow, 'd');
    Check(Equals16(narrow, u"abcĀd"), __LINE__);

    String latin;
    _ZN3WTF6String6appendEh(&latin, 0xE9);
    Check(Equals8(latin, "\xE9"), __LINE__);
    _ZN3WTF6String6appendEh(&latin, 'f');
    Check(Equals8(latin, "\xE9" "f"), __LINE__);

    String shared = Make8("shared");
    String target;
    _ZN3WTF6String6appendERKS0_(&target, &shared);
    Check(target.impl == shared.impl && shared.impl->refCount == 4, __LINE__);
    String suffix = Make8("!");
    _ZN3WTF6String6appendERKS0_(&target, &suffix);
    Check(Equals8(target, "shared!") && shared.impl->refCount == 2, __LINE__);
    const String nullString;
    String emptyString = Make8("");
    StringImpl* before = target.impl;
    _ZN3WTF6String6appendERKS0_(&target, &nullString);
    _ZN3WTF6String6appendERKS0_(&target, &emptyString);
    Check(target.impl == before, __LINE__);
    String wide = Make16(u"Ā");
    _ZN3WTF6String6appendERKS0_(&target, &wide);
    Check(Equals16(target, u"shared!Ā"), __LINE__);

    String chars = Make8("ab");
    _ZN3WTF6String6appendEPKDsj(&chars, u"cd", 2);
    Check(Equals16(chars, u"abcd"), __LINE__);
    String nullChars;
    _ZN3WTF6String6appendEPKDsj(&nullChars, nullptr, 3);
    Check(!nullChars.impl, __LINE__);
    _ZN3WTF6String6appendEPKDsj(&nullChars, u"q", 1);
    Check(Equals16(nullChars, u"q"), __LINE__);
    String latinChars = Make8("ab");
    _ZN3WTF6String6appendEPKhj(&latinChars, Latin1("cde"), 2);
    Check(Equals8(latinChars, "abcd"), __LINE__);
    before = latinChars.impl;
    _ZN3WTF6String6appendEPKhj(&latinChars, nullptr, 0);
    Check(latinChars.impl == before, __LINE__);
    String wideLatin = Make16(u"Ā");
    _ZN3WTF6String6appendEPKhj(&wideLatin, Latin1("z"), 1);
    Check(Equals16(wideLatin, u"Āz"), __LINE__);
    String nullLatin;
    _ZN3WTF6String6appendEPKhj(&nullLatin, Latin1("zz"), 0);
    Check(IsEmptyStatic(nullLatin), __LINE__);

    String atom = Make8("atom");
    atom.impl->hashAndFlags |= Jsc::WtfAbi::StringImplFlagAtom;
    StringImpl* atomImpl = atom.impl;
    Check(Throws<std::logic_error>([&] { _ZN3WTF6String6appendEh(&atom, 's'); }), __LINE__);
    Check(atom.impl == atomImpl && Equals8(atom, "atom") && atom.impl->refCount == 2, __LINE__);
    atom.impl->hashAndFlags &= ~Jsc::WtfAbi::StringImplFlagAtom;
}

void TestInsertRemoveTruncate() {
    String text = Make8("ace");
    String b = Make8("b");
    _ZN3WTF6String6insertERKS0_j(&text, &b, 1);
    Check(Equals8(text, "abce"), __LINE__);
    String tail = Make8("fg");
    _ZN3WTF6String6insertERKS0_j(&text, &tail, 100);
    Check(Equals8(text, "abcefg"), __LINE__);
    String wide = Make16(u"Ā");
    _ZN3WTF6String6insertERKS0_j(&text, &wide, 0);
    Check(Equals16(text, u"Āabcefg"), __LINE__);
    String nullTarget;
    const String nullString;
    _ZN3WTF6String6insertERKS0_j(&nullTarget, &nullString, 0);
    Check(!nullTarget.impl, __LINE__);
    String empty = Make8("");
    _ZN3WTF6String6insertERKS0_j(&nullTarget, &empty, 0);
    Check(nullTarget.impl == empty.impl, __LINE__);
    String nullTarget2;
    _ZN3WTF6String6insertERKS0_j(&nullTarget2, &b, 3);
    Check(nullTarget2.impl == b.impl, __LINE__);

    String letters = Make8("abcdef");
    _ZN3WTF6String6removeEjj(&letters, 1, 2);
    Check(Equals8(letters, "adef"), __LINE__);
    StringImpl* before = letters.impl;
    _ZN3WTF6String6removeEjj(&letters, 4, 1);
    _ZN3WTF6String6removeEjj(&letters, 0, 0);
    Check(letters.impl == before, __LINE__);
    _ZN3WTF6String6removeEjj(&letters, 2, 100);
    Check(Equals8(letters, "ad"), __LINE__);
    _ZN3WTF6String6removeEjj(&letters, 0, 2);
    Check(IsEmptyStatic(letters), __LINE__);
    String wideLetters = Make16(u"xĀy");
    _ZN3WTF6String6removeEjj(&wideLetters, 0, 1);
    Check(Equals16(wideLetters, u"Āy"), __LINE__);

    String truncated = Make8("abcdef");
    _ZN3WTF6String8truncateEj(&truncated, 3);
    Check(Equals8(truncated, "abc"), __LINE__);
    before = truncated.impl;
    _ZN3WTF6String8truncateEj(&truncated, 10);
    Check(truncated.impl == before && truncated.impl->refCount == 2, __LINE__);
    _ZN3WTF6String8truncateEj(&truncated, 0);
    Check(IsEmptyStatic(truncated), __LINE__);
    String nullTruncated;
    _ZN3WTF6String8truncateEj(&nullTruncated, 0);
    Check(!nullTruncated.impl, __LINE__);
}

void TestSubstringAndStrip() {
    String letters = Make8("abcdef");
    Check(Equals8(_ZNK3WTF6String9substringEjj(&letters, 2, 2), "cd"), __LINE__);
    Check(Equals8(_ZNK3WTF6String9substringEjj(&letters, 4, 100), "ef"), __LINE__);
    {
        String whole = _ZNK3WTF6String9substringEjj(&letters, 0, MaxLength);
        Check(whole.impl == letters.impl && letters.impl->refCount == 4, __LINE__);
    }
    Check(IsEmptyStatic(_ZNK3WTF6String9substringEjj(&letters, 6, 1)), __LINE__);
    const String nullString;
    Check(!_ZNK3WTF6String9substringEjj(&nullString, 0, 1).impl, __LINE__);
    String wide = Make16(u"aĀb");
    Check(Equals16(_ZNK3WTF6String9substringEjj(&wide, 1, 1), u"Ā"), __LINE__);

    String padded = Make8("  ab \n");
    Check(Equals8(_ZNK3WTF6String15stripWhiteSpaceEv(&padded), "ab"), __LINE__);
    String widePadded = Make16(u" ab ");
    Check(Equals16(_ZNK3WTF6String15stripWhiteSpaceEv(&widePadded), u"ab"), __LINE__);
    String nbsp = Make8("\xA0" "ab");
    {
        String same = _ZNK3WTF6String15stripWhiteSpaceEv(&nbsp);
        Check(same.impl == nbsp.impl, __LINE__);
    }
    String blank = Make8(" \t");
    Check(IsEmptyStatic(_ZNK3WTF6String15stripWhiteSpaceEv(&blank)), __LINE__);
    Check(!_ZNK3WTF6String15stripWhiteSpaceEv(&nullString).impl, __LINE__);

    String digits = Make8("12ab34");
    Check(Equals8(_ZNK3WTF6String33stripLeadingAndTrailingCharactersEPFbDsE(&digits, IsDigit), "ab"), __LINE__);
    String allDigits = Make8("1234");
    Check(IsEmptyStatic(_ZNK3WTF6String33stripLeadingAndTrailingCharactersEPFbDsE(&allDigits, IsDigit)), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF6String33stripLeadingAndTrailingCharactersEPFbDsE(&digits, nullptr); }), __LINE__);
}

bool Parts(const StringVector& vector, std::initializer_list<const char*> expected) {
    if (vector.size != expected.size() || vector.capacity < vector.size) return false;
    unsigned index = 0;
    for (const char* part : expected) {
        const StringImpl* impl = vector.buffer[index++].impl;
        if (!part) {
            if (impl) return false;
        } else if (!*part) {
            if (!impl || impl->length) return false;
        } else if (!Equals8(impl, part)) {
            return false;
        }
    }
    return true;
}

void TestSplit() {
    String list = Make8("a,b,,c");
    String comma = Make8(",");
    Check(Parts(_ZNK3WTF6String5splitERKS0_(&list, &comma), {"a", "b", "c"}), __LINE__);
    Check(Parts(_ZNK3WTF6String25splitAllowingEmptyEntriesERKS0_(&list, &comma), {"a", "b", "", "c"}), __LINE__);
    String edges = Make8(",a,");
    Check(Parts(_ZNK3WTF6String5splitERKS0_(&edges, &comma), {"a"}), __LINE__);
    Check(Parts(_ZNK3WTF6String25splitAllowingEmptyEntriesERKS0_(&edges, &comma), {"", "a", ""}), __LINE__);
    String colons = Make8("a::b::");
    String separator = Make8("::");
    Check(Parts(_ZNK3WTF6String5splitERKS0_(&colons, &separator), {"a", "b"}), __LINE__);
    {
        String single = Make8("abc");
        StringVector parts = _ZNK3WTF6String5splitERKS0_(&single, &comma);
        Check(Parts(parts, {"abc"}) && parts.buffer[0].impl == single.impl && single.impl->refCount == 4, __LINE__);
    }
    const String nullString;
    Check(Parts(_ZNK3WTF6String5splitERKS0_(&nullString, &comma), {}), __LINE__);
    Check(Parts(_ZNK3WTF6String25splitAllowingEmptyEntriesERKS0_(&nullString, &comma), {nullptr}), __LINE__);
    Check(Parts(_ZNK3WTF6String5splitERKS0_(&list, &nullString), {"a,b,,c"}), __LINE__);
    String empty = Make8("");
    Check(Parts(_ZNK3WTF6String5splitERKS0_(&empty, &comma), {}), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF6String5splitERKS0_(&list, &empty); }), __LINE__);
    String wide = Make16(u"xĀy");
    String wideSeparator = Make16(u"Ā");
    StringVector wideParts = _ZNK3WTF6String5splitERKS0_(&wide, &wideSeparator);
    Check(wideParts.size == 2 && Equals16(wideParts.buffer[0].impl, u"x") && Equals16(wideParts.buffer[1].impl, u"y"), __LINE__);
}

void TestToUIntStrict() {
    bool ok = false;
    String spaced = Make8(" 42 ");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&spaced, &ok, 10) == 42 && ok, __LINE__);
    String max = Make8("4294967295");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&max, &ok, 10) == 4294967295u && ok, __LINE__);
    String overflow = Make8("4294967296");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&overflow, &ok, 10) == 0 && !ok, __LINE__);
    String negative = Make8("-1");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&negative, &ok, 10) == 0 && !ok, __LINE__);
    String plus = Make8("+7");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&plus, &ok, 10) == 7 && ok, __LINE__);
    String hex = Make8("fF");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&hex, &ok, 16) == 255 && ok, __LINE__);
    String trailing = Make8("12a");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&trailing, &ok, 10) == 0 && !ok, __LINE__);
    String inner = Make8("1 2");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&inner, &ok, 10) == 0 && !ok, __LINE__);
    String wide = Make16(u"　5 ");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&wide, &ok, 10) == 5 && ok, __LINE__);
    String base36 = Make8("zz");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&base36, &ok, 40) == 35 * 40 + 35 && ok, __LINE__);
    String blank = Make8(" ");
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&blank, &ok, 10) == 0 && !ok, __LINE__);
    const String nullString;
    ok = true;
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&nullString, &ok, 10) == 0 && !ok, __LINE__);
    Check(_ZNK3WTF6String12toUIntStrictEPbi(&spaced, nullptr, 10) == 42, __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF6String12toUIntStrictEPbi(&spaced, nullptr, 0); }), __LINE__);
}

void TestStatics() {
    const String& nullString = _ZN3WTF10nullStringEv();
    Check(!nullString.impl && &nullString == &_ZN3WTF10nullStringEv(), __LINE__);
    const String& emptyString = _ZN3WTF11emptyStringEv();
    Check(IsEmptyStatic(emptyString) && &emptyString == &_ZN3WTF11emptyStringEv(), __LINE__);
    Check((emptyString.impl->hashAndFlags & 0xFF) == 0x1C, __LINE__);
    Check((emptyString.impl->hashAndFlags >> 8) == 0xEC889E, __LINE__);
    String empty = Make8("");
    Check(empty.impl == emptyString.impl, __LINE__);
}

}  // namespace

int main() {
    TestCreate();
    TestReplace();
    TestSimplifyWhiteSpace();
    TestConstructors();
    TestAppend();
    TestInsertRemoveTruncate();
    TestSubstringAndStrip();
    TestSplit();
    TestToUIntStrict();
    TestStatics();
    std::puts("WTF string create tests passed");
    return 0;
}

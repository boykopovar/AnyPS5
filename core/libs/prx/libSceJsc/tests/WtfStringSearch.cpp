#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

using Jsc::WtfAbi::StringImpl;
using Jsc::WtfAbi::StringView;

namespace {

constexpr std::size_t NotFound = static_cast<std::size_t>(-1);
constexpr unsigned MaxLength = 0x7FFFFFFFu;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF string search check failed at line %d\n", line);
        std::abort();
    }
}

StringImpl Make8(const char* text) {
    return {2, static_cast<unsigned>(std::strlen(text)), text, Jsc::WtfAbi::StringImplFlag8Bit | 0x1230u};
}

StringImpl Make16(const std::u16string& text) {
    return {2, static_cast<unsigned>(text.size()), text.data(), 0x4560u};
}

bool APS5_VABI IsDigit(char16_t character) {
    return character >= '0' && character <= '9';
}

bool APS5_VABI IsWide(char16_t character) {
    return character > 0xFF;
}

template<typename Function> bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestPrefixAndSuffix() {
    StringImpl hello = Make8("Hello World");
    StringImpl helloPrefix = Make8("Hello");
    StringImpl lowerPrefix = Make8("hello");
    StringImpl upperPrefix = Make8("hELL");
    StringImpl longer = Make8("Hello World!");
    StringImpl empty = Make8("");
    const std::u16string widePrefixText = u"He";
    StringImpl widePrefix = Make16(widePrefixText);
    StringImpl world = Make8("World");
    StringImpl worldLower = Make8("world");
    StringImpl worldUpper = Make8("WORLD");

    Check(_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &helloPrefix), __LINE__);
    Check(!_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &lowerPrefix), __LINE__);
    Check(!_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &longer), __LINE__);
    Check(_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &empty), __LINE__);
    Check(_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, nullptr), __LINE__);
    Check(_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &widePrefix), __LINE__);
    Check(_ZNK3WTF10StringImpl10startsWithERKS0_(&hello, &helloPrefix), __LINE__);
    Check(!_ZNK3WTF10StringImpl10startsWithERKS0_(&helloPrefix, &hello), __LINE__);

    Check(_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_(&hello, &upperPrefix), __LINE__);
    Check(!_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_(&hello, nullptr), __LINE__);
    Check(!_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_(&hello, &longer), __LINE__);
    Check(_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(&hello, &lowerPrefix), __LINE__);
    Check(!_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(&hello, &world), __LINE__);

    StringImpl latinUpper = Make8("\xC9t\xC9");
    StringImpl latinLower = Make8("\xE9t");
    const std::u16string wideLatinText = u"ÉT";
    StringImpl wideLatin = Make16(wideLatinText);
    Check(!_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(&latinUpper, &latinLower), __LINE__);
    Check(_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(&latinUpper, &wideLatin), __LINE__);
    Check(!_ZNK3WTF10StringImpl10startsWithERKS0_(&latinUpper, &wideLatin), __LINE__);

    Check(_ZN3WTF10StringImpl8endsWithEPS0_(&hello, &world), __LINE__);
    Check(!_ZN3WTF10StringImpl8endsWithEPS0_(&hello, &worldLower), __LINE__);
    Check(!_ZN3WTF10StringImpl8endsWithEPS0_(&hello, nullptr), __LINE__);
    Check(_ZN3WTF10StringImpl8endsWithEPS0_(&hello, &empty), __LINE__);
    Check(!_ZN3WTF10StringImpl8endsWithEPS0_(&world, &hello), __LINE__);
    Check(_ZN3WTF10StringImpl8endsWithERS0_(&hello, &hello), __LINE__);
    Check(!_ZN3WTF10StringImpl8endsWithERS0_(&hello, &helloPrefix), __LINE__);

    Check(_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseEPKS0_(&hello, &worldUpper), __LINE__);
    Check(!_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseEPKS0_(&hello, nullptr), __LINE__);
    Check(_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_(&hello, &worldLower), __LINE__);
    Check(!_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_(&hello, &helloPrefix), __LINE__);

    const std::u16string wideHelloText = u"Hello ĀWorld";
    StringImpl wideHello = Make16(wideHelloText);
    Check(_ZN3WTF10StringImpl8endsWithERS0_(&wideHello, &world), __LINE__);
    Check(_ZNK3WTF10StringImpl10startsWithERKS0_(&wideHello, &helloPrefix), __LINE__);
    Check(_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_(&wideHello, &worldUpper), __LINE__);
}

void TestInfix() {
    StringImpl hello = Make8("Hello World");
    StringImpl infix = Make8("lo W");
    StringImpl letter = Make8("o");
    StringImpl empty = Make8("");
    StringImpl world = Make8("World");

    Check(_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &infix, 3), __LINE__);
    Check(!_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &infix, 4), __LINE__);
    Check(!_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &letter, 11), __LINE__);
    Check(_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &empty, 11), __LINE__);
    Check(!_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &empty, 12), __LINE__);
    Check(!_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &world, 0xFFFFFFFFu), __LINE__);
    Check(_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &world, 6), __LINE__);

    Check(_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &infix, 7), __LINE__);
    Check(!_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &infix, 8), __LINE__);
    Check(!_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &infix, 3), __LINE__);
    Check(_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &world, 11), __LINE__);
    Check(!_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &world, 12), __LINE__);
    Check(_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &empty, 0), __LINE__);

    const std::u16string wideText = u"xĀlo Wy";
    StringImpl wide = Make16(wideText);
    Check(_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&wide, &infix, 2), __LINE__);
    Check(_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&wide, &infix, 6), __LINE__);
}

void TestFind() {
    StringImpl digits = Make8("ab1c2");
    Check(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsDigit, 0) == 2, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsDigit, 3) == 4, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsDigit, 5) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsDigit, 0xFFFFFFFFu) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsWide, 0) == NotFound, __LINE__);
    const std::u16string wideDigitsText = u"aĀb7";
    StringImpl wideDigits = Make16(wideDigitsText);
    Check(_ZN3WTF10StringImpl4findEPFbDsEj(&wideDigits, &IsWide, 0) == 1, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPFbDsEj(&wideDigits, &IsWide, 2) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPFbDsEj(&wideDigits, &IsDigit, 0) == 3, __LINE__);

    StringImpl haystack = Make8("abcabc");
    StringImpl ca = Make8("ca");
    StringImpl bc = Make8("bc");
    StringImpl c = Make8("c");
    StringImpl empty = Make8("");
    StringImpl tooLong = Make8("abcabcd");
    const std::u16string wideCharacterText = u"Ā";
    StringImpl wideCharacter = Make16(wideCharacterText);
    Check(_ZN3WTF10StringImpl4findEPS0_(&haystack, &ca) == 2, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_(&haystack, &c) == 2, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_(&haystack, &empty) == 0, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_(&haystack, &tooLong) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_(&haystack, nullptr) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_(&haystack, &wideCharacter) == NotFound, __LINE__);

    const std::u16string wideHaystackText = u"xĀyxĀy";
    StringImpl wideHaystack = Make16(wideHaystackText);
    StringImpl y = Make8("y");
    const std::u16string wideNeedleText = u"Āy";
    StringImpl wideNeedle = Make16(wideNeedleText);
    StringImpl yx = Make8("yx");
    Check(_ZN3WTF10StringImpl4findEPS0_(&wideHaystack, &wideCharacter) == 1, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_(&wideHaystack, &y) == 2, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_(&wideHaystack, &yx) == 2, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&wideHaystack, &wideNeedle, 2) == 4, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&wideHaystack, &wideCharacter, 2) == 4, __LINE__);

    Check(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &bc, 0) == 1, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &bc, 2) == 4, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &bc, 5) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &empty, 3) == 3, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &empty, 6) == 6, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &empty, 7) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &c, 3) == 5, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &c, 7) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl4findEPS0_j(&haystack, nullptr, 0) == NotFound, __LINE__);
}

void TestReverseFind() {
    StringImpl haystack = Make8("abcabc");
    StringImpl bc = Make8("bc");
    StringImpl a = Make8("a");
    StringImpl empty = Make8("");
    StringImpl tooLong = Make8("abcabcd");
    StringImpl emptyHaystack = Make8("");
    const std::u16string wideCharacterText = u"Ā";
    StringImpl wideCharacter = Make16(wideCharacterText);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &bc, MaxLength) == 4, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &bc, 3) == 1, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &bc, 1) == 1, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &bc, 0) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &empty, 10) == 6, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &empty, 2) == 2, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &a, 2) == 0, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &a, 100) == 3, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &tooLong, MaxLength) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, nullptr, MaxLength) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &wideCharacter, MaxLength) == NotFound, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&emptyHaystack, &a, MaxLength) == NotFound, __LINE__);

    const std::u16string wideHaystackText = u"ĀbcĀbc";
    StringImpl wideHaystack = Make16(wideHaystackText);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&wideHaystack, &wideCharacter, MaxLength) == 3, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&wideHaystack, &wideCharacter, 2) == 0, __LINE__);
    Check(_ZN3WTF10StringImpl11reverseFindEPS0_j(&wideHaystack, &bc, MaxLength) == 4, __LINE__);
}

void TestFindIgnoringASCIICase() {
    StringImpl hello = Make8("Hello World");
    StringImpl world = Make8("WORLD");
    StringImpl o = Make8("O");
    StringImpl empty = Make8("");
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_(&hello, &world) == 6, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_(&hello, nullptr) == NotFound, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j(&hello, &o, 5) == 7, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j(&hello, &world, 7) == NotFound, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j(&hello, nullptr, 0) == NotFound, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&hello, &o) == 4, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&hello, &empty) == 0, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&hello, &empty, 3) == 3, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&hello, &empty, 20) == 11, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&hello, &o, 12) == NotFound, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&hello, &world, 6) == 6, __LINE__);

    const std::u16string wideText = u"ÀBCbc";
    StringImpl wide = Make16(wideText);
    StringImpl bc = Make8("bc");
    StringImpl latin = Make8("\xE0");
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&wide, &bc) == 1, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&wide, &bc, 2) == 3, __LINE__);
    Check(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&wide, &latin) == NotFound, __LINE__);
}

void TestStringView() {
    const char* helloText = "Hello";
    StringView hello{helloText, 5, true};
    Check(_ZNK3WTF10StringView8containsEPKc(&hello, "ell"), __LINE__);
    Check(_ZNK3WTF10StringView8containsEPKc(&hello, "o"), __LINE__);
    Check(_ZNK3WTF10StringView8containsEPKc(&hello, ""), __LINE__);
    Check(!_ZNK3WTF10StringView8containsEPKc(&hello, "xyz"), __LINE__);
    Check(!_ZNK3WTF10StringView8containsEPKc(&hello, "hello"), __LINE__);
    Check(!_ZNK3WTF10StringView8containsEPKc(&hello, "Hello!"), __LINE__);
    StringView prefix{helloText, 3, true};
    Check(!_ZNK3WTF10StringView8containsEPKc(&prefix, "lo"), __LINE__);
    const std::u16string wideText = u"HeĀlo";
    StringView wide{wideText.data(), 5, false};
    Check(_ZNK3WTF10StringView8containsEPKc(&wide, "lo"), __LINE__);
    Check(!_ZNK3WTF10StringView8containsEPKc(&wide, "Hel"), __LINE__);
    StringView empty{nullptr, 0, true};
    Check(_ZNK3WTF10StringView8containsEPKc(&empty, ""), __LINE__);
    Check(!_ZNK3WTF10StringView8containsEPKc(&empty, "a"), __LINE__);
}

StringView View(const char* text) {
    return {text, static_cast<unsigned>(std::strlen(text)), true};
}

void TestProtocolIs() {
    Check(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("http://example.com"), "http"), __LINE__);
    Check(_ZN3WTF10protocolIsENS_10StringViewEPKc(View(" \t\x01HTTP:foo"), "http"), __LINE__);
    Check(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("ht\ntp:x"), "http"), __LINE__);
    Check(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("h\tt\rtp\n:"), "http"), __LINE__);
    Check(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("https://example.com"), "http"), __LINE__);
    Check(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("http"), "http"), __LINE__);
    Check(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View(""), "http"), __LINE__);
    Check(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("xhttp:"), "http"), __LINE__);
    Check(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("ht tp:"), "http"), __LINE__);
    Check(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("JavaScript:alert(1)"), "javascript"), __LINE__);
    Check(_ZN3WTF10protocolIsENS_10StringViewEPKc(View(":"), ""), __LINE__);
    Check(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("a:"), ""), __LINE__);
    Check(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("x-y+z:"), "x-y+z"), __LINE__);
    Check(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("x\x0dy+z:"), "x-y+z"), __LINE__);
    const std::u16string wideText = u" Data:Ā";
    Check(_ZN3WTF10protocolIsENS_10StringViewEPKc(StringView{wideText.data(), 7, false}, "data"), __LINE__);
    const std::u16string wideMismatchText = u"dŁta:";
    Check(!_ZN3WTF10protocolIsENS_10StringViewEPKc(StringView{wideMismatchText.data(), 5, false}, "data"), __LINE__);
}

void TestInvalidArguments() {
    StringImpl hello = Make8("Hello");
    Check(ThrowsInvalidArgument([&] { _ZNK3WTF10StringImpl10startsWithEPKS0_(nullptr, &hello); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZNK3WTF10StringImpl10startsWithERKS0_(&hello, nullptr); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZN3WTF10StringImpl8endsWithERS0_(&hello, nullptr); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZN3WTF10StringImpl4findEPS0_(nullptr, &hello); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZN3WTF10StringImpl4findEPFbDsEj(&hello, nullptr, 0); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&hello, nullptr); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZNK3WTF10StringView8containsEPKc(nullptr, "a"); }), __LINE__);
    StringView view = View("Hello");
    Check(ThrowsInvalidArgument([&] { _ZNK3WTF10StringView8containsEPKc(&view, nullptr); }), __LINE__);
    Check(ThrowsInvalidArgument([&] { _ZN3WTF10protocolIsENS_10StringViewEPKc(View("http:"), nullptr); }), __LINE__);
    Check(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View(" "), nullptr), __LINE__);
}

void TestDefaultWritingDirection() {
    bool strong = false;
    StringImpl latin = Make8("  123 abc");
    Check(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&latin, &strong) == 0 && strong, __LINE__);
    const std::u16string hebrewText = u" 12 \u05D0\u05D1 a";
    StringImpl hebrew = Make16(hebrewText);
    strong = false;
    Check(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&hebrew, &strong) == 1 && strong, __LINE__);
    const std::u16string arabicText = u"(\u0627)";
    StringImpl arabic = Make16(arabicText);
    strong = false;
    Check(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&arabic, &strong) == 1 && strong, __LINE__);
    const std::u16string neutralText = u" 42 !?";
    StringImpl neutral = Make16(neutralText);
    strong = true;
    Check(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&neutral, &strong) == 0 && !strong, __LINE__);
    StringImpl empty = Make8("");
    strong = true;
    Check(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&empty, &strong) == 0 && !strong, __LINE__);
    Check(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&hebrew, nullptr) == 1, __LINE__);
    const std::u16string surrogateText = u"\U00010900";
    StringImpl surrogate = Make16(surrogateText);
    strong = false;
    Check(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&surrogate, &strong) == 0 && strong, __LINE__);
    Check(ThrowsInvalidArgument([] { _ZN3WTF10StringImpl23defaultWritingDirectionEPb(nullptr, nullptr); }), __LINE__);
}

} // namespace

int main() {
    TestPrefixAndSuffix();
    TestInfix();
    TestFind();
    TestReverseFind();
    TestFindIgnoringASCIICase();
    TestStringView();
    TestProtocolIs();
    TestInvalidArguments();
    TestDefaultWritingDirection();
    return 0;
}

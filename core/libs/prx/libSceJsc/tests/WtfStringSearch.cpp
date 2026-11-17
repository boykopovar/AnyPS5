#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "Testing.hpp"

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

    Require(_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &helloPrefix));
    Require(!_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &lowerPrefix));
    Require(!_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &longer));
    Require(_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &empty));
    Require(_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, nullptr));
    Require(_ZNK3WTF10StringImpl10startsWithEPKS0_(&hello, &widePrefix));
    Require(_ZNK3WTF10StringImpl10startsWithERKS0_(&hello, &helloPrefix));
    Require(!_ZNK3WTF10StringImpl10startsWithERKS0_(&helloPrefix, &hello));

    Require(_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_(&hello, &upperPrefix));
    Require(!_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_(&hello, nullptr));
    Require(!_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseEPKS0_(&hello, &longer));
    Require(_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(&hello, &lowerPrefix));
    Require(!_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(&hello, &world));

    StringImpl latinUpper = Make8("\xC9t\xC9");
    StringImpl latinLower = Make8("\xE9t");
    const std::u16string wideLatinText = u"ÉT";
    StringImpl wideLatin = Make16(wideLatinText);
    Require(!_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(&latinUpper, &latinLower));
    Require(_ZNK3WTF10StringImpl27startsWithIgnoringASCIICaseERKS0_(&latinUpper, &wideLatin));
    Require(!_ZNK3WTF10StringImpl10startsWithERKS0_(&latinUpper, &wideLatin));

    Require(_ZN3WTF10StringImpl8endsWithEPS0_(&hello, &world));
    Require(!_ZN3WTF10StringImpl8endsWithEPS0_(&hello, &worldLower));
    Require(!_ZN3WTF10StringImpl8endsWithEPS0_(&hello, nullptr));
    Require(_ZN3WTF10StringImpl8endsWithEPS0_(&hello, &empty));
    Require(!_ZN3WTF10StringImpl8endsWithEPS0_(&world, &hello));
    Require(_ZN3WTF10StringImpl8endsWithERS0_(&hello, &hello));
    Require(!_ZN3WTF10StringImpl8endsWithERS0_(&hello, &helloPrefix));

    Require(_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseEPKS0_(&hello, &worldUpper));
    Require(!_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseEPKS0_(&hello, nullptr));
    Require(_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_(&hello, &worldLower));
    Require(!_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_(&hello, &helloPrefix));

    const std::u16string wideHelloText = u"Hello ĀWorld";
    StringImpl wideHello = Make16(wideHelloText);
    Require(_ZN3WTF10StringImpl8endsWithERS0_(&wideHello, &world));
    Require(_ZNK3WTF10StringImpl10startsWithERKS0_(&wideHello, &helloPrefix));
    Require(_ZNK3WTF10StringImpl25endsWithIgnoringASCIICaseERKS0_(&wideHello, &worldUpper));
}

void TestInfix() {
    StringImpl hello = Make8("Hello World");
    StringImpl infix = Make8("lo W");
    StringImpl letter = Make8("o");
    StringImpl empty = Make8("");
    StringImpl world = Make8("World");

    Require(_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &infix, 3));
    Require(!_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &infix, 4));
    Require(!_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &letter, 11));
    Require(_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &empty, 11));
    Require(!_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &empty, 12));
    Require(!_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &world, 0xFFFFFFFFu));
    Require(_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&hello, &world, 6));

    Require(_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &infix, 7));
    Require(!_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &infix, 8));
    Require(!_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &infix, 3));
    Require(_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &world, 11));
    Require(!_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &world, 12));
    Require(_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&hello, &empty, 0));

    const std::u16string wideText = u"xĀlo Wy";
    StringImpl wide = Make16(wideText);
    Require(_ZNK3WTF10StringImpl18hasInfixStartingAtERKS0_j(&wide, &infix, 2));
    Require(_ZNK3WTF10StringImpl16hasInfixEndingAtERKS0_j(&wide, &infix, 6));
}

void TestFind() {
    StringImpl digits = Make8("ab1c2");
    Require(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsDigit, 0) == 2);
    Require(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsDigit, 3) == 4);
    Require(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsDigit, 5) == NotFound);
    Require(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsDigit, 0xFFFFFFFFu) == NotFound);
    Require(_ZN3WTF10StringImpl4findEPFbDsEj(&digits, &IsWide, 0) == NotFound);
    const std::u16string wideDigitsText = u"aĀb7";
    StringImpl wideDigits = Make16(wideDigitsText);
    Require(_ZN3WTF10StringImpl4findEPFbDsEj(&wideDigits, &IsWide, 0) == 1);
    Require(_ZN3WTF10StringImpl4findEPFbDsEj(&wideDigits, &IsWide, 2) == NotFound);
    Require(_ZN3WTF10StringImpl4findEPFbDsEj(&wideDigits, &IsDigit, 0) == 3);

    StringImpl haystack = Make8("abcabc");
    StringImpl ca = Make8("ca");
    StringImpl bc = Make8("bc");
    StringImpl c = Make8("c");
    StringImpl empty = Make8("");
    StringImpl tooLong = Make8("abcabcd");
    const std::u16string wideCharacterText = u"Ā";
    StringImpl wideCharacter = Make16(wideCharacterText);
    Require(_ZN3WTF10StringImpl4findEPS0_(&haystack, &ca) == 2);
    Require(_ZN3WTF10StringImpl4findEPS0_(&haystack, &c) == 2);
    Require(_ZN3WTF10StringImpl4findEPS0_(&haystack, &empty) == 0);
    Require(_ZN3WTF10StringImpl4findEPS0_(&haystack, &tooLong) == NotFound);
    Require(_ZN3WTF10StringImpl4findEPS0_(&haystack, nullptr) == NotFound);
    Require(_ZN3WTF10StringImpl4findEPS0_(&haystack, &wideCharacter) == NotFound);

    const std::u16string wideHaystackText = u"xĀyxĀy";
    StringImpl wideHaystack = Make16(wideHaystackText);
    StringImpl y = Make8("y");
    const std::u16string wideNeedleText = u"Āy";
    StringImpl wideNeedle = Make16(wideNeedleText);
    StringImpl yx = Make8("yx");
    Require(_ZN3WTF10StringImpl4findEPS0_(&wideHaystack, &wideCharacter) == 1);
    Require(_ZN3WTF10StringImpl4findEPS0_(&wideHaystack, &y) == 2);
    Require(_ZN3WTF10StringImpl4findEPS0_(&wideHaystack, &yx) == 2);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&wideHaystack, &wideNeedle, 2) == 4);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&wideHaystack, &wideCharacter, 2) == 4);

    Require(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &bc, 0) == 1);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &bc, 2) == 4);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &bc, 5) == NotFound);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &empty, 3) == 3);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &empty, 6) == 6);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &empty, 7) == NotFound);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &c, 3) == 5);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&haystack, &c, 7) == NotFound);
    Require(_ZN3WTF10StringImpl4findEPS0_j(&haystack, nullptr, 0) == NotFound);
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
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &bc, MaxLength) == 4);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &bc, 3) == 1);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &bc, 1) == 1);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &bc, 0) == NotFound);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &empty, 10) == 6);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &empty, 2) == 2);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &a, 2) == 0);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &a, 100) == 3);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &tooLong, MaxLength) == NotFound);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, nullptr, MaxLength) == NotFound);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&haystack, &wideCharacter, MaxLength) == NotFound);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&emptyHaystack, &a, MaxLength) == NotFound);

    const std::u16string wideHaystackText = u"ĀbcĀbc";
    StringImpl wideHaystack = Make16(wideHaystackText);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&wideHaystack, &wideCharacter, MaxLength) == 3);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&wideHaystack, &wideCharacter, 2) == 0);
    Require(_ZN3WTF10StringImpl11reverseFindEPS0_j(&wideHaystack, &bc, MaxLength) == 4);
}

void TestFindIgnoringASCIICase() {
    StringImpl hello = Make8("Hello World");
    StringImpl world = Make8("WORLD");
    StringImpl o = Make8("O");
    StringImpl empty = Make8("");
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_(&hello, &world) == 6);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_(&hello, nullptr) == NotFound);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j(&hello, &o, 5) == 7);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j(&hello, &world, 7) == NotFound);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseEPKS0_j(&hello, nullptr, 0) == NotFound);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&hello, &o) == 4);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&hello, &empty) == 0);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&hello, &empty, 3) == 3);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&hello, &empty, 20) == 11);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&hello, &o, 12) == NotFound);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&hello, &world, 6) == 6);

    const std::u16string wideText = u"ÀBCbc";
    StringImpl wide = Make16(wideText);
    StringImpl bc = Make8("bc");
    StringImpl latin = Make8("\xE0");
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&wide, &bc) == 1);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_j(&wide, &bc, 2) == 3);
    Require(_ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&wide, &latin) == NotFound);
}

void TestStringView() {
    const char* helloText = "Hello";
    StringView hello{helloText, 5, true};
    Require(_ZNK3WTF10StringView8containsEPKc(&hello, "ell"));
    Require(_ZNK3WTF10StringView8containsEPKc(&hello, "o"));
    Require(_ZNK3WTF10StringView8containsEPKc(&hello, ""));
    Require(!_ZNK3WTF10StringView8containsEPKc(&hello, "xyz"));
    Require(!_ZNK3WTF10StringView8containsEPKc(&hello, "hello"));
    Require(!_ZNK3WTF10StringView8containsEPKc(&hello, "Hello!"));
    StringView prefix{helloText, 3, true};
    Require(!_ZNK3WTF10StringView8containsEPKc(&prefix, "lo"));
    const std::u16string wideText = u"HeĀlo";
    StringView wide{wideText.data(), 5, false};
    Require(_ZNK3WTF10StringView8containsEPKc(&wide, "lo"));
    Require(!_ZNK3WTF10StringView8containsEPKc(&wide, "Hel"));
    StringView empty{nullptr, 0, true};
    Require(_ZNK3WTF10StringView8containsEPKc(&empty, ""));
    Require(!_ZNK3WTF10StringView8containsEPKc(&empty, "a"));
}

StringView View(const char* text) {
    return {text, static_cast<unsigned>(std::strlen(text)), true};
}

void TestProtocolIs() {
    Require(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("http://example.com"), "http"));
    Require(_ZN3WTF10protocolIsENS_10StringViewEPKc(View(" \t\x01HTTP:foo"), "http"));
    Require(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("ht\ntp:x"), "http"));
    Require(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("h\tt\rtp\n:"), "http"));
    Require(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("https://example.com"), "http"));
    Require(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("http"), "http"));
    Require(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View(""), "http"));
    Require(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("xhttp:"), "http"));
    Require(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("ht tp:"), "http"));
    Require(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("JavaScript:alert(1)"), "javascript"));
    Require(_ZN3WTF10protocolIsENS_10StringViewEPKc(View(":"), ""));
    Require(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("a:"), ""));
    Require(_ZN3WTF10protocolIsENS_10StringViewEPKc(View("x-y+z:"), "x-y+z"));
    Require(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View("x\x0dy+z:"), "x-y+z"));
    const std::u16string wideText = u" Data:Ā";
    Require(_ZN3WTF10protocolIsENS_10StringViewEPKc(StringView{wideText.data(), 7, false}, "data"));
    const std::u16string wideMismatchText = u"dŁta:";
    Require(!_ZN3WTF10protocolIsENS_10StringViewEPKc(StringView{wideMismatchText.data(), 5, false}, "data"));
}

void TestInvalidArguments() {
    StringImpl hello = Make8("Hello");
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF10StringImpl10startsWithEPKS0_(nullptr, &hello); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF10StringImpl10startsWithERKS0_(&hello, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10StringImpl8endsWithERS0_(&hello, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10StringImpl4findEPS0_(nullptr, &hello); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10StringImpl4findEPFbDsEj(&hello, nullptr, 0); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF10StringImpl21findIgnoringASCIICaseERKS0_(&hello, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF10StringView8containsEPKc(nullptr, "a"); }));
    StringView view = View("Hello");
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF10StringView8containsEPKc(&view, nullptr); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF10protocolIsENS_10StringViewEPKc(View("http:"), nullptr); }));
    Require(!_ZN3WTF10protocolIsENS_10StringViewEPKc(View(" "), nullptr));
}

void TestDefaultWritingDirection() {
    bool strong = false;
    StringImpl latin = Make8("  123 abc");
    Require(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&latin, &strong) == 0 && strong);
    const std::u16string hebrewText = u" 12 \u05D0\u05D1 a";
    StringImpl hebrew = Make16(hebrewText);
    strong = false;
    Require(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&hebrew, &strong) == 1 && strong);
    const std::u16string arabicText = u"(\u0627)";
    StringImpl arabic = Make16(arabicText);
    strong = false;
    Require(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&arabic, &strong) == 1 && strong);
    const std::u16string neutralText = u" 42 !?";
    StringImpl neutral = Make16(neutralText);
    strong = true;
    Require(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&neutral, &strong) == 0 && !strong);
    StringImpl empty = Make8("");
    strong = true;
    Require(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&empty, &strong) == 0 && !strong);
    Require(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&hebrew, nullptr) == 1);
    const std::u16string surrogateText = u"\U00010900";
    StringImpl surrogate = Make16(surrogateText);
    strong = false;
    Require(_ZN3WTF10StringImpl23defaultWritingDirectionEPb(&surrogate, &strong) == 0 && strong);
    Require(ThrowsInvalidArgument([] { _ZN3WTF10StringImpl23defaultWritingDirectionEPb(nullptr, nullptr); }));
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

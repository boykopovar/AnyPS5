#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

using Version = std::array<std::uint8_t, 4>;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU version: %s\n", message);
    std::abort();
}

bool ExpectedInvariant(std::uint32_t unit) {
    if (unit > 0x7f) return false;
    for (const std::uint32_t variant : {0x0a, 0x21, 0x23, 0x24, 0x40, 0x5b, 0x5c, 0x5d, 0x5e, 0x60, 0x7b, 0x7c, 0x7d, 0x7e})
        if (unit == variant) return false;
    return true;
}

template<class F>
bool Rejects(F call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

Version FromString(const char* text) {
    Version version{9, 9, 9, 9};
    u_versionFromString_nid_postfix(version.data(), text);
    return version;
}

Version FromUString(const char16_t* text) {
    Version version{9, 9, 9, 9};
    u_versionFromUString_nid_postfix(version.data(), text);
    return version;
}

std::string ToString(Version version) {
    std::array<char, 21> text;
    text.fill('X');
    u_versionToString_nid_postfix(version.data(), text.data());
    const auto length = std::strlen(text.data());
    Require(length <= 15 && text[length + 1] == 'X', "version string overrun");
    return text.data();
}

void CharsToUChars() {
    std::array<char, 256> chars;
    for (std::size_t index = 0; index < chars.size(); ++index) chars[index] = static_cast<char>(index);
    std::array<char16_t, 257> units;
    units.fill(0x5555);
    u_charsToUChars_nid_postfix(chars.data(), units.data(), 256);
    for (std::size_t index = 0; index < chars.size(); ++index) Require(units[index] == index, "bytes widen without sign extension");
    Require(units[256] == 0x5555, "widening stays within the length");
    units.fill(0x5555);
    u_charsToUChars_nid_postfix(chars.data(), units.data(), 0);
    u_charsToUChars_nid_postfix(chars.data(), units.data(), -1);
    Require(units[0] == 0x5555, "nonpositive lengths write nothing");
    u_charsToUChars_nid_postfix(nullptr, nullptr, 0);
    u_charsToUChars_nid_postfix(nullptr, nullptr, -5);
    Require(Rejects([&] { u_charsToUChars_nid_postfix(nullptr, units.data(), 1); }), "null source rejected");
    Require(Rejects([&] { u_charsToUChars_nid_postfix(chars.data(), nullptr, 1); }), "null destination rejected");
}

void UCharsToChars() {
    std::array<char16_t, 0x10000> units;
    for (std::size_t index = 0; index < units.size(); ++index) units[index] = static_cast<char16_t>(index);
    std::array<char, 0x10001> chars;
    chars.fill('X');
    u_UCharsToChars_nid_postfix(units.data(), chars.data(), 0x10000);
    for (std::uint32_t unit = 0; unit < 0x10000; ++unit)
        Require(chars[unit] == (ExpectedInvariant(unit) ? static_cast<char>(unit) : 0), "only invariant characters narrow");
    Require(chars[0x10000] == 'X', "narrowing stays within the length");
    chars.fill('X');
    u_UCharsToChars_nid_postfix(units.data(), chars.data(), 0);
    u_UCharsToChars_nid_postfix(units.data(), chars.data(), -1);
    Require(chars[0] == 'X', "nonpositive lengths write nothing");
    u_UCharsToChars_nid_postfix(nullptr, nullptr, 0);
    Require(Rejects([&] { u_UCharsToChars_nid_postfix(nullptr, chars.data(), 1); }), "null source rejected");
    Require(Rejects([&] { u_UCharsToChars_nid_postfix(units.data(), nullptr, 1); }), "null destination rejected");
}

void AuStrcpy() {
    std::array<char, 8> chars;
    chars.fill('X');
    Require(u_austrcpy_nid_postfix(chars.data(), u"") == chars.data(), "empty copy returns destination");
    Require(chars[0] == 0 && chars[1] == 'X', "empty copy writes only terminator");

    chars.fill('X');
    Require(u_austrcpy_nid_postfix(chars.data(), u"Ab_9") == chars.data(), "ASCII copy returns destination");
    Require(std::memcmp(chars.data(), "Ab_9\0XXX", chars.size()) == 0, "ASCII characters and terminator copy");

    chars.fill('X');
    u_austrcpy_nid_postfix(chars.data(), u"A!#$@B");
    Require(std::memcmp(chars.data(), "A!#$@B\0X", chars.size()) == 0, "punctuation outside the invariant set copies unchanged");

    std::array<char16_t, 128> asciiUnits;
    for (std::size_t index = 0; index < 127; ++index) asciiUnits[index] = static_cast<char16_t>(index + 1);
    asciiUnits[127] = 0;
    std::array<char, 129> asciiChars;
    asciiChars.fill('X');
    Require(u_austrcpy_nid_postfix(asciiChars.data(), asciiUnits.data()) == asciiChars.data(), "full ASCII copy returns destination");
    for (std::size_t index = 0; index < 127; ++index)
        Require(asciiChars[index] == static_cast<char>(index + 1), "every unit in 0x01-0x7f copies unchanged");
    Require(asciiChars[127] == 0 && asciiChars[128] == 'X', "full ASCII copy writes terminator without overrun");

    Require(Rejects([&] { u_austrcpy_nid_postfix(nullptr, u"a"); }), "null destination rejected");
    Require(Rejects([&] { u_austrcpy_nid_postfix(chars.data(), nullptr); }), "null source rejected");
}

void AuStrncpy() {
    std::array<char, 8> chars;
    chars.fill('X');
    Require(u_austrncpy_nid_postfix(chars.data(), u"Abcdef", 3) == chars.data(), "truncated copy returns destination");
    Require(std::memcmp(chars.data(), "AbcXXXXX", chars.size()) == 0, "truncated copy omits terminator");

    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), u"Abc", 3);
    Require(std::memcmp(chars.data(), "AbcXXXXX", chars.size()) == 0, "exact-length copy omits terminator");

    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), u"Abc", 6);
    Require(std::memcmp(chars.data(), "Abc\0XXXX", chars.size()) == 0, "longer bound writes one terminator without padding");

    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), u"A!#$@", 7);
    Require(std::memcmp(chars.data(), "A!#$@\0XX", chars.size()) == 0, "non-invariant ASCII units copy unchanged within bound");

    std::array<char16_t, 128> asciiUnits;
    for (std::size_t index = 0; index < 127; ++index) asciiUnits[index] = static_cast<char16_t>(index + 1);
    asciiUnits[127] = 0;
    std::array<char, 129> asciiChars;
    asciiChars.fill('X');
    Require(u_austrncpy_nid_postfix(asciiChars.data(), asciiUnits.data(), 128) == asciiChars.data(), "bounded full ASCII copy returns destination");
    for (std::size_t index = 0; index < 127; ++index)
        Require(asciiChars[index] == static_cast<char>(index + 1), "every unit in 0x01-0x7f copies unchanged in bounded copy");
    Require(asciiChars[127] == 0 && asciiChars[128] == 'X', "bounded full ASCII copy writes terminator without overrun");

    const std::array<char16_t, 3> unterminated{u'!', u'$', u'@'};
    chars.fill('X');
    u_austrncpy_nid_postfix(chars.data(), unterminated.data(), 3);
    Require(std::memcmp(chars.data(), "!$@XXXXX", chars.size()) == 0, "bounded copy stops at count without reading past it");

    chars.fill('X');
    Require(u_austrncpy_nid_postfix(chars.data(), u"Abc", 0) == chars.data(), "zero count returns destination");
    Require(u_austrncpy_nid_postfix(chars.data(), u"Abc", -2) == chars.data(), "negative count returns destination");
    Require(chars[0] == 'X', "nonpositive counts write nothing");
    Require(u_austrncpy_nid_postfix(nullptr, nullptr, 0) == nullptr, "zero count allows null pointers");
    Require(u_austrncpy_nid_postfix(chars.data(), nullptr, -1) == chars.data(), "negative count allows null source");
    Require(Rejects([&] { u_austrncpy_nid_postfix(nullptr, u"a", 1); }), "null destination with positive count rejected");
    Require(Rejects([&] { u_austrncpy_nid_postfix(chars.data(), nullptr, 1); }), "null source with positive count rejected");
}

void VersionFromString() {
    Require(FromString("1.2.3.4") == Version{1, 2, 3, 4}, "four parts");
    Require(FromString("65.1") == Version{65, 1, 0, 0}, "missing parts are zero");
    Require(FromString("1.2.3.4.5") == Version{1, 2, 3, 4}, "parts after the fourth are ignored");
    Require(FromString("") == Version{0, 0, 0, 0}, "empty string");
    Require(FromString("x") == Version{0, 0, 0, 0}, "no digits");
    Require(FromString("3x.4") == Version{3, 0, 0, 0}, "parsing stops at a non-delimiter");
    Require(FromString("1..2") == Version{1, 0, 0, 0}, "empty part stops parsing");
    Require(FromString("1.2.") == Version{1, 2, 0, 0}, "trailing delimiter");
    Require(FromString(" 7. 8") == Version{7, 8, 0, 0}, "leading whitespace is skipped by strtoul");
    Require(FromString("+5.-1") == Version{5, 255, 0, 0}, "signs follow strtoul");
    Require(FromString("256.300.511") == Version{0, 44, 255, 0}, "parts truncate to eight bits");
    Require(FromString("99999999999999999999999.1") == Version{255, 1, 0, 0}, "overflow saturates before truncation");
    Require(FromString(nullptr) == Version{0, 0, 0, 0}, "null string clears the version");
    u_versionFromString_nid_postfix(nullptr, "1.2");
}

void VersionFromUString() {
    Require(FromUString(u"10.2.1") == Version{10, 2, 1, 0}, "UTF-16 version");
    Require(FromUString(u"") == Version{0, 0, 0, 0}, "empty UTF-16 string");
    Require(FromUString(u"1$2") == Version{1, 0, 0, 0}, "variant character becomes NUL");
    Require(FromUString(u"4.é5") == Version{4, 0, 0, 0}, "non-ASCII character becomes NUL");
    Require(FromUString(u"１.2") == Version{0, 0, 0, 0}, "fullwidth digit is not a digit");
    Require(FromUString((std::u16string(17, u'0') + u"1.2").c_str()) == Version{1, 2, 0, 0}, "twenty units are read");
    Require(FromUString((std::u16string(18, u'0') + u"1.2").c_str()) == Version{1, 0, 0, 0}, "units after the twentieth are ignored");
    Require(FromUString(nullptr) == Version{9, 9, 9, 9}, "null string leaves the version unchanged");
    u_versionFromUString_nid_postfix(nullptr, u"1.2");
}

void VersionToString() {
    Require(ToString({1, 2, 3, 4}) == "1.2.3.4", "four parts");
    Require(ToString({65, 1, 0, 0}) == "65.1", "trailing zero parts are omitted");
    Require(ToString({7, 0, 0, 0}) == "7.0", "at least two parts");
    Require(ToString({0, 0, 0, 0}) == "0.0", "zero version");
    Require(ToString({1, 0, 0, 5}) == "1.0.0.5", "inner zero parts are kept");
    Require(ToString({0, 0, 1, 0}) == "0.0.1", "leading zero parts are kept");
    Require(ToString({255, 199, 99, 10}) == "255.199.99.10", "multi-digit parts");
    Require(ToString({100, 105, 109, 110}) == "10.15.19.110", "a zero tens digit after the hundreds digit is dropped");
    std::array<char, 4> text{'X', 'X', 'X', 'X'};
    u_versionToString_nid_postfix(nullptr, text.data());
    Require(text[0] == 0 && text[1] == 'X', "null version writes an empty string");
    const Version version{1, 2, 0, 0};
    u_versionToString_nid_postfix(version.data(), nullptr);
}

void RoundTrip() {
    const auto written = [](std::uint32_t part) { return part < 100 || part % 100 >= 10; };
    for (std::uint32_t part = 0; part < 256; ++part) {
        if (!written(part) || !written(255 - part)) continue;
        const Version version{static_cast<std::uint8_t>(part), 3, static_cast<std::uint8_t>(255 - part), 1};
        Require(FromString(ToString(version).c_str()) == version, "string round trip");
    }
}

}

int main() {
    CharsToUChars();
    UCharsToChars();
    AuStrcpy();
    AuStrncpy();
    VersionFromString();
    VersionFromUString();
    VersionToString();
    RoundTrip();
    return 0;
}

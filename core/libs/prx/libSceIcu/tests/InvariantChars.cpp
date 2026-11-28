#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

void Require(bool value, const std::string& message) {
    if (!value) throw std::runtime_error("icu invariant chars: " + message);
}

char Byte(std::uint8_t value) { return static_cast<char>(value); }

bool Throws(void (*call)()) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

}

int main() {
    Require(u_asciiToUpper_nid_postfix(u'a') == u'A', "u_asciiToUpper did not raise a");
    Require(u_asciiToUpper_nid_postfix(u'z') == u'Z', "u_asciiToUpper did not raise z");
    Require(u_asciiToUpper_nid_postfix(u'`') == u'`', "u_asciiToUpper changed the character before a");
    Require(u_asciiToUpper_nid_postfix(u'{') == u'{', "u_asciiToUpper changed the character after z");
    Require(u_asciiToUpper_nid_postfix(u'Q') == u'Q', "u_asciiToUpper changed an uppercase letter");
    Require(u_asciiToUpper_nid_postfix(u'é') == u'é', "u_asciiToUpper changed a non-ASCII letter");
    Require(u_asciiToUpper_nid_postfix(static_cast<char16_t>(0xff41)) == static_cast<char16_t>(0xff41),
        "u_asciiToUpper changed a fullwidth letter");

    Require(uprv_toupper_nid_postfix('a') == 'A', "uprv_toupper did not raise a");
    Require(uprv_toupper_nid_postfix('z') == 'Z', "uprv_toupper did not raise z");
    Require(uprv_toupper_nid_postfix('`') == '`', "uprv_toupper changed the character before a");
    Require(uprv_toupper_nid_postfix('{') == '{', "uprv_toupper changed the character after z");
    Require(uprv_toupper_nid_postfix(Byte(0xe9)) == Byte(0xe9), "uprv_toupper changed a high byte");

    Require(uprv_asciitolower_nid_postfix('A') == 'a', "uprv_asciitolower did not lower A");
    Require(uprv_asciitolower_nid_postfix('Z') == 'z', "uprv_asciitolower did not lower Z");
    Require(uprv_asciitolower_nid_postfix('@') == '@', "uprv_asciitolower changed the character before A");
    Require(uprv_asciitolower_nid_postfix('[') == '[', "uprv_asciitolower changed the character after Z");
    Require(uprv_asciitolower_nid_postfix(Byte(0xc1)) == Byte(0xc1), "uprv_asciitolower changed a high byte");

    Require(uprv_ebcdictolower_nid_postfix(Byte(0xc1)) == Byte(0x81), "uprv_ebcdictolower did not lower EBCDIC A");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xc9)) == Byte(0x89), "uprv_ebcdictolower did not lower EBCDIC I");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xd1)) == Byte(0x91), "uprv_ebcdictolower did not lower EBCDIC J");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xd9)) == Byte(0x99), "uprv_ebcdictolower did not lower EBCDIC R");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xe2)) == Byte(0xa2), "uprv_ebcdictolower did not lower EBCDIC S");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xe9)) == Byte(0xa9), "uprv_ebcdictolower did not lower EBCDIC Z");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xc0)) == Byte(0xc0), "uprv_ebcdictolower changed 0xc0");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xca)) == Byte(0xca), "uprv_ebcdictolower changed 0xca");
    Require(uprv_ebcdictolower_nid_postfix(Byte(0xe1)) == Byte(0xe1), "uprv_ebcdictolower changed 0xe1");
    Require(uprv_ebcdictolower_nid_postfix('A') == 'A', "uprv_ebcdictolower changed ASCII A");

    Require(uprv_stricmp_nid_postfix("Hello", "hELLO") == 0, "uprv_stricmp did not ignore case");
    Require(uprv_stricmp_nid_postfix("A", "c") == 'a' - 'c', "uprv_stricmp did not return the lowercase difference");
    Require(uprv_stricmp_nid_postfix("[", "a") == '[' - 'a', "uprv_stricmp lowered a character outside A-Z");
    Require(uprv_stricmp_nid_postfix("ab", "a") == 1, "uprv_stricmp did not order the longer string after");
    Require(uprv_stricmp_nid_postfix("a", "ab") == -1, "uprv_stricmp did not order the shorter string before");
    Require(uprv_stricmp_nid_postfix(nullptr, nullptr) == 0, "uprv_stricmp did not treat two nulls as equal");
    Require(uprv_stricmp_nid_postfix(nullptr, "a") == -1, "uprv_stricmp did not order null first");
    Require(uprv_stricmp_nid_postfix("a", nullptr) == 1, "uprv_stricmp did not order null first");

    Require(uprv_strnicmp_nid_postfix("HELLOx", "helloy", 5) == 0, "uprv_strnicmp compared past n");
    Require(uprv_strnicmp_nid_postfix("HELLOx", "helloy", 6) == 'x' - 'y', "uprv_strnicmp stopped before n");
    Require(uprv_strnicmp_nid_postfix("a", "b", 0) == 0, "uprv_strnicmp compared with n of zero");
    Require(uprv_strnicmp_nid_postfix("ab", "a", 2) == 1, "uprv_strnicmp did not order the longer string after");
    Require(uprv_strnicmp_nid_postfix("Ab", "aB", 100) == 0, "uprv_strnicmp did not stop at the terminator");
    Require(uprv_strnicmp_nid_postfix(nullptr, "a", 1) == -1, "uprv_strnicmp did not order null first");

    Require(uprv_isInvariantString_nid_postfix("Hello, world.", -1) == 1, "uprv_isInvariantString rejected invariant text");
    Require(uprv_isInvariantString_nid_postfix("a@b", -1) == 0, "uprv_isInvariantString accepted @");
    Require(uprv_isInvariantString_nid_postfix("Hi!", -1) == 0, "uprv_isInvariantString accepted !");
    Require(uprv_isInvariantString_nid_postfix("abc@", 3) == 1, "uprv_isInvariantString read past length");
    const char high[] = {'a', Byte(0xc3), Byte(0xa9), 0};
    Require(uprv_isInvariantString_nid_postfix(high, -1) == 0, "uprv_isInvariantString accepted a high byte");
    Require(uprv_isInvariantString_nid_postfix(nullptr, 0) == 1, "uprv_isInvariantString rejected an empty null string");
    Require(Throws([] { uprv_isInvariantString_nid_postfix(nullptr, -1); }),
        "uprv_isInvariantString accepted a null string");

    Require(uprv_isInvariantUString_nid_postfix(u"Hello, world.", -1) == 1,
        "uprv_isInvariantUString rejected invariant text");
    Require(uprv_isInvariantUString_nid_postfix(u"a~", -1) == 0, "uprv_isInvariantUString accepted ~");
    Require(uprv_isInvariantUString_nid_postfix(u"café", -1) == 0, "uprv_isInvariantUString accepted a non-ASCII letter");
    Require(uprv_isInvariantUString_nid_postfix(u"abc~", 3) == 1, "uprv_isInvariantUString read past length");
    Require(uprv_isInvariantUString_nid_postfix(nullptr, 0) == 1, "uprv_isInvariantUString rejected an empty null string");
    Require(Throws([] { uprv_isInvariantUString_nid_postfix(nullptr, 2); }),
        "uprv_isInvariantUString accepted a null string");

    Require(uprv_ebcdicToAscii_nid_postfix(Byte(0xc1)) == 'A', "uprv_ebcdicToAscii did not convert EBCDIC A");
    Require(uprv_ebcdicToAscii_nid_postfix(Byte(0x81)) == 'a', "uprv_ebcdicToAscii did not convert EBCDIC a");
    Require(uprv_ebcdicToAscii_nid_postfix(Byte(0xe9)) == 'Z', "uprv_ebcdicToAscii did not convert EBCDIC Z");
    Require(uprv_ebcdicToAscii_nid_postfix(Byte(0xf0)) == '0', "uprv_ebcdicToAscii did not convert EBCDIC 0");
    Require(uprv_ebcdicToAscii_nid_postfix(Byte(0x40)) == ' ', "uprv_ebcdicToAscii did not convert the EBCDIC space");
    Require(uprv_ebcdicToAscii_nid_postfix(Byte(0x7c)) == '@', "uprv_ebcdicToAscii did not convert EBCDIC @");
    Require(uprv_ebcdicToAscii_nid_postfix(Byte(0x4a)) == 0, "uprv_ebcdicToAscii converted a byte outside its table");
    Require(uprv_ebcdicToAscii_nid_postfix(Byte(0xff)) == 0, "uprv_ebcdicToAscii converted 0xff");

    Require(uprv_ebcdicToLowercaseAscii_nid_postfix(Byte(0xc1)) == 'a', "uprv_ebcdicToLowercaseAscii did not lower EBCDIC A");
    Require(uprv_ebcdicToLowercaseAscii_nid_postfix(Byte(0xe9)) == 'z', "uprv_ebcdicToLowercaseAscii did not lower EBCDIC Z");
    Require(uprv_ebcdicToLowercaseAscii_nid_postfix(Byte(0x81)) == 'a', "uprv_ebcdicToLowercaseAscii changed EBCDIC a");
    Require(uprv_ebcdicToLowercaseAscii_nid_postfix(Byte(0xf0)) == '0', "uprv_ebcdicToLowercaseAscii did not convert EBCDIC 0");
    Require(uprv_ebcdicToLowercaseAscii_nid_postfix(Byte(0x4a)) == 0,
        "uprv_ebcdicToLowercaseAscii converted a byte outside its table");

    const std::uint8_t ebcdicHello[] = {0xc8, 0x85, 0x93, 0x93, 0x96, 0};
    std::uint8_t ascii[10];
    std::memset(ascii, 0x7f, sizeof(ascii));
    Require(uprv_aestrncpy_nid_postfix(ascii, ebcdicHello, -1) == ascii, "uprv_aestrncpy did not return its destination");
    Require(std::memcmp(ascii, "Hello", 6) == 0, "uprv_aestrncpy with n of -1 did not copy the string and its terminator");
    Require(ascii[6] == 0x7f, "uprv_aestrncpy with n of -1 wrote past the terminator");
    std::memset(ascii, 0x7f, sizeof(ascii));
    uprv_aestrncpy_nid_postfix(ascii, ebcdicHello, 3);
    Require(std::memcmp(ascii, "Hel", 3) == 0 && ascii[3] == 0x7f, "uprv_aestrncpy did not stop at n");
    std::memset(ascii, 0x7f, sizeof(ascii));
    uprv_aestrncpy_nid_postfix(ascii, ebcdicHello, 8);
    Require(std::memcmp(ascii, "Hello\0\0\0", 8) == 0 && ascii[8] == 0x7f, "uprv_aestrncpy did not pad to n with zeros");
    std::memset(ascii, 0x7f, sizeof(ascii));
    uprv_aestrncpy_nid_postfix(ascii, ebcdicHello, 0);
    Require(ascii[0] == 0x7f, "uprv_aestrncpy wrote with n of zero");
    uprv_aestrncpy_nid_postfix(ascii, ebcdicHello, -2);
    Require(ascii[0] == 0x7f, "uprv_aestrncpy wrote with n of -2");
    const std::uint8_t ebcdicUnmapped[] = {0xc1, 0x4a, 0xc2, 0};
    uprv_aestrncpy_nid_postfix(ascii, ebcdicUnmapped, -1);
    Require(ascii[0] == 'A' && ascii[1] == 0 && ascii[2] == 'B' && ascii[3] == 0,
        "uprv_aestrncpy did not write zero for a byte outside its table");
    Require(uprv_aestrncpy_nid_postfix(nullptr, ebcdicHello, 0) == nullptr, "uprv_aestrncpy refused a null destination it does not write");
    Require(Throws([] {
        const std::uint8_t source[] = {0xc1, 0};
        uprv_aestrncpy_nid_postfix(nullptr, source, 1);
    }), "uprv_aestrncpy accepted a null destination");
    Require(Throws([] {
        const std::uint8_t source[] = {0xc1, 0};
        uprv_aestrncpy_nid_postfix(nullptr, source, -1);
    }), "uprv_aestrncpy accepted a null destination with n of -1");
    Require(Throws([] {
        std::uint8_t destination[2];
        uprv_aestrncpy_nid_postfix(destination, nullptr, 0);
    }), "uprv_aestrncpy accepted a null source");

    const std::uint8_t asciiText[] = {'A', 'z', '0', '9', ' ', '.', 0};
    const std::uint8_t ebcdicText[] = {0xc1, 0xa9, 0xf0, 0xf9, 0x40, 0x4b, 0};
    std::uint8_t ebcdic[10];
    std::memset(ebcdic, 0x7f, sizeof(ebcdic));
    Require(uprv_eastrncpy_nid_postfix(ebcdic, asciiText, -1) == ebcdic, "uprv_eastrncpy did not return its destination");
    Require(std::memcmp(ebcdic, ebcdicText, 7) == 0, "uprv_eastrncpy with n of -1 did not copy the string and its terminator");
    Require(ebcdic[7] == 0x7f, "uprv_eastrncpy with n of -1 wrote past the terminator");
    std::memset(ebcdic, 0x7f, sizeof(ebcdic));
    uprv_eastrncpy_nid_postfix(ebcdic, asciiText, 2);
    Require(ebcdic[0] == 0xc1 && ebcdic[1] == 0xa9 && ebcdic[2] == 0x7f, "uprv_eastrncpy did not stop at n");
    std::memset(ebcdic, 0x7f, sizeof(ebcdic));
    uprv_eastrncpy_nid_postfix(ebcdic, asciiText, 9);
    Require(std::memcmp(ebcdic, ebcdicText, 6) == 0 && ebcdic[6] == 0 && ebcdic[7] == 0 && ebcdic[8] == 0 && ebcdic[9] == 0x7f,
        "uprv_eastrncpy did not pad to n with zeros");
    const std::uint8_t asciiVariant[] = {'a', '@', 0xe9, 'b', 0};
    uprv_eastrncpy_nid_postfix(ebcdic, asciiVariant, -1);
    Require(ebcdic[0] == 0x81 && ebcdic[1] == 0x6f && ebcdic[2] == 0x6f && ebcdic[3] == 0x82 && ebcdic[4] == 0,
        "uprv_eastrncpy did not substitute the EBCDIC question mark for a byte outside its table");
    Require(uprv_eastrncpy_nid_postfix(nullptr, asciiText, 0) == nullptr, "uprv_eastrncpy refused a null destination it does not write");
    Require(Throws([] {
        const std::uint8_t source[] = {'a', 0};
        uprv_eastrncpy_nid_postfix(nullptr, source, 1);
    }), "uprv_eastrncpy accepted a null destination");
    Require(Throws([] {
        std::uint8_t destination[2];
        uprv_eastrncpy_nid_postfix(destination, nullptr, -1);
    }), "uprv_eastrncpy accepted a null source");

    const char ebcdicUpperA[] = {Byte(0xc1), 0};
    const char ebcdicLowerA[] = {Byte(0x81), 0};
    const char ebcdicUpperAB[] = {Byte(0xc1), Byte(0xc2), 0};
    const char ebcdicOutside[] = {Byte(0x4a), 0};
    const char ebcdicAt[] = {Byte(0x7c), 0};
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix(ebcdicUpperAB, ebcdicUpperAB) == 0,
        "uprv_compareInvEbcdicAsAscii did not find equal strings equal");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix(ebcdicLowerA, ebcdicUpperA) == 'a' - 'A',
        "uprv_compareInvEbcdicAsAscii did not order by the ASCII values");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix(ebcdicUpperA, ebcdicUpperAB) == -'B',
        "uprv_compareInvEbcdicAsAscii did not order a prefix first");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix(ebcdicUpperAB, ebcdicUpperA) == 'B',
        "uprv_compareInvEbcdicAsAscii did not order the longer string after");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix(ebcdicOutside, ebcdicUpperA) == -0x4a - 'A',
        "uprv_compareInvEbcdicAsAscii did not negate a byte outside its table");
    Require(uprv_compareInvEbcdicAsAscii_nid_postfix(ebcdicAt, ebcdicUpperA) == -0x7c - 'A',
        "uprv_compareInvEbcdicAsAscii did not negate a byte that is not invariant");
    Require(Throws([] { uprv_compareInvEbcdicAsAscii_nid_postfix(nullptr, ""); }),
        "uprv_compareInvEbcdicAsAscii accepted a null first string");
    Require(Throws([] { uprv_compareInvEbcdicAsAscii_nid_postfix("", nullptr); }),
        "uprv_compareInvEbcdicAsAscii accepted a null second string");

    const char ebcdicLoose[] = {Byte(0xc1), Byte(0x6d), Byte(0x60), Byte(0x40), Byte(0x05), Byte(0x15), Byte(0x25), Byte(0x0b), Byte(0x0c), Byte(0x0d), Byte(0xc2), 0};
    const char ebcdicLowerAB[] = {Byte(0x81), Byte(0x82), 0};
    const char ebcdicLowerB[] = {Byte(0x82), 0};
    Require(uprv_compareEBCDICPropertyNames_nid_postfix(ebcdicLoose, ebcdicLowerAB) == 0,
        "uprv_compareEBCDICPropertyNames did not ignore case, separators and EBCDIC white space");
    Require(uprv_compareEBCDICPropertyNames_nid_postfix(ebcdicLowerA, ebcdicLowerB) == 0x81 - 0x82,
        "uprv_compareEBCDICPropertyNames did not return the lowercase difference");
    Require(uprv_compareEBCDICPropertyNames_nid_postfix(ebcdicLowerAB, ebcdicUpperA) == 0x82,
        "uprv_compareEBCDICPropertyNames did not order the longer name after");
    Require(uprv_compareEBCDICPropertyNames_nid_postfix("", "") == 0,
        "uprv_compareEBCDICPropertyNames did not match two empty names");
    Require(Throws([] { uprv_compareEBCDICPropertyNames_nid_postfix(nullptr, ""); }),
        "uprv_compareEBCDICPropertyNames accepted a null first name");
    Require(Throws([] { uprv_compareEBCDICPropertyNames_nid_postfix("", nullptr); }),
        "uprv_compareEBCDICPropertyNames accepted a null second name");
    return 0;
}

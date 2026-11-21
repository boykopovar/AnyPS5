#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
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
    return 0;
}

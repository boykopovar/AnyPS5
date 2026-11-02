#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

constexpr auto Invalid = static_cast<std::size_t>(-1);
constexpr auto Incomplete = static_cast<std::size_t>(-2);

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

static std::size_t Decode(const char* text, std::size_t count, std::uint16_t& value) {
    unsigned char state[16]{};
    value = 0xcccc;
    *__error_nid_postfix() = 0;
    return mbrtowc_nid_postfix(&value, text, count, state);
}

static void CheckDecoding() {
    std::uint16_t value = 0;
    Require(_Getmbcurmax_nid_postfix() == 3, "three bytes per character at most");
    Require(Decode("A", 1, value) == 1 && value == 'A', "ASCII");
    Require(Decode("\xc3\xa9", 2, value) == 2 && value == 0xe9, "two bytes");
    Require(Decode("\xe2\x82\xac", 3, value) == 3 && value == 0x20ac, "three bytes");
    Require(Decode("\xed\xa0\x80", 3, value) == 3 && value == 0xd800, "an encoded surrogate decodes");
    Require(Decode("\xef\xbf\xbf", 3, value) == 3 && value == 0xffff, "U+FFFF decodes");
    Require(Decode("\xc1\xbf", 2, value) == 2 && value == 0x7f, "an overlong form decodes");
    Require(Decode("\xc0\x80", 2, value) == 0 && value == 0, "an overlong NUL ends the string");
    Require(Decode("\xf0\x9f\x98\x80", 4, value) == Invalid && *__error_nid_postfix() == 86, "four-byte forms are rejected");
    Require(Decode("\x80", 1, value) == Invalid && *__error_nid_postfix() == 86, "a lone continuation byte");
    Require(Decode("\xe9", 1, value) == Incomplete, "a Latin-1 byte starts a three-byte form");
    Require(Decode("\xe9" "A", 2, value) == Invalid, "the Latin-1 byte is not followed by a continuation");
    Require(Decode("\xc3", 0, value) == Incomplete, "no bytes");
}

static void CheckState() {
    unsigned char state[16]{};
    std::uint16_t value = 0;
    Require(mbrtowc_nid_postfix(&value, "\xe2", 1, state) == Incomplete && !mbsinit_nid_postfix(state), "a partial character is pending");
    Require(mbrtowc_nid_postfix(&value, "\x82", 1, state) == Incomplete, "still pending");
    Require(mbrtowc_nid_postfix(&value, "\xac", 1, state) == 1 && value == 0x20ac && mbsinit_nid_postfix(state), "the last byte completes it");
    Require(mbrtowc_nid_postfix(&value, "\xc3", 1, state) == Incomplete, "pending again");
    Require(mbrtowc_nid_postfix(&value, "A", 1, state) == Invalid && !mbsinit_nid_postfix(state), "an error leaves the state pending");
    unsigned char pending[16];
    std::memcpy(pending, state, sizeof(pending));
    value = 0x7777;
    Require(mbrtowc_nid_postfix(&value, nullptr, 0, state) == Incomplete && std::memcmp(state, pending, sizeof(pending)) == 0,
        "a null string with no bytes to read is incomplete and leaves a pending state");
    *__error_nid_postfix() = 0;
    Require(mbrtowc_nid_postfix(&value, nullptr, 1, state) == Invalid && *__error_nid_postfix() == 86 && value == 0x7777
            && std::memcmp(state, pending, sizeof(pending)) == 0,
        "a null string in a pending state is an error and keeps the state");
    Require(mbrlen_nid_postfix(nullptr, 0, state) == 0 && mbsinit_nid_postfix(state), "mbrlen with a null string resets the state");
    unsigned char initial[16]{};
    Require(mbrtowc_nid_postfix(&value, nullptr, 0, state) == Incomplete && std::memcmp(state, initial, sizeof(initial)) == 0,
        "a null string with no bytes to read is incomplete in the initial state");
    Require(mbrtowc_nid_postfix(&value, nullptr, 1, state) == 0 && value == 0x7777 && std::memcmp(state, initial, sizeof(initial)) == 0,
        "a null string in the initial state converts nothing");
    Require(mbsinit_nid_postfix(nullptr) == 1, "a null state is initial");

    mbtowc_nid_postfix(nullptr, nullptr, 0);
    Require(mbtowc_nid_postfix(&value, "\xc3", 1) == -1, "mbtowc reports a partial character as -1");
    Require(mbtowc_nid_postfix(&value, "\xa9", 1) == 1 && value == 0xe9, "mbtowc keeps the partial character");
    Require(mbtowc_nid_postfix(&value, "\xe2\x82\xac", 8) == 3 && value == 0x20ac, "mbtowc limits the count to three bytes");
    mbtowc_nid_postfix(&value, "\xc3", 1);

    Require(mbtowc_nid_postfix(nullptr, nullptr, 0) == 0 && mbtowc_nid_postfix(&value, "A", 1) == 1, "a null string resets mbtowc");
    Require(mblen_nid_postfix("\xc3", 1) == -1 && mblen_nid_postfix("\xa9", 1) == -1, "mblen keeps no state");
    Require(mblen_nid_postfix("\xe2\x82\xac", 3) == 3 && mblen_nid_postfix(nullptr, 0) == 0, "mblen");
}

static void CheckEncoding() {
    char bytes[4] = {};
    Require(wcrtomb_nid_postfix(bytes, 0x7f, nullptr) == 1 && bytes[0] == 0x7f, "one byte");
    Require(wcrtomb_nid_postfix(bytes, 0xe9, nullptr) == 2 && std::memcmp(bytes, "\xc3\xa9", 2) == 0, "two bytes");
    Require(wcrtomb_nid_postfix(bytes, 0xdc00, nullptr) == 3 && std::memcmp(bytes, "\xed\xb0\x80", 3) == 0, "a surrogate is encoded alone");
    Require(wcrtomb_nid_postfix(nullptr, 0x20ac, nullptr) == 1, "a null buffer");
    Require(wctomb_nid_postfix(bytes, 0xffff) == 3 && std::memcmp(bytes, "\xef\xbf\xbf", 3) == 0 && wctomb_nid_postfix(nullptr, 1) == 0, "wctomb");
}

static void CheckStrings() {
    const char text[] = "a\xc3\xa9\xe2\x82\xac" "b";
    std::uint16_t wide[8];
    std::memset(wide, 0xcc, sizeof(wide));
    Require(mbstowcs_nid_postfix(wide, text, 8) == 4 && wide[1] == 0xe9 && wide[2] == 0x20ac && wide[4] == 0, "mbstowcs");
    Require(mbstowcs_nid_postfix(nullptr, text, 0) == 4, "mbstowcs counts characters");
    std::memset(wide, 0xcc, sizeof(wide));
    Require(mbstowcs_nid_postfix(wide, "a\xff", 8) == Invalid && wide[0] == 'a' && wide[1] == 0xcccc, "mbstowcs stops at an invalid byte");

    unsigned char state[16]{};
    const char* cursor = text;
    std::memset(wide, 0xcc, sizeof(wide));
    Require(mbsrtowcs_nid_postfix(wide, &cursor, 2, state) == 2 && cursor == text + 3, "mbsrtowcs advances by whole characters");
    cursor = text;
    std::memset(wide, 0xcc, sizeof(wide));
    Require(mbsrtowcs_nid_postfix(wide, &cursor, 0, state) == 0 && wide[0] == 0 && cursor == text, "mbsrtowcs clears the first element");
    const char invalid[] = "ab\xff";
    cursor = invalid;
    *__error_nid_postfix() = 0;
    Require(mbsrtowcs_nid_postfix(wide, &cursor, 8, state) == Invalid && cursor == invalid + 2 && *__error_nid_postfix() == 86, "mbsrtowcs points at the invalid byte");

    const std::uint16_t source[] = {'a', 0xe9, 0x20ac, 0};
    char narrow[8];
    std::memset(narrow, 'x', sizeof(narrow));
    Require(wcstombs_nid_postfix(narrow, source, 8) == 6 && std::memcmp(narrow, "a\xc3\xa9\xe2\x82\xac\0", 7) == 0, "wcstombs");
    std::memset(narrow, 'x', sizeof(narrow));
    Require(wcstombs_nid_postfix(narrow, source, 4) == 4 && std::memcmp(narrow, "a\xc3\xa9\xe2x", 5) == 0, "wcstombs fills the capacity mid-character");
    Require(wcstombs_nid_postfix(nullptr, source, 0) == 6, "wcstombs counts bytes");
}

static void CheckMbrlen() {
    unsigned char state[16]{};
    Require(mbrlen_nid_postfix("A", 1, state) == 1, "mbrlen counts an ASCII byte");
    Require(mbrlen_nid_postfix("\xc3\xa9", 2, state) == 2, "mbrlen counts a two-byte form");
    Require(mbrlen_nid_postfix("\xe2\x82\xac" "A", 4, state) == 3 && mbsinit_nid_postfix(state), "mbrlen counts one three-byte form");
    Require(mbrlen_nid_postfix("", 1, state) == 0 && mbsinit_nid_postfix(state), "mbrlen returns 0 for a NUL byte");
    Require(mbrlen_nid_postfix("A", 0, state) == Incomplete && mbsinit_nid_postfix(state), "mbrlen of no bytes is incomplete");
    Require(mbrlen_nid_postfix("\xe2\x82", 2, state) == Incomplete && !mbsinit_nid_postfix(state), "mbrlen keeps a partial form pending");
    Require(mbrlen_nid_postfix("\xac", 1, state) == 1 && mbsinit_nid_postfix(state), "mbrlen counts only the byte that completes the form");
    *__error_nid_postfix() = 0;
    Require(mbrlen_nid_postfix("\x80", 1, state) == Invalid && *__error_nid_postfix() == 86, "mbrlen rejects a lone continuation byte with EILSEQ");
    Require(mbrlen_nid_postfix(nullptr, 0, state) == 0 && mbsinit_nid_postfix(state), "mbrlen with a null string resets the state");

    std::uint16_t value = 0;
    Require(mbrlen_nid_postfix("\xe2", 1, nullptr) == Incomplete, "mbrlen keeps a partial form in its own internal state");
    Require(mbrtowc_nid_postfix(&value, "A", 1, nullptr) == 1 && value == 'A', "mbrtowc does not share mbrlen's internal state");
    Require(mbrlen_nid_postfix("\x82\xac", 2, nullptr) == 2, "mbrlen completes the form from its internal state");
}

int main() {
    CheckDecoding();
    CheckState();
    CheckMbrlen();
    CheckEncoding();
    CheckStrings();
    return 0;
}

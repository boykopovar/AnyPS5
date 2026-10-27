#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::exit(1);
    }
}

int main() {
    const std::uint16_t text[] = {'a', 'b', 'c', 0};
    const std::uint16_t empty[] = {0};
    const std::uint16_t twoBytes[] = {'a', 0x100, 'b', 0};
    const std::uint16_t bytes[] = {0x7f, 0x80, 0xe9, 0xff, 0};
    std::uint64_t state[2]{};
    char out[8];
    const auto* source = text;
    std::memset(out, 'x', sizeof(out));
    *__error_nid_postfix() = 7;
    Require(wcsrtombs_nid_postfix(out, &source, sizeof(out), state) == 3, "full conversion length");
    Require(source == nullptr && std::memcmp(out, "abc\0xxxx", 8) == 0, "full conversion and terminator");
    Require(*__error_nid_postfix() == 7, "successful conversion preserves errno");

    source = text;
    std::memset(out, 'x', sizeof(out));
    Require(wcsrtombs_nid_postfix(out, &source, 0, nullptr) == 0, "zero capacity length");
    Require(source == text && out[0] == 'x', "zero capacity leaves input and output alone");
    Require(wcsrtombs_nid_postfix(out, &source, 2, nullptr) == 2, "partial conversion length");
    Require(source == text + 2 && std::memcmp(out, "abxxxxxx", 8) == 0, "partial conversion has no terminator");
    Require(wcsrtombs_nid_postfix(out + 2, &source, 1, nullptr) == 1, "exact capacity length");
    Require(source == text + 3 && std::memcmp(out, "abcxxxxx", 8) == 0, "exact capacity leaves terminator pending");
    Require(wcsrtombs_nid_postfix(out + 3, &source, 1, nullptr) == 0, "resume at terminator");
    Require(source == nullptr && std::memcmp(out, "abc\0xxxx", 8) == 0, "resumed conversion terminates");

    source = empty;
    std::memset(out, 'x', sizeof(out));
    Require(wcsrtombs_nid_postfix(out, &source, 1, state) == 0, "empty string length");
    Require(source == nullptr && out[0] == 0 && out[1] == 'x', "empty string terminator");
    source = bytes;
    Require(wcsrtombs_nid_postfix(out, &source, sizeof(out), state) == 7, "UTF-8 conversion length");
    Require(source == nullptr && std::memcmp(out, "\x7f\xc2\x80\xc3\xa9\xc3\xbf\0", 8) == 0, "high characters become two UTF-8 bytes");

    source = text;
    Require(wcsrtombs_nid_postfix(nullptr, &source, 0, state) == 3 && source == text, "length query ignores capacity and preserves source");
    Require(wcsrtombs_nid_postfix(nullptr, &source, 1, nullptr) == 3 && source == text, "length query with implicit state");
    source = empty;
    Require(wcsrtombs_nid_postfix(nullptr, &source, 0, state) == 0 && source == empty, "empty length query");

    source = twoBytes;
    std::memset(out, 'x', sizeof(out));
    *__error_nid_postfix() = 7;
    Require(wcsrtombs_nid_postfix(out, &source, sizeof(out), state) == 4, "a character above 0xff converts");
    Require(source == nullptr && std::memcmp(out, "a\xc4\x80" "b\0", 5) == 0, "U+0100 becomes C4 80");
    Require(*__error_nid_postfix() == 7, "conversion preserves errno");
    source = twoBytes;
    Require(wcsrtombs_nid_postfix(nullptr, &source, 0, state) == 4 && source == twoBytes, "length query counts UTF-8 bytes");
    source = twoBytes;
    std::memset(out, 'x', sizeof(out));
    Require(wcsrtombs_nid_postfix(out, &source, 2, state) == 1 && source == twoBytes + 1, "a character that does not fit stops the conversion");
    Require(std::memcmp(out, "axxxxxxx", 8) == 0, "no partial character is written");
}

#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>

namespace {

const int facet = 0;
const int guestIllegalSequence = 86;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal wide ctype: %s\n", message);
    std::abort();
}

std::uint16_t ExpectedWiden(int value) {
    const auto byte = static_cast<unsigned char>(value);
    return byte < 0x80 ? byte : 0xffff;
}

bool InvalidByte(int value) {
    const auto byte = static_cast<unsigned char>(value);
    return (byte >= 0x80 && byte < 0xc0) || byte >= 0xf0;
}

void CheckWiden() {
    for (int value = CHAR_MIN; value <= CHAR_MAX; ++value) {
        errno = 0;
        Require(sceLibcInternalWideCtypeWiden_nid_no_patch(&facet, static_cast<char>(value)) == ExpectedWiden(value), "do_widen(char) is btowc of the byte");
        Require(errno == (InvalidByte(value) ? guestIllegalSequence : 0), "do_widen(char) reports an invalid byte as btowc does");
    }
    const char source[] = {'A', '\0', 'z', '\x7f', '\x80', '\xc3', '\xff', ' '};
    std::array<std::uint16_t, sizeof(source) + 1> destination{};
    destination.fill(0x2323);
    Require(sceLibcInternalWideCtypeWidenRange_nid_no_patch(&facet, source, source + sizeof(source), destination.data()) == source + sizeof(source), "do_widen(range) returns last");
    const std::array<std::uint16_t, sizeof(source)> widened{'A', 0, 'z', 0x7f, 0xffff, 0xffff, 0xffff, ' '};
    Require(std::memcmp(destination.data(), widened.data(), sizeof(widened)) == 0 && destination.back() == 0x2323, "do_widen(range) widens each byte and stops at last");
    destination.fill(0x2323);
    Require(sceLibcInternalWideCtypeWidenRange_nid_no_patch(&facet, source + 2, source + 5, destination.data()) == source + 5, "do_widen(subrange) returns last");
    Require(destination[0] == 'z' && destination[1] == 0x7f && destination[2] == 0xffff && destination[3] == 0x2323, "do_widen(subrange) writes only the subrange");
    Require(sceLibcInternalWideCtypeWidenRange_nid_no_patch(&facet, source, source, nullptr) == source, "an empty do_widen range writes nothing");
}

void CheckNarrow() {
    for (std::uint32_t value = 0; value <= 0xffff; ++value) {
        const auto wide = static_cast<std::uint16_t>(value);
        const char expected = static_cast<char>(value);
        Require(sceLibcInternalWideCtypeNarrow_nid_no_patch(&facet, wide, '?') == (value < 0x80 ? expected : '?'), "do_narrow(wchar_t) is wctob or the default");
        Require(sceLibcInternalWideCtypeNarrow_nid_no_patch(&facet, wide, '\x80') == (value < 0x80 ? expected : '\x80'), "do_narrow(wchar_t) returns a non-ASCII default");
    }
    const std::uint16_t source[] = {'A', 0, 0x7f, 0x80, 0xe9, 0x20ac, 0xffff, 'q'};
    std::array<char, std::size(source) + 1> destination{};
    destination.fill('#');
    Require(sceLibcInternalWideCtypeNarrowRange_nid_no_patch(&facet, source, source + std::size(source), '*', destination.data()) == source + std::size(source), "do_narrow(range) returns last");
    const char narrowed[] = {'A', '\0', '\x7f', '*', '*', '*', '*', 'q'};
    Require(std::memcmp(destination.data(), narrowed, sizeof(narrowed)) == 0 && destination.back() == '#', "do_narrow(range) narrows each element and stops at last");
    destination.fill('#');
    Require(sceLibcInternalWideCtypeNarrowRange_nid_no_patch(&facet, source + 1, source + 4, '\0', destination.data()) == source + 4, "do_narrow(subrange) returns last");
    Require(destination[0] == '\0' && destination[1] == '\x7f' && destination[2] == '\0' && destination[3] == '#', "do_narrow(subrange) writes only the subrange");
    Require(sceLibcInternalWideCtypeNarrowRange_nid_no_patch(&facet, source, source, '?', nullptr) == source, "an empty do_narrow range writes nothing");
}

}

int main() {
    CheckWiden();
    CheckNarrow();
    return 0;
}

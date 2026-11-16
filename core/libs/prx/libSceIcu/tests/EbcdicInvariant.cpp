#include "prx/libSceIcu/include/InventoryExports.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&uprv_ebcdicToAscii_nid_postfix), char (APS5_VABI*)(char)>);
static_assert(std::is_same_v<decltype(&uprv_ebcdicToLowercaseAscii_nid_postfix), char (APS5_VABI*)(char)>);
static_assert(std::is_same_v<decltype(&uprv_compareInvEbcdicAsAscii_nid_postfix), std::int32_t (APS5_VABI*)(const char*, const char*)>);
static_assert(std::is_same_v<decltype(&uprv_aestrncpy_nid_postfix), std::uint8_t* (APS5_VABI*)(std::uint8_t*, const std::uint8_t*, std::int32_t)>);
static_assert(std::is_same_v<decltype(&uprv_eastrncpy_nid_postfix), std::uint8_t* (APS5_VABI*)(std::uint8_t*, const std::uint8_t*, std::int32_t)>);

constexpr std::uint8_t Filler = 0xa5;
constexpr char Invariants[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 \"%&'()*+,-./:;<=>?_";

std::uint8_t ToAscii(std::uint8_t value) {
    return static_cast<std::uint8_t>(uprv_ebcdicToAscii_nid_postfix(static_cast<char>(value)));
}

std::uint8_t ToLowerAscii(std::uint8_t value) {
    return static_cast<std::uint8_t>(uprv_ebcdicToLowercaseAscii_nid_postfix(static_cast<char>(value)));
}

const std::uint8_t* Bytes(const char* text) {
    return reinterpret_cast<const std::uint8_t*>(text);
}

void Characters() {
    Require(ToAscii(0xc1) == 'A' && ToAscii(0xc9) == 'I' && ToAscii(0xd1) == 'J' && ToAscii(0xd9) == 'R');
    Require(ToAscii(0xe2) == 'S' && ToAscii(0xe9) == 'Z' && ToAscii(0x81) == 'a' && ToAscii(0xa9) == 'z');
    Require(ToAscii(0xf0) == '0' && ToAscii(0xf9) == '9' && ToAscii(0x40) == ' ' && ToAscii(0x6d) == '_');
    Require(ToAscii(0x00) == 0 && ToAscii(0x04) == 0 && ToAscii(0xff) == 0 && ToAscii(0x15) == 0x0a);
    Require(ToAscii(0xe0) == 0x5c && ToLowerAscii(0xe0) == 0x7c);
    Require(ToLowerAscii(0xc1) == 'a' && ToLowerAscii(0xe9) == 'z' && ToLowerAscii(0x81) == 'a' && ToLowerAscii(0xf5) == '5');
    for (int value = 0; value < 256; ++value) {
        const std::uint8_t ascii = ToAscii(static_cast<std::uint8_t>(value));
        const std::uint8_t lower = ToLowerAscii(static_cast<std::uint8_t>(value));
        if (value == 0xe0) continue;
        Require(lower == ('A' <= ascii && ascii <= 'Z' ? ascii + 0x20 : ascii));
    }
}

void Copies() {
    std::array<std::uint8_t, 128> ebcdic;
    std::array<std::uint8_t, 128> ascii;
    ebcdic.fill(Filler);
    ascii.fill(Filler);
    const std::size_t length = std::strlen(Invariants);
    Require(uprv_eastrncpy_nid_postfix(ebcdic.data(), Bytes(Invariants), -1) == ebcdic.data());
    Require(ebcdic[length] == 0 && ebcdic[length + 1] == Filler);
    Require(ebcdic[0] == 0xc1 && ebcdic[26] == 0x81 && ebcdic[52] == 0xf0 && ebcdic[62] == 0x40);
    Require(uprv_aestrncpy_nid_postfix(ascii.data(), ebcdic.data(), -1) == ascii.data());
    Require(std::memcmp(ascii.data(), Invariants, length + 1) == 0 && ascii[length + 1] == Filler);

    ascii.fill(Filler);
    const std::uint8_t source[] = {0xc1, 0xc2, 0xc3, 0};
    uprv_aestrncpy_nid_postfix(ascii.data(), source, 2);
    Require(ascii[0] == 'A' && ascii[1] == 'B' && ascii[2] == Filler);
    uprv_aestrncpy_nid_postfix(ascii.data(), source, 6);
    Require(std::memcmp(ascii.data(), "ABC\0\0\0", 6) == 0 && ascii[6] == Filler);
    ascii.fill(Filler);
    uprv_aestrncpy_nid_postfix(ascii.data(), source, 0);
    uprv_aestrncpy_nid_postfix(ascii.data(), source, -2);
    Require(ascii[0] == Filler);
    const std::uint8_t unmapped[] = {0xc1, 0x04, 0xc2, 0};
    uprv_aestrncpy_nid_postfix(ascii.data(), unmapped, -1);
    Require(ascii[0] == 'A' && ascii[1] == 0 && ascii[2] == 'B' && ascii[3] == 0 && ascii[4] == Filler);

    ebcdic.fill(Filler);
    uprv_eastrncpy_nid_postfix(ebcdic.data(), Bytes("A!\x80z"), 6);
    Require(ebcdic[0] == 0xc1 && ebcdic[1] == 0x6f && ebcdic[2] == 0x6f && ebcdic[3] == 0xa9);
    Require(ebcdic[4] == 0 && ebcdic[5] == 0 && ebcdic[6] == Filler);
    ebcdic.fill(Filler);
    uprv_eastrncpy_nid_postfix(ebcdic.data(), Bytes("AB"), 1);
    uprv_eastrncpy_nid_postfix(ebcdic.data() + 4, Bytes("AB"), 0);
    Require(ebcdic[0] == 0xc1 && ebcdic[1] == Filler && ebcdic[4] == Filler);
}

void Comparison() {
    const auto compare = uprv_compareInvEbcdicAsAscii_nid_postfix;
    Require(compare("", "") == 0);
    Require(compare("\xc1\x81\xf0", "\xc1\x81\xf0") == 0);
    Require(compare("\xc1", "\x81") == 'A' - 'a');
    Require(compare("\x81", "\xf0") == 'a' - '0');
    Require(compare("\xf0", "\xc1") == '0' - 'A');
    Require(compare("\xc1", "") == 'A');
    Require(compare("", "\xc1") == -'A');
    Require(compare("\xc1\xc2", "\xc1") == 'B');
    Require(compare("\x04", "\xc1") == -0x04 - 'A');
    Require(compare("\xc1", "\xff") == 'A' + 0xff);
    Require(compare("\x04", "\x06") == 2);
    Require(compare("\x7c", "\xc1") == -0x7c - 'A');
    Require(compare("\x04", "") == -0x04);
}

}

int main() {
    Characters();
    Copies();
    Comparison();
    std::array<std::uint8_t, 8> buffer{};
    RequireThrows([&] { uprv_aestrncpy_nid_postfix(nullptr, buffer.data(), 1); });
    RequireThrows([&] { uprv_aestrncpy_nid_postfix(buffer.data(), nullptr, 1); });
    RequireThrows([&] { uprv_eastrncpy_nid_postfix(nullptr, buffer.data(), 1); });
    RequireThrows([&] { uprv_eastrncpy_nid_postfix(buffer.data(), nullptr, 1); });
    RequireThrows([] { uprv_compareInvEbcdicAsAscii_nid_postfix(nullptr, ""); });
    RequireThrows([] { uprv_compareInvEbcdicAsAscii_nid_postfix("", nullptr); });
    return 0;
}

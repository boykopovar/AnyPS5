#include "prx/libc/include/DebugNidBreakpoints.hpp"

#include <array>

namespace {

constexpr char kAlphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";

int DecodeChar(char c) {
    for (int i = 0; i < 64; ++i) {
        if (kAlphabet[i] == c)
            return i;
    }
    return -1;
}

}

namespace Debug {

bool ParseNid(std::string_view text, std::uint64_t& nid) {
    if (text.size() != 11)
        return false;

    std::array<int, 11> digits{};
    for (std::size_t i = 0; i < digits.size(); ++i) {
        digits[i] = DecodeChar(text[i]);
        if (digits[i] < 0)
            return false;
    }

    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t group = 0; group < 2; ++group) {
        const std::size_t c = group * 4;
        const auto triple = static_cast<std::uint32_t>((digits[c] << 18) | (digits[c + 1] << 12) |
                                                       (digits[c + 2] << 6) | digits[c + 3]);
        bytes[group * 3] = static_cast<std::uint8_t>(triple >> 16);
        bytes[group * 3 + 1] = static_cast<std::uint8_t>(triple >> 8);
        bytes[group * 3 + 2] = static_cast<std::uint8_t>(triple);
    }

    const auto tail = static_cast<std::uint32_t>((digits[8] << 12) | (digits[9] << 6) | digits[10]);
    if ((tail & 3u) != 0)
        return false;
    bytes[6] = static_cast<std::uint8_t>(tail >> 10);
    bytes[7] = static_cast<std::uint8_t>(tail >> 2);

    nid = 0;
    for (std::uint8_t byte : bytes)
        nid = (nid << 8) | byte;
    return true;
}

std::string FormatNid(std::uint64_t nid) {
    std::array<std::uint8_t, 8> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>(nid >> (56 - 8 * i));

    std::string text;
    text.reserve(11);
    for (std::size_t group = 0; group < 2; ++group) {
        const auto triple = (static_cast<std::uint32_t>(bytes[group * 3]) << 16) |
                            (static_cast<std::uint32_t>(bytes[group * 3 + 1]) << 8) |
                            static_cast<std::uint32_t>(bytes[group * 3 + 2]);
        text.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        text.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        text.push_back(kAlphabet[(triple >> 6) & 0x3F]);
        text.push_back(kAlphabet[triple & 0x3F]);
    }
    const auto tail = (static_cast<std::uint32_t>(bytes[6]) << 16) | (static_cast<std::uint32_t>(bytes[7]) << 8);
    text.push_back(kAlphabet[(tail >> 18) & 0x3F]);
    text.push_back(kAlphabet[(tail >> 12) & 0x3F]);
    text.push_back(kAlphabet[(tail >> 6) & 0x3F]);
    return text;
}

}

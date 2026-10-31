#include "prx/libSceNKWebKit/include/UrlHelpers.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

namespace {

using NKWebKit::UrlAbi::StringView;

char16_t CharacterAt(StringView value, std::uint32_t index) {
    if (value.is8Bit) return static_cast<const std::uint8_t*>(value.characters)[index];
    return static_cast<const char16_t*>(value.characters)[index];
}

std::u16string CopyString(StringView value) {
    if (!value.characters && value.length) throw std::invalid_argument("WTF::StringView: null characters with nonzero length");
    std::u16string result;
    result.reserve(value.length);
    for (std::uint32_t i = 0; i < value.length; ++i) result.push_back(CharacterAt(value, i));
    return result;
}

bool AsciiWhitespace(char16_t value) {
    return value == ' ' || (value >= '\t' && value <= '\r');
}

std::optional<std::uint64_t> HexGroup(StringView value, std::uint32_t start, std::uint32_t count, bool rejectLeadingPlus) {
    std::uint32_t end = start + count;
    if (rejectLeadingPlus && CharacterAt(value, start) == '+') return std::nullopt;
    while (start < end && AsciiWhitespace(CharacterAt(value, start))) ++start;
    if (start < end && CharacterAt(value, start) == '+') ++start;
    std::uint64_t result = 0;
    bool haveDigits = false;
    while (start < end) {
        const char16_t c = CharacterAt(value, start);
        std::uint32_t digit = 0;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else break;
        haveDigits = true;
        result = result * 16 + digit;
        ++start;
    }
    if (!haveDigits) return std::nullopt;
    while (start < end && AsciiWhitespace(CharacterAt(value, start))) ++start;
    if (start != end) return std::nullopt;
    return result;
}

bool UuidVersion4(StringView value) {
    if (value.length != 36) return false;
    if (!value.characters) throw std::invalid_argument("WTF::StringView: null UUID characters");
    for (const auto separator : {8u, 13u, 18u, 23u}) {
        if (CharacterAt(value, separator) != '-') return false;
    }
    const auto first = HexGroup(value, 0, 8, true);
    const auto second = HexGroup(value, 9, 4, true);
    const auto third = HexGroup(value, 14, 4, false);
    const auto fourth = HexGroup(value, 19, 4, true);
    const auto fifth = HexGroup(value, 24, 12, true);
    if (!first || !second || !third || !fourth || !fifth) return false;
    return ((*third & 0xf000u) == 0x4000u) && ((*fourth & 0xc000u) == 0x8000u);
}

std::optional<std::uint16_t> DefaultPort(StringView protocol) {
    if (!protocol.characters && protocol.length) throw std::invalid_argument("WTF::StringView: null protocol characters");
    const auto key = CopyString(protocol);
    if (key == u"ftp") return 21;
    if (key == u"http" || key == u"ws") return 80;
    if (key == u"https" || key == u"wss") return 443;
    return std::nullopt;
}

}

extern "C" {

bool APS5_VABI _ZN3WTF14isVersion4UUIDENS_10StringViewE(StringView value) {
    return UuidVersion4(value);
}

bool APS5_VABI _ZN3WTF24isDefaultPortForProtocolEtNS_10StringViewE(std::uint16_t port, StringView protocol) {
    return DefaultPort(protocol) == port;
}

}

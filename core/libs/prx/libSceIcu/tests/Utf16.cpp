#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>
#include "prx/libSceIcu/libSceIcu.hpp"



namespace {

void Require(bool value) {
    if (!value) throw std::runtime_error("ICU UTF-16 scalar test failed");
}

bool Pair(std::uint16_t first, std::uint16_t second) {
    return (first & 0xfc00) == 0xd800 && (second & 0xfc00) == 0xdc00;
}

std::size_t TerminatedLength(const std::vector<std::uint16_t>& text) {
    return std::find(text.begin(), text.end(), 0) - text.begin();
}

const std::uint16_t* ExpectedFind(const std::vector<std::uint16_t>& text, std::int32_t scalar, std::size_t length, bool last) {
    const std::uint16_t* result = nullptr;
    if (static_cast<std::uint32_t>(scalar) > 0x10ffff) return result;
    for (std::size_t i = 0; i < length; ++i) {
        bool match;
        if (scalar < 0x10000) {
            match = text[i] == scalar;
            if (i != 0 && Pair(text[i - 1], text[i])) match = false;
            if (i + 1 < length && Pair(text[i], text[i + 1])) match = false;
        } else {
            const auto value = static_cast<std::uint32_t>(scalar) - 0x10000;
            match = i + 1 < length && text[i] == 0xd800 + (value >> 10) && text[i + 1] == 0xdc00 + (value & 0x3ff);
        }
        if (match) {
            result = text.data() + i;
            if (!last) break;
        }
    }
    return result;
}

void Check(std::vector<std::uint16_t> text) {
    const auto payload = text.size();
    text.push_back(0);
    const auto original = text;
    constexpr std::array<std::int32_t, 16> scalars{0, 0x61, 0x62, 0xffff, 0xd800, 0xdbff, 0xdc00, 0xdfff, 0x10000, 0x103ff, 0x10fc00, 0x10ffff, -1, 0x110000, std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::min()};
    for (std::int32_t length = -1; length <= static_cast<std::int32_t>(payload); ++length) {
        const auto units = length < 0 ? TerminatedLength(text) : static_cast<std::size_t>(length);
        auto expectedCount = static_cast<std::int32_t>(units);
        for (std::size_t i = 1; i < units; ++i) if (Pair(text[i - 1], text[i])) --expectedCount;
        Require(u_countChar32_nid_postfix(text.data(), length) == expectedCount);
        for (std::int32_t threshold = -1; threshold <= static_cast<std::int32_t>(payload) + 1; ++threshold)
            Require(u_strHasMoreChar32Than_nid_postfix(text.data(), length, threshold) == (expectedCount > threshold));
        for (const auto scalar : scalars) {
            if (length >= 0) Require(u_memchr32_nid_postfix(text.data(), scalar, length) == ExpectedFind(text, scalar, units, false));
            else {
                Require(u_strchr32_nid_postfix(text.data(), scalar) == ExpectedFind(text, scalar, units + 1, false));
                Require(u_strrchr32_nid_postfix(text.data(), scalar) == ExpectedFind(text, scalar, units + 1, true));
            }
        }
    }
    Require(text == original);
}

void Enumerate(std::vector<std::uint16_t>& text, unsigned remaining) {
    Check(text);
    if (remaining == 0) return;
    for (const auto unit : std::array<std::uint16_t, 6>{0, 0x61, 0xd800, 0xdbff, 0xdc00, 0xdfff}) {
        text.push_back(unit);
        Enumerate(text, remaining - 1);
        text.pop_back();
    }
}

void Domains() {
    for (const auto length : {-2, -1, 0, 1, std::numeric_limits<std::int32_t>::max()}) {
        Require(u_countChar32_nid_postfix(nullptr, length) == 0);
        Require(u_strHasMoreChar32Than_nid_postfix(nullptr, length, 0) == 0);
        Require(u_strHasMoreChar32Than_nid_postfix(nullptr, length, -1) == 1);
    }
    const std::uint16_t empty[] = {0};
    Require(u_countChar32_nid_postfix(empty, -2) == 0);
    Require(u_strHasMoreChar32Than_nid_postfix(empty, -2, 0) == 0);
    Require(u_strHasMoreChar32Than_nid_postfix(empty, 0, -1) == 1);
    Require(u_strHasMoreChar32Than_nid_postfix(empty, std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::max()) == 0);
    Require(u_strHasMoreChar32Than_nid_postfix(empty, std::numeric_limits<std::int32_t>::max(), 0) == 1);
    for (const auto scalar : {-1, 0x110000, std::numeric_limits<std::int32_t>::max()}) {
        Require(u_strchr32_nid_postfix(nullptr, scalar) == nullptr);
        Require(u_strrchr32_nid_postfix(nullptr, scalar) == nullptr);
        Require(u_memchr32_nid_postfix(nullptr, scalar, 5) == nullptr);
    }
    for (const auto count : {-2, -1, 0}) Require(u_memchr32_nid_postfix(nullptr, 0x61, count) == nullptr);
    for (const auto scalar : {0xd800, 0xdbff, 0xdc00, 0xdfff}) {
        Require(u_strchr32_nid_postfix(nullptr, scalar) == nullptr);
        Require(u_strrchr32_nid_postfix(nullptr, scalar) == nullptr);
        for (const auto count : {1, 2, std::numeric_limits<std::int32_t>::max()}) Require(u_memchr32_nid_postfix(nullptr, scalar, count) == nullptr);
    }
    for (const auto scalar : {0x10000, 0x10ffff}) Require(u_memchr32_nid_postfix(nullptr, scalar, 1) == nullptr);
    for (unsigned operation = 0; operation < 3; ++operation) {
        bool rejected = false;
        try {
            if (operation == 0) u_strchr32_nid_postfix(nullptr, 0x61);
            if (operation == 1) u_strrchr32_nid_postfix(nullptr, 0x61);
            if (operation == 2) u_memchr32_nid_postfix(nullptr, 0x61, 1);
        } catch (const std::runtime_error&) { rejected = true; }
        Require(rejected);
    }
}

} // namespace

int main() {
    std::vector<std::uint16_t> text;
    Enumerate(text, 5);
    Check({0xffff, 0xdbff, 0xdfff, 0xd800, 0xdc00, 0x61, 0xd800, 0xdc00});
    Domains();
}

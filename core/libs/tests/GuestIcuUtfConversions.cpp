#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>


namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU UTF conversion: %s\n", message);
    std::abort();
}

std::string Utf8(std::uint32_t value) {
    if (value <= 0x7f) return {static_cast<char>(value)};
    if (value <= 0x7ff) return {static_cast<char>(0xc0 + value / 64), static_cast<char>(0x80 + value % 64)};
    if (value <= 0xffff) return {static_cast<char>(0xe0 + value / 4096), static_cast<char>(0x80 + value / 64 % 64), static_cast<char>(0x80 + value % 64)};
    return {static_cast<char>(0xf0 + value / 262144), static_cast<char>(0x80 + value / 4096 % 64), static_cast<char>(0x80 + value / 64 % 64), static_cast<char>(0x80 + value % 64)};
}

std::u16string Utf16(std::uint32_t value) {
    if (value <= 0xffff) return {static_cast<char16_t>(value)};
    value -= 0x10000;
    return {static_cast<char16_t>(0xd800 + value / 1024), static_cast<char16_t>(0xdc00 + value % 1024)};
}

void Scalars() {
    for (std::uint32_t value = 0; value <= 0x10ffff; ++value) {
        if (value >= 0xd800 && value <= 0xdfff) continue;
        const auto bytes = Utf8(value);
        const auto units = Utf16(value);
        std::array<char16_t, 4> wide{0x5555, 0x5555, 0x5555, 0x5555};
        std::array<char, 6> narrow{'X', 'X', 'X', 'X', 'X', 'X'};
        std::int32_t length = -1, error = 0;
        Require(u_strFromUTF8_nid_postfix(wide.data(), 3, &length, bytes.data(), bytes.size(), &error) == wide.data(), "scalar UTF-8 return");
        Require(error == 0 && length == static_cast<std::int32_t>(units.size()) && std::memcmp(wide.data(), units.data(), units.size() * 2) == 0 && wide[units.size()] == 0 && wide[3] == 0x5555, "scalar UTF-8 decode");
        Require(u_strToUTF8_nid_postfix(narrow.data(), 5, &length, units.data(), units.size(), &error) == narrow.data(), "scalar UTF-16 return");
        Require(error == 0 && length == static_cast<std::int32_t>(bytes.size()) && std::memcmp(narrow.data(), bytes.data(), bytes.size()) == 0 && narrow[bytes.size()] == 0 && narrow[5] == 'X', "scalar UTF-16 encode");
        wide.fill(0x5555);
        Require(u_strFromUTF8Lenient_nid_postfix(wide.data(), 4, &length, bytes.data(), bytes.size(), &error) == wide.data(), "scalar lenient return");
        Require(error == 0 && length == static_cast<std::int32_t>(units.size()) && std::memcmp(wide.data(), units.data(), units.size() * 2) == 0 && wide[units.size()] == 0, "scalar lenient decode");
    }
}

void Capacity() {
    const std::string bytes = std::string("A\0", 2) + Utf8(0x1f642) + Utf8(0x20ac) + "Z";
    const std::u16string units = std::u16string(u"A\0", 2) + Utf16(0x1f642) + u"\u20acZ";
    for (std::int32_t capacity = 0; capacity <= 12; ++capacity) {
        std::array<char16_t, 16> wide;
        wide.fill(0x5555);
        std::int32_t length = -1, error = 0;
        Require(u_strFromUTF8_nid_postfix(wide.data(), capacity, &length, bytes.data(), bytes.size(), &error) == wide.data(), "capacity UTF-8 return");
        Require(length == static_cast<std::int32_t>(units.size()) && error == (capacity < length ? 15 : capacity == length ? -124 : 0), "capacity UTF-8 status");
        for (std::int32_t index = 0; index < 16; ++index) Require(wide[index] == (index < capacity && index < length ? units[index] : index == length && index < capacity ? 0 : 0x5555), "capacity UTF-8 prefix or canary");
        std::array<char, 16> narrow;
        narrow.fill('X');
        error = 0;
        Require(u_strToUTF8_nid_postfix(narrow.data(), capacity, &length, units.data(), units.size(), &error) == narrow.data(), "capacity UTF-16 return");
        Require(length == static_cast<std::int32_t>(bytes.size()) && error == (capacity < length ? 15 : capacity == length ? -124 : 0), "capacity UTF-16 status");
        const auto prefix = capacity < 2 ? capacity : capacity < 6 ? 2 : capacity < 9 ? 6 : capacity < 10 ? 9 : 10;
        for (std::int32_t index = 0; index < 16; ++index) Require(narrow[index] == (index < prefix ? bytes[index] : index == length && index < capacity ? 0 : 'X'), "UTF-8 complete-sequence prefix or canary");
        wide.fill(0x5555);
        error = 0;
        auto* result = u_strFromUTF8Lenient_nid_postfix(wide.data(), capacity, &length, bytes.data(), bytes.size(), &error);
        if (capacity < static_cast<std::int32_t>(bytes.size())) Require(!result && error == 15 && length == static_cast<std::int32_t>(bytes.size()) && wide[0] == 0x5555, "lenient capacity fast rejection");
        else Require(result == wide.data() && error == 0 && length == static_cast<std::int32_t>(units.size()) && std::memcmp(wide.data(), units.data(), units.size() * 2) == 0, "lenient capacity conversion");
    }
    std::int32_t error = 0, length = -1;
    Require(!u_strFromUTF8_nid_postfix(nullptr, 0, &length, bytes.data(), bytes.size(), &error) && error == 15 && length == static_cast<std::int32_t>(units.size()), "UTF-8 null preflight");
    error = 0;
    Require(!u_strToUTF8_nid_postfix(nullptr, 0, &length, units.data(), units.size(), &error) && error == 15 && length == static_cast<std::int32_t>(bytes.size()), "UTF-16 null preflight");
    std::array<char16_t, 8> wide{};
    error = 0;
    u_strFromUTF8_nid_postfix(wide.data(), 8, &length, bytes.c_str(), -1, &error);
    Require(error == 0 && length == 1 && wide[0] == u'A' && wide[1] == 0, "UTF-8 NUL length");
    std::array<char, 8> narrow{};
    u_strToUTF8_nid_postfix(narrow.data(), 8, &length, units.c_str(), -1, &error);
    Require(error == 0 && length == 1 && narrow[0] == 'A' && narrow[1] == 0, "UTF-16 NUL length");
}

void Malformed() {
    const std::vector<std::pair<std::string, std::u16string>> cases{
        {std::string("\x80", 1), u"\ufffd"}, {std::string("\xc0\x80", 2), u"\ufffd\ufffd"},
        {std::string("\xe0\x80\x80", 3), u"\ufffd\ufffd\ufffd"}, {std::string("\xed\xa0\x80", 3), u"\ufffd\ufffd\ufffd"},
        {std::string("\xf4\x90\x80\x80", 4), u"\ufffd\ufffd\ufffd\ufffd"},
        {std::string("\xe2\x82", 2), u"\ufffd"}, {std::string("\xf0\x90\x80", 3), u"\ufffd"},
        {std::string("\xe2\x82" "A", 3), u"\ufffdA"}
    };
    for (const auto& [input, expected] : cases) {
        for (const std::int32_t sourceLength : {-1, static_cast<std::int32_t>(input.size())}) {
            std::array<char16_t, 20> output;
            output.fill(0x5555);
            std::int32_t length = 77, error = 0, substitutions = 99;
            Require(!u_strFromUTF8_nid_postfix(output.data(), 20, &length, input.c_str(), sourceLength, &error) && error == 10 && length == 77 && output[0] == 0x5555, "strict malformed UTF-8");
            error = 0;
            Require(u_strFromUTF8WithSub_nid_postfix(output.data(), 20, &length, input.c_str(), sourceLength, 0xfffd, &substitutions, &error) == output.data(), "substitution return");
            Require(error == 0 && length == static_cast<std::int32_t>(expected.size()) && std::memcmp(output.data(), expected.data(), expected.size() * 2) == 0, "maximal-subpart replacement");
            std::int32_t expectedCount = 0;
            for (const auto value : expected) expectedCount += value == 0xfffd;
            Require(substitutions == expectedCount && output[length] == 0, "substitution count and terminator");
            error = 0;
            u_strFromUTF8WithSub_nid_postfix(output.data(), 20, &length, input.c_str(), sourceLength, 0x1f642, &substitutions, &error);
            Require(error == 0 && length == static_cast<std::int32_t>(expected.size()) + expectedCount && substitutions == expectedCount, "supplementary substitution geometry");
        }
    }
    const std::u16string input{u'A', 0xd800, u'B', 0xdc00, 0xd800, 0xdc00};
    const auto expected = std::string("A?") + "B?" + Utf8(0x10000);
    std::array<char, 32> output{};
    std::int32_t length = 77, substitutions = 99, error = 0;
    Require(!u_strToUTF8_nid_postfix(output.data(), 32, &length, input.data(), input.size(), &error) && error == 10 && length == 77 && output[0] == 'A' && output[1] == 0, "strict isolated surrogate prefix");
    error = 0;
    Require(u_strToUTF8WithSub_nid_postfix(output.data(), 32, &length, input.data(), input.size(), '?', &substitutions, &error) == output.data(), "surrogate substitution return");
    Require(error == 0 && substitutions == 2 && length == static_cast<std::int32_t>(expected.size()) && std::memcmp(output.data(), expected.data(), expected.size()) == 0, "surrogate substitutions and valid pair");
    const std::string overlong("\xc0\x80", 2);
    std::array<char16_t, 4> wide{0x5555, 0x5555, 0x5555, 0x5555};
    error = 0;
    u_strFromUTF8Lenient_nid_postfix(wide.data(), 4, &length, overlong.data(), overlong.size(), &error);
    Require(error == 0 && length == 1 && wide[0] == 0 && wide[1] == 0, "lenient overlong form");
    error = 0;
    u_strFromUTF8Lenient_nid_postfix(wide.data(), 4, &length, "\xf0\x90", -1, &error);
    Require(error == 0 && length == 1 && wide[0] == 0xfffd && wide[1] == 0, "lenient truncated sequence");
}

void Status() {
    std::array<char16_t, 8> output;
    output.fill(0x5555);
    std::int32_t length = 77, substitutions = 99, error = 10;
    Require(!u_strFromUTF8WithSub_nid_postfix(output.data(), 8, &length, "a", 1, '?', &substitutions, &error) && length == 77 && substitutions == 99 && output[0] == 0x5555 && error == 10, "latched failure output preservation");
    for (const std::int32_t warning : {-124, -128}) {
        error = warning;
        Require(u_strFromUTF8_nid_postfix(output.data(), 8, &length, "a", -1, &error) == output.data() && error == (warning == -124 ? 0 : warning) && length == 1, "warning propagation");
    }
    for (const std::int32_t substitution : {0xd800, 0xdfff, 0x110000}) {
        error = 0; length = 77; substitutions = 99;
        Require(!u_strFromUTF8WithSub_nid_postfix(output.data(), 8, &length, "a", 1, substitution, &substitutions, &error) && error == 1 && length == 77 && substitutions == 99, "invalid substitution argument");
    }
    error = 0;
    Require(!u_strFromUTF8_nid_postfix(nullptr, 1, &length, "a", 1, &error) && error == 1, "null positive destination");
    error = 0;
    Require(!u_strFromUTF8_nid_postfix(output.data(), -1, &length, "a", 1, &error) && error == 1, "negative capacity");
    error = 0;
    Require(!u_strFromUTF8_nid_postfix(output.data(), 8, &length, nullptr, 1, &error) && error == 1, "null positive source");
    error = 0;
    Require(u_strFromUTF8_nid_postfix(output.data(), 8, nullptr, nullptr, 0, &error) == output.data() && error == 0 && output[0] == 0, "null empty source");
}

} // namespace

int main() {
    Scalars();
    Capacity();
    Malformed();
    Status();
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

extern "C" {
char16_t* APS5_VABI u_strcpy_nid_postfix(char16_t*, const char16_t*);
char16_t* APS5_VABI u_strncpy_nid_postfix(char16_t*, const char16_t*, std::int32_t);
char16_t* APS5_VABI u_strcat_nid_postfix(char16_t*, const char16_t*);
char16_t* APS5_VABI u_strncat_nid_postfix(char16_t*, const char16_t*, std::int32_t);
char16_t* APS5_VABI u_memcpy_nid_postfix(char16_t*, const char16_t*, std::int32_t);
}

namespace {

using Buffer = std::array<char16_t, 32>;

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "ICU copy: %s\n", message);
    std::abort();
}

Buffer Pattern() {
    Buffer result{};
    for (std::size_t index = 0; index < result.size(); ++index)
        result[index] = static_cast<char16_t>(0xa100 + index);
    return result;
}

std::vector<std::vector<char16_t>> Sources() {
    std::vector<std::vector<char16_t>> result;
    constexpr std::array<char16_t, 5> alphabet{0, u'A', 0xd800, 0xdc00, 0xffff};
    std::size_t combinations = 1;
    for (std::size_t length = 0; length <= 4; ++length) {
        for (std::size_t code = 0; code < combinations; ++code) {
            std::vector<char16_t> text(length);
            auto digits = code;
            for (auto& unit : text) {
                unit = alphabet[digits % alphabet.size()];
                digits /= alphabet.size();
            }
            text.push_back(0);
            result.push_back(std::move(text));
        }
        combinations *= alphabet.size();
    }
    return result;
}

void Copies(const std::vector<char16_t>& source) {
    const auto preserved = source;
    const auto terminator = std::find(source.begin(), source.end(), 0);
    const auto length = static_cast<std::size_t>(terminator - source.begin());
    auto destination = Pattern();
    auto expected = destination;
    std::copy_n(source.begin(), length + 1, expected.begin() + 3);
    Require(u_strcpy_nid_postfix(destination.data() + 3, source.data()) == destination.data() + 3, "strcpy return pointer");
    Require(destination == expected, "strcpy content or guard");
    for (const std::int32_t count : {-3, 0, 1, 2, 3, 4, 5, 7, std::numeric_limits<std::int32_t>::max()}) {
        destination = Pattern();
        expected = destination;
        const auto written = count <= 0 ? 0 : std::min<std::size_t>(static_cast<std::size_t>(count), length + 1);
        std::copy_n(source.begin(), written, expected.begin() + 3);
        Require(u_strncpy_nid_postfix(destination.data() + 3, source.data(), count) == destination.data() + 3, "strncpy return pointer");
        Require(destination == expected, "strncpy termination, nonpadding or guard");
    }
    for (std::size_t count = 0; count <= source.size(); ++count) {
        destination = Pattern();
        expected = destination;
        std::copy_n(source.begin(), count, expected.begin() + 3);
        Require(u_memcpy_nid_postfix(destination.data() + 3, source.data(), static_cast<std::int32_t>(count)) == destination.data() + 3, "memcpy return pointer");
        Require(destination == expected, "memcpy embedded zero, raw units or guard");
    }
    Require(source == preserved, "copy changed source");
}

void Concatenations(const std::vector<char16_t>& source) {
    const auto preserved = source;
    const auto terminator = std::find(source.begin(), source.end(), 0);
    const auto length = static_cast<std::size_t>(terminator - source.begin());
    for (const std::vector<char16_t> prefix : {std::vector<char16_t>{}, std::vector<char16_t>{u'P'}, std::vector<char16_t>{0xd83d, 0xde00, u'X'}}) {
        auto initial = Pattern();
        std::copy(prefix.begin(), prefix.end(), initial.begin() + 3);
        initial[3 + prefix.size()] = 0;
        auto destination = initial;
        auto expected = initial;
        std::copy_n(source.begin(), length + 1, expected.begin() + 3 + prefix.size());
        Require(u_strcat_nid_postfix(destination.data() + 3, source.data()) == destination.data() + 3, "strcat return pointer");
        Require(destination == expected, "strcat content, termination or guard");
        for (const std::int32_t count : {-3, 0, 1, 2, 3, 4, 5, 7, std::numeric_limits<std::int32_t>::max()}) {
            destination = initial;
            expected = initial;
            if (count > 0) {
                const auto appended = std::min<std::size_t>(static_cast<std::size_t>(count), length);
                std::copy_n(source.begin(), appended, expected.begin() + 3 + prefix.size());
                expected[3 + prefix.size() + appended] = 0;
            }
            Require(u_strncat_nid_postfix(destination.data() + 3, source.data(), count) == destination.data() + 3, "strncat return pointer");
            Require(destination == expected, "strncat count, termination or guard");
        }
    }
    Require(source == preserved, "concatenation changed source");
}

void NonpositiveCounts() {
    const auto* invalid = reinterpret_cast<const char16_t*>(static_cast<std::uintptr_t>(1));
    for (const auto count : {std::numeric_limits<std::int32_t>::min(), -1, 0}) {
        auto destination = Pattern();
        const auto initial = destination;
        Require(u_strncpy_nid_postfix(destination.data(), invalid, count) == destination.data(), "strncpy no-read return");
        Require(u_strncat_nid_postfix(destination.data(), invalid, count) == destination.data(), "strncat no-read return");
        Require(u_memcpy_nid_postfix(destination.data(), invalid, count) == destination.data(), "memcpy no-read return");
        Require(destination == initial, "nonpositive count wrote destination");
        Require(u_strncpy_nid_postfix(nullptr, nullptr, count) == nullptr, "strncpy null no-op");
        Require(u_strncat_nid_postfix(nullptr, nullptr, count) == nullptr, "strncat null no-op");
        Require(u_memcpy_nid_postfix(nullptr, nullptr, count) == nullptr, "memcpy null no-op");
    }
}

void SharedStorage() {
    auto initial = Pattern();
    initial[3] = u'P';
    initial[4] = 0;
    initial[24] = 0xd83d;
    initial[25] = 0xde00;
    initial[26] = 0;
    initial[27] = u'R';
    for (int operation = 0; operation < 5; ++operation) {
        auto destination = initial;
        auto expected = initial;
        char16_t* returned = nullptr;
        if (operation == 0) {
            std::copy_n(initial.begin() + 24, 3, expected.begin() + 3);
            returned = u_strcpy_nid_postfix(destination.data() + 3, destination.data() + 24);
        } else if (operation == 1) {
            std::copy_n(initial.begin() + 24, 3, expected.begin() + 3);
            returned = u_strncpy_nid_postfix(destination.data() + 3, destination.data() + 24, 6);
        } else if (operation == 2) {
            std::copy_n(initial.begin() + 24, 3, expected.begin() + 4);
            returned = u_strcat_nid_postfix(destination.data() + 3, destination.data() + 24);
        } else if (operation == 3) {
            expected[4] = initial[24];
            expected[5] = 0;
            returned = u_strncat_nid_postfix(destination.data() + 3, destination.data() + 24, 1);
        } else {
            std::copy_n(initial.begin() + 24, 4, expected.begin() + 3);
            returned = u_memcpy_nid_postfix(destination.data() + 3, destination.data() + 24, 4);
        }
        Require(returned == destination.data() + 3 && destination == expected, "disjoint ranges in shared storage");
    }
}

} // namespace

int main() {
    NonpositiveCounts();
    SharedStorage();
    for (const auto& source : Sources()) {
        Copies(source);
        Concatenations(source);
    }
}

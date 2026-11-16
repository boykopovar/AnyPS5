#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU termination check failed at line %u\n", location.line());
    std::exit(EXIT_FAILURE);
}

template<typename TException, typename TAction>
void Throws(TAction action) {
    try { action(); } catch (const TException&) { return; }
    Require(false);
}

template<typename TChar, typename TFunction>
void Termination(TFunction function) {
    for (const auto initial : {-128, -124, -1, 0, 1, 15}) {
        for (const auto capacity : {-2147483647, -1, 0, 1, 2, 3}) {
            for (const auto length : {-2147483647, -1, 0, 1, 2, 3, 2147483647}) {
                std::array<TChar, 5> buffer;
                buffer.fill(0x5a5a);
                auto status = initial;
                Require(function(buffer.data(), capacity, length, &status) == length);
                const bool write = initial <= 0 && length >= 0 && length < capacity;
                const int expectedStatus = initial > 0 || length < 0 ? initial : length < capacity ? (initial == -124 ? 0 : initial) : length == capacity ? -124 : 15;
                Require(status == expectedStatus);
                for (int i = 0; i < 5; ++i) Require(buffer[i] == (write && i == length ? 0 : 0x5a5a));
            }
        }
    }
    std::int32_t status = 0;
    Require(function(nullptr, 0, 1, &status) == 1 && status == 15);
    Require(function(nullptr, 1, 0, nullptr) == 0);
    status = 1;
    Require(function(nullptr, 1, 0, &status) == 0 && status == 1);
    status = 0;
    Throws<std::invalid_argument>([&] { function(nullptr, 1, 0, &status); });
}

}

int main() {
    Termination<char16_t>(u_terminateUChars_nid_postfix);
    Termination<std::int32_t>(u_terminateWChars_nid_postfix);
    for (unsigned i = 0; i <= 65535; ++i)
        Require(u_asciiToUpper_nid_postfix(static_cast<char16_t>(i)) == (i >= 'a' && i <= 'z' ? i - 32 : i));
    std::array<char16_t, 130> buffer;
    std::string ascii;
    for (int i = 1; i <= 127; ++i) ascii.push_back(static_cast<char>(i));
    buffer.fill(0x5a5a);
    Require(u_uastrcpy_nid_postfix(buffer.data(), ascii.c_str()) == buffer.data());
    for (int i = 0; i < 127; ++i) Require(buffer[i] == i + 1);
    Require(buffer[127] == 0 && buffer[128] == 0x5a5a);
    for (const auto count : {0, 1, 2, 3, 4, 5}) {
        buffer.fill(0x5a5a);
        Require(u_uastrncpy_nid_postfix(buffer.data(), "abc", count) == buffer.data());
        for (int i = 0; i < 7; ++i) {
            const int expected = i < std::min(count, 3) ? 'a' + i : i == 3 && count > 3 ? 0 : 0x5a5a;
            Require(buffer[i] == expected);
        }
    }
    buffer.fill(0x5a5a);
    Require(u_uastrncpy_nid_postfix(buffer.data(), "abc", -1) == buffer.data());
    Require(buffer[0] == 0 && buffer[1] == 0x5a5a);
    Throws<std::invalid_argument>([&] { u_uastrncpy_nid_postfix(buffer.data(), "", std::numeric_limits<std::int32_t>::min()); });
    Throws<std::invalid_argument>([] { u_uastrcpy_nid_postfix(nullptr, ""); });
    Throws<std::invalid_argument>([&] { u_uastrcpy_nid_postfix(buffer.data(), nullptr); });
    Throws<std::invalid_argument>([] { u_uastrncpy_nid_postfix(nullptr, "", 0); });
    for (int i = 128; i <= 255; ++i) {
        char source[]{static_cast<char>(i), 0};
        buffer.fill(0x5a5a);
        Require(u_uastrncpy_nid_postfix(buffer.data(), source, 0) == buffer.data() && buffer[0] == 0x5a5a);
    }
    std::puts("ICU termination tests passed");
}

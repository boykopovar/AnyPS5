#include "prx/libwebrtc/libwebrtc.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc FastIntToBuffer check failed at line %u\n", location.line());
    std::abort();
}

constexpr char Fill = 'x';

std::array<char, 40> Filled() {
    std::array<char, 40> buffer;
    buffer.fill(Fill);
    return buffer;
}

void Check(int value, std::string_view text, std::size_t zeroedTo) {
    std::array<char, 40> buffer = Filled();
    char* end = _ZN4absl16numbers_internal15FastIntToBufferEiPc_nid_postfix(value, buffer.data());
    Require(end == buffer.data() + text.size());
    Require(std::string_view(buffer.data(), text.size()) == text);
    for (std::size_t index = text.size(); index < zeroedTo; ++index)
        Require(buffer[index] == '\0');
    for (std::size_t index = zeroedTo; index < buffer.size(); ++index)
        Require(buffer[index] == Fill);
}

void TestDigits() {
    Check(0, "0", 2);
    Check(7, "7", 2);
    Check(-7, "-7", 3);
    Check(10, "10", 8);
    Check(-10, "-10", 9);
    Check(123, "123", 8);
    Check(-4567, "-4567", 9);
    Check(1234567, "1234567", 8);
    Check(99999999, "99999999", 9);
    Check(-99999999, "-99999999", 10);
    Check(100000000, "100000000", 10);
    Check(1234567890, "1234567890", 11);
    Check(std::numeric_limits<int>::max(), "2147483647", 11);
    Check(std::numeric_limits<int>::min(), "-2147483648", 12);
}

void TestNullBuffer() {
    try {
        _ZN4absl16numbers_internal15FastIntToBufferEiPc_nid_postfix(1, nullptr);
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

}

int main() {
    TestDigits();
    TestNullBuffer();
    std::puts("libwebrtc FastIntToBuffer tests passed");
}

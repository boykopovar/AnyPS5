#include "prx/libwebrtc/libwebrtc.hpp"
#include "Testing.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string_view>

namespace {

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

void TestUnwritableGuestBuffer() {
    errno = 71;
    try {
        _ZN4absl16numbers_internal15FastIntToBufferEiPc_nid_postfix(123, reinterpret_cast<char*>(0x10));
    } catch (const std::invalid_argument&) {
        Require(errno == 71);
        return;
    }
    Require(false);
}

void TestPreservedErrno() {
    for (const int number : {0, -7, 12, -12345, 1234567890}) {
        auto buffer = Filled();
        errno = 72;
        _ZN4absl16numbers_internal15FastIntToBufferEiPc_nid_postfix(number, buffer.data());
        Require(errno == 72);
    }
}

}

int main() {
    TestDigits();
    TestNullBuffer();
    TestUnwritableGuestBuffer();
    TestPreservedErrno();
    std::puts("libwebrtc FastIntToBuffer tests passed");
}

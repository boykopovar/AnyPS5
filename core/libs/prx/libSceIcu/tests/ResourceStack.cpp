#include "prx/libSceIcu/libSceIcu.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

namespace {

static_assert(std::is_same_v<decltype(&ures_initStackObject_nid_postfix), void (APS5_VABI*)(UResourceBundle*)>);

struct GuardedBundle {
    std::array<std::uint8_t, 32> before;
    std::array<std::uint8_t, 200> bundle;
    std::array<std::uint8_t, 32> after;
};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "ICU resource stack: %s\n", message);
    std::abort();
}

void RequireGuards(const GuardedBundle& bundle, std::uint8_t value) {
    for (const auto byte : bundle.before) Require(byte == value, "ures_initStackObject preserves the leading guard");
    for (const auto byte : bundle.after) Require(byte == value, "ures_initStackObject preserves the trailing guard");
}

}

int main() {
    GuardedBundle actual;
    for (const std::uint8_t value : {std::uint8_t{0}, std::uint8_t{0x5a}, std::uint8_t{0xff}}) {
        actual.before.fill(value);
        actual.bundle.fill(value);
        actual.after.fill(value);
        errno = 73;
        ures_initStackObject_nid_postfix(reinterpret_cast<UResourceBundle*>(actual.bundle.data()));
        Require(errno == 73, "ures_initStackObject preserves errno");
        RequireGuards(actual, value);
        for (const auto byte : actual.bundle) Require(byte == 0, "ures_initStackObject clears the complete ICU 68 bundle");
    }
}

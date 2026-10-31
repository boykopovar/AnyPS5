#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 BIGNUM check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

LibreSsl3Bignum Filled() {
    LibreSsl3Bignum number;
    std::memset(&number, 0xA5, sizeof(number));
    return number;
}

void TestSign() {
    for (std::int32_t value : {0, 1, 2, -1, std::numeric_limits<std::int32_t>::max(), std::numeric_limits<std::int32_t>::min()}) {
        LibreSsl3Bignum number = Filled();
        number.neg = value;
        LibreSsl3Bignum before;
        std::memcpy(&before, &number, sizeof(number));
        Require(BN_is_negative_nid_postfix(&number) == (value != 0 ? 1 : 0));
        Require(std::memcmp(&before, &number, sizeof(number)) == 0);
    }
}

void TestOtherFields() {
    std::uint64_t words[] = {5};
    LibreSsl3Bignum number {};
    number.d = words;
    number.top = 1;
    number.dmax = 1;
    number.flags = -1;
    Require(BN_is_negative_nid_postfix(&number) == 0);
    number.neg = 1;
    Require(BN_is_negative_nid_postfix(&number) == 1);
}

void TestNull() {
    RequireInvalidArgument([] { BN_is_negative_nid_postfix(nullptr); });
}

}

int main() {
    TestSign();
    TestOtherFields();
    TestNull();
    std::puts("LibreSSL3 BIGNUM tests passed");
}

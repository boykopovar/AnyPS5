#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JSBigInt equality check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

JSC::JSBigInt Make(bool sign, std::uint64_t* digits, std::uint32_t length) {
    JSC::JSBigInt value{};
    value.length = length;
    value.hash = 0x12345678;
    value.sign = sign;
    value.data = digits;
    return value;
}

bool Equals(JSC::JSBigInt& x, JSC::JSBigInt& y) {
    return _ZN3JSC8JSBigInt6equalsEPS0_S1_(&x, &y);
}

bool EqualsInt32(JSC::JSBigInt& x, std::int32_t value) {
    return _ZN3JSC8JSBigInt13equalsToInt32Ei(&x, value);
}

}

int main() {
    std::uint64_t a[] = {1, 2, 3};
    std::uint64_t b[] = {1, 2, 3};
    std::uint64_t c[] = {1, 2, 4};

    auto zero = Make(false, nullptr, 0);
    auto otherZero = Make(false, nullptr, 0);
    auto x = Make(false, a, 3);
    auto y = Make(false, b, 3);
    auto negativeY = Make(true, b, 3);
    auto z = Make(false, c, 3);
    auto shortY = Make(false, b, 2);

    Require(Equals(zero, otherZero));
    Require(Equals(x, x));
    Require(Equals(x, y));
    Require(!Equals(x, negativeY));
    Require(!Equals(x, z));
    Require(!Equals(x, shortY));
    Require(!Equals(zero, x));

    std::uint64_t one[] = {1};
    std::uint64_t big[] = {2147483648ull};
    std::uint64_t max[] = {2147483647ull};
    auto positiveOne = Make(false, one, 1);
    auto negativeOne = Make(true, one, 1);
    auto minInt32 = Make(true, big, 1);
    auto maxInt32 = Make(false, max, 1);
    auto positiveBig = Make(false, big, 1);
    auto twoDigits = Make(false, a, 2);

    Require(EqualsInt32(zero, 0));
    Require(!EqualsInt32(positiveOne, 0));
    Require(!EqualsInt32(zero, 1));
    Require(EqualsInt32(positiveOne, 1));
    Require(!EqualsInt32(positiveOne, -1));
    Require(EqualsInt32(negativeOne, -1));
    Require(!EqualsInt32(negativeOne, 1));
    Require(EqualsInt32(minInt32, std::numeric_limits<std::int32_t>::min()));
    Require(!EqualsInt32(positiveBig, std::numeric_limits<std::int32_t>::min()));
    Require(EqualsInt32(maxInt32, std::numeric_limits<std::int32_t>::max()));
    Require(!EqualsInt32(twoDigits, 1));

    std::puts("JSBigInt equality tests passed");
    return 0;
}

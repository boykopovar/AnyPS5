#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

using NKWebKit::JscAbi::JSPromise;

constexpr std::uint64_t Undefined = 0xa;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "promise state check failed at line %u\n", location.line());
        std::abort();
    }
}

template<typename Fn>
void ThrowsInvalidArgument(Fn&& fn, int testErrno, std::source_location location = std::source_location::current()) {
    errno = testErrno;
    bool caught = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        caught = true;
    }
    Require(caught, location);
    Require(errno == testErrno, location);
}

std::uint64_t Int32Value(std::int32_t value) {
    return 0xfffe000000000000ull | static_cast<std::uint32_t>(value);
}

std::uint64_t DoubleValue(double value) {
    return std::bit_cast<std::uint64_t>(value) + (1ull << 49);
}

JSPromise Promise(std::uint64_t flags, std::uint64_t reactionsOrResult) {
    JSPromise promise{};
    promise.flags = flags;
    promise.reactionsOrResult = reactionsOrResult;
    return promise;
}

struct Expected {
    std::uint64_t flags;
    std::uint32_t status;
    bool handled;
};

void TestEncodings() {
    const std::uint64_t result = 0x00007f1234567890ull;
    const Expected cases[] = {
        {Int32Value(0), 0, false},
        {Int32Value(1), 1, false},
        {Int32Value(2), 2, false},
        {Int32Value(3), 3, false},
        {Int32Value(4), 0, true},
        {Int32Value(5), 1, true},
        {Int32Value(6), 2, true},
        {Int32Value(7), 3, true},
        {Int32Value(8 | 1), 1, false},
        {Int32Value(12 | 2), 2, true},
        {Int32Value(0x7ffffffc), 0, true},
        {DoubleValue(0.0), 0, false},
        {DoubleValue(1.0), 1, false},
        {DoubleValue(2.0), 2, false},
        {DoubleValue(6.0), 2, true},
        {DoubleValue(4294967295.0), 3, true},
        {DoubleValue(2147483648.0), 0, false},
        {DoubleValue(4294967292.0), 0, true},
        {DoubleValue(1.5), 1, false},
    };
    for (const Expected& expected : cases) {
        const JSPromise promise = Promise(expected.flags, result);
        Require(_ZNK3JSC9JSPromise6statusERNS_2VME(&promise, nullptr) == expected.status);
        Require(_ZNK3JSC9JSPromise9isHandledERNS_2VME(&promise, nullptr) == expected.handled);
        Require(_ZNK3JSC9JSPromise6resultERNS_2VME(&promise, nullptr) == (expected.status ? result : Undefined));
    }
}

void TestResult() {
    const JSPromise pending = Promise(Int32Value(0), 0x1234);
    const JSPromise pendingHandled = Promise(Int32Value(4), 0x1234);
    const JSPromise fulfilled = Promise(Int32Value(1), 0x5678);
    const JSPromise rejected = Promise(Int32Value(2 | 4), 0x9abc);
    const JSPromise rejectedWithZeroResult = Promise(Int32Value(2), 0);
    Require(_ZNK3JSC9JSPromise6resultERNS_2VME(&pending, nullptr) == Undefined);
    Require(_ZNK3JSC9JSPromise6resultERNS_2VME(&pendingHandled, nullptr) == Undefined);
    Require(_ZNK3JSC9JSPromise6resultERNS_2VME(&fulfilled, nullptr) == 0x5678);
    Require(_ZNK3JSC9JSPromise6resultERNS_2VME(&rejected, nullptr) == 0x9abc);
    Require(_ZNK3JSC9JSPromise6resultERNS_2VME(&rejectedWithZeroResult, nullptr) == 0);
    int vmStandIn = 0;
    Require(_ZNK3JSC9JSPromise6statusERNS_2VME(&fulfilled, &vmStandIn) == 1);
}

void TestNegativeInt32TakesDoublePath() {
    const JSPromise negative = Promise(Int32Value(-1), 0x77);
    Require(_ZNK3JSC9JSPromise6statusERNS_2VME(&negative, nullptr) == 0);
    Require(_ZNK3JSC9JSPromise9isHandledERNS_2VME(&negative, nullptr) == false);
    Require(_ZNK3JSC9JSPromise6resultERNS_2VME(&negative, nullptr) == Undefined);
}

void TestGuestPointerValidation() {
    auto* unmapped = reinterpret_cast<JSPromise*>(std::uintptr_t{0x1});
    ThrowsInvalidArgument([] { _ZNK3JSC9JSPromise6statusERNS_2VME(nullptr, nullptr); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZNK3JSC9JSPromise6statusERNS_2VME(unmapped, nullptr); }, EINVAL);
    ThrowsInvalidArgument([] { _ZNK3JSC9JSPromise6resultERNS_2VME(nullptr, nullptr); }, ENOMEM);
    ThrowsInvalidArgument([&] { _ZNK3JSC9JSPromise6resultERNS_2VME(unmapped, nullptr); }, EFAULT);
    ThrowsInvalidArgument([] { _ZNK3JSC9JSPromise9isHandledERNS_2VME(nullptr, nullptr); }, EAGAIN);
    ThrowsInvalidArgument([&] { _ZNK3JSC9JSPromise9isHandledERNS_2VME(unmapped, nullptr); }, EINVAL);
}

}

int main() {
    TestEncodings();
    TestResult();
    TestNegativeInt32TakesDoublePath();
    TestGuestPointerValidation();
    return 0;
}

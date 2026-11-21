#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    constexpr std::int64_t negative = std::numeric_limits<std::int64_t>::min();
    constexpr std::int64_t zero = 0;
    constexpr std::int64_t oneNanosecond = 1;
    constexpr std::int64_t twelveMilliseconds = 12000000;
    const auto sleep = _ZNSt3__111this_thread9sleep_forERKNS_6chrono8durationIxNS_5ratioILl1ELl1000000000EEEEE_nid_postfix;
    sleep(&negative);
    sleep(&zero);
    sleep(&oneNanosecond);
    const auto before = std::chrono::steady_clock::now();
    sleep(&twelveMilliseconds);
    const auto elapsed = std::chrono::steady_clock::now() - before;
    Require(elapsed >= std::chrono::milliseconds(12), "sleep_for returned before its positive duration");
    constexpr std::int64_t oneMillisecond = 1000000;
    auto shortest = std::chrono::steady_clock::duration::max();
    for (int attempt = 0; attempt < 5; ++attempt) {
        const auto start = std::chrono::steady_clock::now();
        sleep(&oneMillisecond);
        const auto taken = std::chrono::steady_clock::now() - start;
        Require(taken >= std::chrono::milliseconds(1), "sleep_for returned before one millisecond");
        shortest = std::min(shortest, taken);
    }
    Require(shortest < std::chrono::milliseconds(10), "a one millisecond sleep_for took a coarse timer tick every time");
    try {
        sleep(nullptr);
        Require(false, "null duration was read");
    } catch (const std::invalid_argument&) {
    }
    return 0;
}

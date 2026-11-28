#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <utility>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    constexpr std::array<std::pair<std::int64_t, std::int64_t>, 9> cases{{
        {0, 0}, {1, 0}, {999999, 0}, {1000000, 1}, {1609459200123456, 1609459200},
        {-1, 0}, {-999999, 0}, {-1000000, -1}, {std::numeric_limits<std::int64_t>::min(), -9223372036854}
    }};
    for (const auto& [microseconds, expected] : cases)
        Require(_ZNSt3__16chrono12system_clock9to_time_tERKNS0_10time_pointIS1_NS0_8durationIxNS_5ratioILl1ELl1000000EEEEEEE_nid_postfix(&microseconds) == expected,
            "system_clock::to_time_t differs from libc++ duration_cast");
    try {
        _ZNSt3__16chrono12system_clock9to_time_tERKNS0_10time_pointIS1_NS0_8durationIxNS_5ratioILl1ELl1000000EEEEEEE_nid_postfix(nullptr);
        Require(false, "null time point was read");
    } catch (const std::invalid_argument&) {
    }
    return 0;
}

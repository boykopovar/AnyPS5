#include "prx/libc/include/Api.hpp"
#include <array>
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
    const auto sort = _ZNSt3__16__sortIRNS_6__lessIjjEEPjEEvT0_S5_T__nid_postfix;
    std::array<std::uint32_t, 8> values{std::numeric_limits<std::uint32_t>::max(), 0, 3, 3, 0x80000000u, 1, 2, std::numeric_limits<std::uint32_t>::max()};
    sort(values.data(), values.data() + values.size(), nullptr);
    Require((values == std::array<std::uint32_t, 8>{0, 1, 2, 3, 3, 0x80000000u, std::numeric_limits<std::uint32_t>::max(), std::numeric_limits<std::uint32_t>::max()}),
        "unsigned sort order or duplicate count differs");
    std::array<std::uint32_t, 4> partial{99, 42, 7, 88};
    sort(partial.data() + 1, partial.data() + 3, nullptr);
    Require((partial == std::array<std::uint32_t, 4>{99, 7, 42, 88}), "sort changed values outside its range");
    sort(nullptr, nullptr, nullptr);
    std::uint32_t single = 123;
    sort(&single, &single + 1, nullptr);
    Require(single == 123, "one-element sort changed its value");
    try {
        sort(reinterpret_cast<std::uint32_t*>(16), reinterpret_cast<std::uint32_t*>(8), nullptr);
        Require(false, "reversed range was accepted");
    } catch (const std::invalid_argument&) {
    }
    try {
        sort(reinterpret_cast<std::uint32_t*>(1), reinterpret_cast<std::uint32_t*>(9), nullptr);
        Require(false, "unreadable range was accepted");
    } catch (const std::invalid_argument&) {
    }
    return 0;
}

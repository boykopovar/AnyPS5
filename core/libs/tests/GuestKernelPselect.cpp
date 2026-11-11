#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {
void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "pselect timeout/mask check failed at line %u\n", static_cast<unsigned>(location.line()));
    std::abort();
}

struct Timespec {
    std::int64_t seconds;
    std::int64_t nanoseconds;
};

struct SignalSet {
    std::uint32_t bits[4]{};
};
} // namespace

int main() {
    int descriptors[2]{-1, -1};
    Require(pipe_nid_postfix(descriptors) == 0);
    const int reader = descriptors[0];
    const int writer = descriptors[1];

    SignalSet sigmask{};
    sigmask.bits[0] = 1u << (30 - 1); // SigUsr1

    Timespec zero{0, 0};
    Timespec invalid_nanos{0, 1000000000L};
    Timespec negative_nanos{0, -1};

    std::array<std::uint64_t, 16> read_fds{};
    read_fds[reader / 64] |= std::uint64_t{1} << (reader % 64);

    *__error_nid_postfix() = 0;
    Require(pselect_nid_postfix(reader + 1, read_fds.data(), nullptr, nullptr, &zero, &sigmask) == 0);

    *__error_nid_postfix() = 0;
    Require(pselect_nid_postfix(-1, read_fds.data(), nullptr, nullptr, &zero, &sigmask) == -1);
    Require(*__error_nid_postfix() == 22); // EINVAL

    *__error_nid_postfix() = 0;
    Require(pselect_nid_postfix(reader + 1, read_fds.data(), nullptr, nullptr, &invalid_nanos, &sigmask) == -1);
    Require(*__error_nid_postfix() == 22); // EINVAL

    *__error_nid_postfix() = 0;
    Require(pselect_nid_postfix(reader + 1, read_fds.data(), nullptr, nullptr, &negative_nanos, &sigmask) == -1);
    Require(*__error_nid_postfix() == 22); // EINVAL

    Require(close_nid_postfix(reader) == 0);
    Require(close_nid_postfix(writer) == 0);

    return 0;
}

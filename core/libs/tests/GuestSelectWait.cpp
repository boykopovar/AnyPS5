#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

int main() {
    using Clock = std::chrono::steady_clock;
    for (const std::int64_t micros : {0, 50, 500, 1000, 2000, 16667}) {
        const GuestTimeval limit{0, micros};
        for (const int count : {0, 1024}) {
            const auto start = Clock::now();
            Require(select_nid_postfix(count, nullptr, nullptr, nullptr, &limit) == 0,
                "select without descriptors did not time out");
            Require(Clock::now() - start >= std::chrono::microseconds(micros),
                "select returned before its deadline");
            Require(limit.seconds == 0 && limit.microseconds == micros,
                "select changed the guest timeout");
        }
    }
    const GuestTimeval shortLimit{0, 1000};
    const auto shortWaitStart = Clock::now();
    for (int i = 0; i < 20; ++i) {
        Require(select_nid_postfix(0, nullptr, nullptr, nullptr, &shortLimit) == 0,
            "repeated select without descriptors did not time out");
    }
    const auto shortWait = Clock::now() - shortWaitStart;
    Require(shortWait >= std::chrono::milliseconds(20) && shortWait < std::chrono::milliseconds(200),
        "twenty 1 ms selects did not stay close to their deadlines");
    for (const GuestTimeval limit : {GuestTimeval{-1, 0}, GuestTimeval{0, -1}, GuestTimeval{0, 1000000}}) {
        Require(select_nid_postfix(0, nullptr, nullptr, nullptr, &limit) == -1 && *__error_nid_postfix() == 22,
            "select accepted an invalid timeout");
    }
    const GuestTimeval zero{};
    for (const int count : {-1, 1025}) {
        Require(select_nid_postfix(count, nullptr, nullptr, nullptr, &zero) == -1 && *__error_nid_postfix() == 22,
            "select accepted an invalid descriptor limit");
    }
    std::array<std::uint64_t, 16> read{};
    read[0] = 1 | (std::uint64_t{1} << 63);
    read[1] = 1;
    const GuestTimeval limit{0, 500};
    const auto start = Clock::now();
    Require(select_nid_postfix(1, read.data(), nullptr, nullptr, &limit) == 0,
        "select with no ready device did not time out");
    Require(Clock::now() - start >= std::chrono::microseconds(limit.microseconds),
        "select with no ready device returned before its deadline");
    Require(read[0] == 0,
        "select did not clear the bits up to the end of the last copied word");
    Require(read[1] == 1,
        "select did not preserve the words past the descriptor limit");
    std::array<std::uint64_t, 16> write{};
    write[0] = 2;
    const GuestTimeval longLimit{1, 0};
    Require(select_nid_postfix(2, nullptr, write.data(), nullptr, &longLimit) == 1 && write[0] == 2,
        "select did not report a ready device");
    std::puts("20 select wait scenarios passed");
}

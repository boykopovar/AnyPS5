#include "prx/libkernel/Time/include/TimedWait.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

static void Require(bool condition, const char* message, std::uint64_t frequency, std::uint64_t ticks) {
    if (!condition) {
        std::fprintf(stderr, "%s: frequency %llu ticks %llu\n", message, static_cast<unsigned long long>(frequency), static_cast<unsigned long long>(ticks));
        std::abort();
    }
}

static std::uint64_t Reference(std::uint64_t ticks, std::uint64_t frequency) {
    return (ticks / frequency) * 1000000000ULL + (ticks % frequency) * 1000000000ULL / frequency;
}

static std::uint64_t Next(std::uint64_t& state) {
    state ^= state << 13;
    state ^= state >> 7;
    state ^= state << 17;
    return state;
}

static void Check(const TimedWait::TickConverter& converter, std::uint64_t frequency, std::uint64_t ticks) {
    Require(converter.Nanos(ticks) == Reference(ticks, frequency), "tick conversion differs from the division formula", frequency, ticks);
}

static void CheckFrequency(std::uint64_t frequency, std::uint64_t& state, int iterations) {
    const TimedWait::TickConverter converter(frequency);
    std::vector<std::uint64_t> ticks{0, 1, 2, frequency - 1, frequency, frequency + 1, frequency * 2 - 1, frequency * 2, frequency * 3 + 1,
        1000000000ULL - 1, 1000000000ULL, 1000000000ULL + 1,
        std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::uint64_t>::max() - 1,
        std::uint64_t{1} << 31, std::uint64_t{1} << 32, (std::uint64_t{1} << 32) + 1, std::uint64_t{1} << 62, std::uint64_t{1} << 63, (std::uint64_t{1} << 63) - 1, (std::uint64_t{1} << 63) + 1};
    for (int bits = 1; bits < 64; ++bits) {
        const std::uint64_t base = std::uint64_t{1} << bits;
        ticks.push_back(base - 1);
        ticks.push_back(base);
        ticks.push_back(base + 1);
    }
    for (std::uint64_t whole = 1; whole <= 1000000; whole *= 10) {
        ticks.push_back(whole * frequency);
        ticks.push_back(whole * frequency - 1);
        ticks.push_back(whole * frequency + 1);
    }
    for (const std::uint64_t value : ticks) Check(converter, frequency, value);
    for (int iteration = 0; iteration < iterations; ++iteration) {
        const std::uint64_t value = Next(state);
        Check(converter, frequency, value >> (Next(state) % 64));
        Check(converter, frequency, (Next(state) % 100000) * frequency + Next(state) % frequency);
    }
}

int main() {
    constexpr std::array<std::uint64_t, 36> frequencies{1, 2, 3, 5, 7, 1000, 1024, 65536, 1000000, 2441406, 3000000, 3579545, 9999999, 10000000, 12000000,
        14318180, 19200000, 24000000, 25000000, 26000000, 38400000, 48000000, 100000000, 999999999, 1000000000, 1000000001, 1953125000, 2000000000,
        3000000000ULL, 3417968750ULL, 4294967296ULL, 4294967297ULL, 17179869183ULL, 17179869184ULL, 17179869185ULL, 18446744073709551615ULL};
    std::uint64_t state = 0x9e3779b97f4a7c15ULL;
    for (const std::uint64_t frequency : frequencies) CheckFrequency(frequency, state, 200000);
    for (int iteration = 0; iteration < 3000; ++iteration) {
        const std::uint64_t frequency = (Next(state) >> (Next(state) % 64)) | 1;
        CheckFrequency(frequency, state, 2000);
    }
    return 0;
}

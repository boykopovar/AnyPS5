#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>

static int failures = 0;

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        ++failures;
    }
}

using Sequence = std::array<std::int64_t, 5>;

static Sequence Draw() {
    Sequence values{};
    for (auto& value : values) value = random_nid_postfix();
    return values;
}

static std::uint32_t Word(const char* table, std::size_t index) {
    std::uint32_t value;
    std::memcpy(&value, table + sizeof(value) * index, sizeof(value));
    return value;
}

static bool SeedIs(const std::uint16_t* seed, std::uint16_t low, std::uint16_t middle, std::uint16_t high) {
    return seed[0] == low && seed[1] == middle && seed[2] == high;
}

static void CheckRand48() {
    std::uint16_t seed[3] = {0x330e, 0xabcd, 0x1234};
    Require(erand48_nid_postfix(seed) == 0x657eb725p-32 && SeedIs(seed, 0x5101, 0xb725, 0x657e), "erand48 first value");
    Require(nrand48_nid_postfix(seed) == 1804928587 && SeedIs(seed, 0x6378, 0x0c96, 0xd72a), "nrand48 second value");
    Require(jrand48_nid_postfix(seed) == 1517566982 && SeedIs(seed, 0x2a23, 0x3c06, 0x5a74), "jrand48 third value");
    Require(erand48_nid_postfix(seed) == 0x72534abfp-32 && SeedIs(seed, 0x62f2, 0x4abf, 0x7253), "erand48 fourth value");
    Require(nrand48_nid_postfix(seed) == 684387517 && SeedIs(seed, 0x8d15, 0xd97a, 0x5195), "nrand48 fifth value");
    Require(jrand48_nid_postfix(seed) == 3807181130 && SeedIs(seed, 0xeffc, 0xf94a, 0xe2ec), "jrand48 keeps bit 31 without sign extension");

    std::uint16_t zero[3] = {};
    Require(nrand48_nid_postfix(zero) == 0 && SeedIs(zero, 0x000b, 0, 0), "rand48 zero seed adds the increment");
    std::uint16_t top[3] = {0xffff, 0xffff, 0xffff};
    const double value = erand48_nid_postfix(top);
    Require(value >= 0.0 && value < 1.0, "erand48 stays below 1");

    srand48_nid_postfix(0x1234abcd);
    std::uint16_t expectedSeed[3] = {0x330e, 0xabcd, 0x1234};
    Require(drand48_nid_postfix() == erand48_nid_postfix(expectedSeed), "drand48 matches erand48 on initialized seed");
    Require(lrand48_nid_postfix() == nrand48_nid_postfix(expectedSeed), "lrand48 matches nrand48 on initialized seed");
    Require(mrand48_nid_postfix() == jrand48_nid_postfix(expectedSeed), "mrand48 matches jrand48 on initialized seed");
    Require(drand48_nid_postfix() == erand48_nid_postfix(expectedSeed), "drand48 fourth value matches erand48");

    const std::uint16_t parameters[7] = {1, 0, 0, 3, 0, 0, 5};
    lcong48_nid_postfix(parameters);

    Require(lrand48_nid_postfix() == 0 && mrand48_nid_postfix() == 0, "lcong48 sets seed, multiplier and addend");
    std::uint16_t high[3] = {0, 0, 1};
    Require(jrand48_nid_postfix(high) == 196608 && SeedIs(high, 5, 0, 3), "lcong48 multiplier scales the seed");
    std::uint16_t custom[3] = {2, 0, 0};
    Require(nrand48_nid_postfix(custom) == 0 && SeedIs(custom, 11, 0, 0), "lcong48 parameters apply to nrand48");
    const std::uint16_t newSeed[3] = {0x330e, 0xabcd, 0x1234};
    std::uint16_t* const echoed = seed48_nid_postfix(newSeed);
    Require(SeedIs(echoed, 0x330e, 0xabcd, 0x1234), "seed48 returns a copy of the new seed");
    std::uint16_t again[3] = {0x330e, 0xabcd, 0x1234};
    Require(drand48_nid_postfix() == erand48_nid_postfix(again), "seed48 restores the default multiplier and addend");
    lcong48_nid_postfix(parameters);
    srand48_nid_postfix(0x1234abcd);

    std::uint16_t reset[3] = {0x330e, 0xabcd, 0x1234};
    Require(lrand48_nid_postfix() == nrand48_nid_postfix(reset), "srand48 restores the default multiplier and addend");

    std::uint16_t measured[3] = {0x000b, 0x0016, 0x0021};
    Require(jrand48_nid_postfix(measured) == 3702088075 && SeedIs(measured, 0xe6ba, 0x618b, 0xdca9), "jrand48 matches the console for a result with bit 31 set");
    std::uint16_t topBit[3] = {0, 0, 0x8000};
    Require(jrand48_nid_postfix(topBit) == 2147483648, "jrand48 matches the console for 0x80000000");
    std::uint16_t ones[3] = {0xffff, 0xffff, 0xffff};
    Require(jrand48_nid_postfix(ones) == 4294582547, "jrand48 matches the console for an all-ones seed");
    constexpr std::array<std::int64_t, 6> consoleMrand48 {178800969, 1952030186, 3585512650, 1443049011, 2428758494, 7588830};
    srand48_nid_postfix(1);

    for (const std::int64_t expected : consoleMrand48) Require(mrand48_nid_postfix() == expected, "mrand48 matches the console after srand48(1)");
    constexpr std::array<double, 3> consoleDrand48 {0x4435afd7p-32, 0xae99dc60p-32, 0x43f73108p-32};
    srand48_nid_postfix(7);
    for (const double expected : consoleDrand48) Require(drand48_nid_postfix() == expected, "drand48 matches the titles' libc.prx after srand48(7)");

#ifndef _WIN32
    std::uint16_t guest[3] = {0x1234, 0x5678, 0x9abc};
    unsigned short host[3] = {0x1234, 0x5678, 0x9abc};
    for (int round = 0; round < 64; ++round) {
        Require(erand48_nid_postfix(guest) == std::ldexp(std::floor(::erand48(host) * 0x1p32), -32), "erand48 keeps the host's upper 32 bits");
        Require(nrand48_nid_postfix(guest) == ::nrand48(host), "nrand48 matches the host");
        Require(jrand48_nid_postfix(guest) == static_cast<std::uint32_t>(::jrand48(host)), "jrand48 matches the host's 32 bits");
    }
    ::srand48(1234567);
    srand48_nid_postfix(1234567);

    for (int round = 0; round < 32; ++round) {
        Require(drand48_nid_postfix() == std::ldexp(std::floor(::drand48() * 0x1p32), -32), "drand48 keeps the host's upper 32 bits");
        Require(lrand48_nid_postfix() == ::lrand48(), "lrand48 matches the host");
        Require(mrand48_nid_postfix() == static_cast<std::uint32_t>(::mrand48()), "mrand48 matches the host's 32 bits");
    }
#endif
}

int main() {
    CheckRand48();
    constexpr Sequence seedOne {1804289383, 846930886, 1681692777, 1714636915, 1957747793};
    constexpr Sequence seed12345 {383100999, 858300821, 357768173, 455528251, 133005921};
    constexpr std::array<Sequence, 5> seed42ByType {{
        {413993338, 139015486, 2118548913, 1172713531, 191403351},
        {769798547, 2024571666, 1204852799, 931293870, 1762463907},
        {2051258974, 339992574, 1379825892, 1298392284, 825292997},
        {71876166, 708592740, 1483128881, 907283241, 442951012},
        {472624893, 994493761, 100792968, 176611971, 1804504504},
    }};
    constexpr std::array<std::int64_t, 6> seed7Type1 {1380991591, 1769076016, 21842418, 334161865, 1367472909, 1249858043};

    Require(Draw() == seedOne, "unseeded random must match srandom(1)");
    srandom_nid_postfix(1);

    Require(Draw() == seedOne, "srandom(1) sequence");
    srandom_nid_postfix(0x100000001ull);

    Require(Draw() == seedOne, "srandom must use the low 32 bits of the seed");
    srandom_nid_postfix(12345);

    Require(Draw() == seed12345, "srandom(12345) sequence");

    struct SizeCase {
        std::int64_t size;
        int type;
        std::size_t usedBytes;
    };
    constexpr std::array<SizeCase, 10> sizes {{
        {8, 0, 8}, {31, 0, 8}, {32, 1, 32}, {63, 1, 32}, {64, 2, 64},
        {127, 2, 64}, {128, 3, 128}, {255, 3, 128}, {256, 4, 256}, {1000, 4, 256},
    }};
    std::vector<std::vector<char>> buffers;
    char* defaultTable = nullptr;
    char* active = nullptr;
    for (const auto& sizeCase : sizes) {
        auto& buffer = buffers.emplace_back(static_cast<std::size_t>(sizeCase.size) + 4, static_cast<char>(0xa5));
        char* previous = initstate_nid_postfix(42, buffer.data(), sizeCase.size);
        if (defaultTable == nullptr) defaultTable = previous;
        else Require(previous == active, "initstate must return the previous state");
        Require(Word(buffer.data(), 0) == static_cast<std::uint32_t>(sizeCase.type), "initstate type word");
        Require(Draw() == seed42ByType[static_cast<std::size_t>(sizeCase.type)], "initstate(42) sequence for its size");
        Require(std::all_of(buffer.begin() + static_cast<std::ptrdiff_t>(sizeCase.usedBytes), buffer.end(),
            [](char value) { return value == static_cast<char>(0xa5); }), "initstate must use only the rounded state size");
        active = buffer.data();
    }
    Require(defaultTable != nullptr && Word(defaultTable, 0) == 5 * 5 + 3, "default table must hold its rear position");
    Require(setstate_nid_postfix(defaultTable) == active, "setstate must return the previous state");
    Require(random_nid_postfix() == 116285904 && random_nid_postfix() == 591987137,
        "setstate on the default table must continue the srandom(12345) sequence");

    alignas(4) std::array<char, 32> small{};
    alignas(4) std::array<char, 64> large{};
    initstate_nid_postfix(7, small.data(), static_cast<std::int64_t>(small.size()));

    for (std::size_t i = 0; i < 3; ++i) Require(random_nid_postfix() == seed7Type1[i], "initstate(7) on 32 bytes");
    initstate_nid_postfix(7, large.data(), static_cast<std::int64_t>(large.size()));

    Require(setstate_nid_postfix(small.data()) == large.data(), "setstate must return the 64-byte state");
    Require(setstate_nid_postfix(large.data()) == small.data(), "setstate must return the 32-byte state");
    Require(Word(small.data(), 0) == 5 * 3 + 1, "setstate must save the rear position of the old state");
    Require(setstate_nid_postfix(small.data()) == large.data(), "setstate back to the 32-byte state");
    for (std::size_t i = 3; i < seed7Type1.size(); ++i) Require(random_nid_postfix() == seed7Type1[i], "setstate must resume the sequence");

    initstate_nid_postfix(7, small.data(), static_cast<std::int64_t>(small.size()));
    random_nid_postfix();
    alignas(4) std::array<char, 128> garbled{};
    const std::uint32_t garbledWord = 5 * 7 + 1;
    std::memcpy(garbled.data(), &garbledWord, sizeof(garbledWord));
    Require(setstate_nid_postfix(garbled.data()) == small.data(), "setstate must take a rear position past the degree as the console does");
    random_nid_postfix();
    Require(setstate_nid_postfix(small.data()) == garbled.data(), "setstate back from the garbled state");
    Require(random_nid_postfix() == seed7Type1[1], "returning to the earlier state must resume its sequence");

    initstate_nid_postfix(7, small.data(), static_cast<std::int64_t>(small.size()));
    for (int draw = 0; draw < 3; ++draw) random_nid_postfix();
    alignas(4) std::array<char, 8> refused{};
    Require(initstate_nid_postfix(1, refused.data(), 4) == nullptr, "initstate below 8 bytes returns null");
    Require(Word(small.data(), 0) == 3 * 5 + 1, "initstate below 8 bytes still saves the current state's rear position");
    initstate_nid_postfix(7, small.data(), static_cast<std::int64_t>(small.size()));
    random_nid_postfix();
    random_nid_postfix();

    *__error_nid_postfix() = 1234;
    std::array<char, 8> tiny{};
    tiny.fill(static_cast<char>(0x5a));
    for (const std::int64_t size : {std::int64_t{7}, std::int64_t{0}, std::int64_t{-1}})
        Require(initstate_nid_postfix(1, tiny.data(), size) == nullptr, "initstate below 8 bytes must return null");
    Require(initstate_nid_postfix(1, nullptr, 4) == nullptr, "initstate below 8 bytes must return null before reading the state");
    Require(*__error_nid_postfix() == 1234, "initstate below 8 bytes must leave errno unchanged");
    Require(std::all_of(tiny.begin(), tiny.end(), [](char value) { return value == static_cast<char>(0x5a); }),
        "initstate below 8 bytes must not write the state");
    Require(random_nid_postfix() == seed7Type1[2], "a rejected initstate must keep the current state");

    bool threw = false;
    try {
        initstate_nid_postfix(1, nullptr, 128);

    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "initstate with a null state must throw");
    threw = false;
    try {
        setstate_nid_postfix(nullptr);

    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw, "setstate with a null state must throw");

    constexpr std::size_t threadCount = 4;
    constexpr std::size_t callsPerThread = 2000;
    srandom_nid_postfix(99);

    std::vector<std::int64_t> serial(threadCount * callsPerThread);
    for (auto& value : serial) value = random_nid_postfix();
    srandom_nid_postfix(99);

    std::vector<std::int64_t> concurrent(threadCount * callsPerThread);
    std::vector<std::thread> threads;
    for (std::size_t t = 0; t < threadCount; ++t)
        threads.emplace_back([&concurrent, t] {
            for (std::size_t i = 0; i < callsPerThread; ++i) concurrent[t * callsPerThread + i] = random_nid_postfix();
        });
    for (auto& thread : threads) thread.join();
    std::sort(serial.begin(), serial.end());
    std::sort(concurrent.begin(), concurrent.end());
    Require(serial == concurrent, "concurrent calls must draw the serial sequence");

    for (const std::int64_t size : {8, 32, 64, 128, 256}) {
        std::array<char, 256> entropyState{};
        char* previous = initstate_nid_postfix(42, entropyState.data(), size);
        srandomdev_nid_postfix();
        const std::size_t separation = size == 32 || size == 128 ? 3 : 1;
        std::int64_t expected;
        if (size == 8) {
            std::int64_t x = static_cast<std::int32_t>(Word(entropyState.data(), 1));
            if (x == 0) x = 123459876;
            x = 16807 * (x % 127773) - 2836 * (x / 127773);
            if (x < 0) x += 0x7fffffff;
            expected = x;
        } else {
            const std::uint32_t sum = Word(entropyState.data(), 1) + Word(entropyState.data(), 1 + separation);
            expected = sum >> 1;
        }
        Require(random_nid_postfix() == expected, "srandomdev seeds the shared library's active state from host entropy");
        setstate_nid_postfix(previous);
    }

    return failures == 0 ? 0 : 1;
}

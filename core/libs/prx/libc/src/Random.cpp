#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

constexpr int MaxTypes = 5;
constexpr int LinearCongruentialType = 0;
constexpr int LinearCongruentialShuffles = 50;
constexpr std::array<std::int64_t, MaxTypes> MinimumStateBytes {8, 32, 64, 128, 256};
constexpr std::array<int, MaxTypes> Degrees {0, 7, 15, 31, 63};
constexpr std::array<int, MaxTypes> Separations {0, 3, 1, 3, 1};

std::uint32_t g_defaultTable[32]{};

struct AdditiveFeedbackState {
    char* table;
    int type;
    int degree;
    int separation;
    int front;
    int rear;
};

std::mutex g_randomLock;
AdditiveFeedbackState g_random {reinterpret_cast<char*>(g_defaultTable), 3, 31, 3, 3, 0};

std::uint32_t LoadWord(const char* table, int index) {
    std::uint32_t value;
    std::memcpy(&value, table + sizeof(value) * static_cast<std::size_t>(index), sizeof(value));
    return value;
}

void StoreWord(char* table, int index, std::uint32_t value) {
    std::memcpy(table + sizeof(value) * static_cast<std::size_t>(index), &value, sizeof(value));
}

std::uint32_t GoodRand(std::uint32_t context) {
    std::int64_t x = static_cast<std::int32_t>(context);
    if (x == 0) x = 123459876;
    const std::int64_t high = x / 127773;
    const std::int64_t low = x % 127773;
    x = 16807 * low - 2836 * high;
    if (x < 0) x += 0x7fffffff;
    return static_cast<std::uint32_t>(x);
}

std::uint32_t NextLocked() {
    auto& state = g_random;
    if (state.type == LinearCongruentialType) {
        const std::uint32_t value = GoodRand(LoadWord(state.table, 1));
        StoreWord(state.table, 1, value);
        return value;
    }
    const std::uint32_t sum = LoadWord(state.table, 1 + state.front) + LoadWord(state.table, 1 + state.rear);
    StoreWord(state.table, 1 + state.front, sum);
    if (++state.front >= state.degree) {
        state.front = 0;
        ++state.rear;
    } else if (++state.rear >= state.degree) {
        state.rear = 0;
    }
    return sum >> 1;
}

void SeedLocked(std::uint32_t seed) {
    auto& state = g_random;
    StoreWord(state.table, 1, seed);
    int rounds = LinearCongruentialShuffles;
    if (state.type != LinearCongruentialType) {
        for (int i = 1; i < state.degree; ++i) StoreWord(state.table, 1 + i, GoodRand(LoadWord(state.table, i)));
        state.front = state.separation;
        state.rear = 0;
        rounds = 10 * state.degree;
    }
    for (int i = 0; i < rounds; ++i) NextLocked();
}

void SaveTypeWordLocked() {
    const auto& state = g_random;
    const int word = state.type == LinearCongruentialType ? state.type : MaxTypes * state.rear + state.type;
    StoreWord(state.table, 0, static_cast<std::uint32_t>(word));
}

bool SeedDefaultState() {
    SeedLocked(1);
    SaveTypeWordLocked();
    return true;
}

[[maybe_unused]] const bool g_defaultStateSeeded = SeedDefaultState();

std::uint16_t g_rand48Seed[3]{0x330e, 0xabcd, 0x1234};
constexpr std::uint64_t DefaultRand48Multiplier = 0x5deece66dull;
constexpr std::uint16_t DefaultRand48Addend = 0xb;
std::uint64_t g_rand48Multiplier = DefaultRand48Multiplier;
std::uint16_t g_rand48Addend = DefaultRand48Addend;
std::uint16_t g_seed48Result[3]{};

std::uint64_t NextRand48(std::uint16_t* seed, const char* function) {
    if (seed == nullptr) throw std::invalid_argument(std::string(function) + ": null seed");
    constexpr std::uint64_t Mask = (1ull << 48) - 1;
    std::uint64_t value = (static_cast<std::uint64_t>(seed[2]) << 32) | (static_cast<std::uint64_t>(seed[1]) << 16) | seed[0];
    value = (value * g_rand48Multiplier + g_rand48Addend) & Mask;
    seed[0] = static_cast<std::uint16_t>(value);
    seed[1] = static_cast<std::uint16_t>(value >> 16);
    seed[2] = static_cast<std::uint16_t>(value >> 32);
    return value;
}

}

extern "C" {

double APS5_VABI erand48_nid_postfix(std::uint16_t* seed) {
    return std::ldexp(static_cast<double>(NextRand48(seed, __func__) >> 16), -32);
}

std::int64_t APS5_VABI nrand48_nid_postfix(std::uint16_t* seed) {
    return static_cast<std::int64_t>(NextRand48(seed, __func__) >> 17);
}

std::int64_t APS5_VABI jrand48_nid_postfix(std::uint16_t* seed) {
    return static_cast<std::int64_t>(NextRand48(seed, __func__) >> 16);
}

double APS5_VABI drand48_nid_postfix() {
    std::lock_guard lock(g_randomLock);
    return std::ldexp(static_cast<double>(NextRand48(g_rand48Seed, __func__) >> 16), -32);
}

std::int64_t APS5_VABI lrand48_nid_postfix() {
    std::lock_guard lock(g_randomLock);
    return static_cast<std::int64_t>(NextRand48(g_rand48Seed, __func__) >> 17);
}

std::int64_t APS5_VABI mrand48_nid_postfix() {
    std::lock_guard lock(g_randomLock);
    return static_cast<std::int64_t>(NextRand48(g_rand48Seed, __func__) >> 16);
}

void APS5_VABI srand48_nid_postfix(std::int64_t seedval) {
    std::lock_guard lock(g_randomLock);
    g_rand48Seed[0] = 0x330e;
    g_rand48Seed[1] = static_cast<std::uint16_t>(static_cast<std::uint32_t>(seedval));
    g_rand48Seed[2] = static_cast<std::uint16_t>(static_cast<std::uint32_t>(seedval) >> 16);
    g_rand48Multiplier = DefaultRand48Multiplier;
    g_rand48Addend = DefaultRand48Addend;
}

std::uint16_t* APS5_VABI seed48_nid_postfix(const std::uint16_t* seed) {
    if (seed == nullptr) throw std::invalid_argument("seed48: null seed");
    std::lock_guard lock(g_randomLock);
    for (int index = 0; index < 3; ++index) g_rand48Seed[index] = g_seed48Result[index] = seed[index];
    g_rand48Multiplier = DefaultRand48Multiplier;
    g_rand48Addend = DefaultRand48Addend;
    return g_seed48Result;
}

void APS5_VABI lcong48_nid_postfix(const std::uint16_t* parameters) {
    if (parameters == nullptr) throw std::invalid_argument("lcong48: null parameters");
    std::lock_guard lock(g_randomLock);
    for (int index = 0; index < 3; ++index) g_rand48Seed[index] = parameters[index];
    g_rand48Multiplier = parameters[3] | (static_cast<std::uint64_t>(parameters[4]) << 16) | (static_cast<std::uint64_t>(parameters[5]) << 32);
    g_rand48Addend = parameters[6];
}

std::int64_t APS5_VABI random_nid_postfix() {
    std::lock_guard lock(g_randomLock);
    return static_cast<std::int64_t>(NextLocked());
}

void APS5_VABI srandom_nid_postfix(std::uint64_t seed) {
    std::lock_guard lock(g_randomLock);
    SeedLocked(static_cast<std::uint32_t>(seed));
}

void APS5_VABI srandomdev_nid_postfix() {
    std::lock_guard lock(g_randomLock);
    const auto words = g_random.type == LinearCongruentialType ? 1 : g_random.degree;
    HostEntropy_nid_no_patch(g_random.table + sizeof(std::uint32_t), static_cast<std::size_t>(words) * sizeof(std::uint32_t));
    if (g_random.type != LinearCongruentialType) {
        g_random.front = g_random.separation;
        g_random.rear = 0;
    }
}

char* APS5_VABI initstate_nid_postfix(std::uint64_t seed, char* state, std::int64_t size) {
    if (size < MinimumStateBytes[0]) return nullptr;
    if (state == nullptr) throw std::invalid_argument("initstate: null state");
    std::lock_guard lock(g_randomLock);
    char* previous = g_random.table;
    SaveTypeWordLocked();
    int type = MaxTypes - 1;
    while (size < MinimumStateBytes[static_cast<std::size_t>(type)]) --type;
    g_random = {state, type, Degrees[static_cast<std::size_t>(type)], Separations[static_cast<std::size_t>(type)], 0, 0};
    SeedLocked(static_cast<std::uint32_t>(seed));
    SaveTypeWordLocked();
    return previous;
}

char* APS5_VABI setstate_nid_postfix(char* state) {
    if (state == nullptr) throw std::invalid_argument("setstate: null state");
    std::lock_guard lock(g_randomLock);
    const std::uint32_t word = LoadWord(state, 0);
    const int type = static_cast<int>(word % MaxTypes);
    const std::uint32_t rear = word / MaxTypes;
    const int degree = Degrees[static_cast<std::size_t>(type)];
    if (type != LinearCongruentialType && rear >= static_cast<std::uint32_t>(degree)) return nullptr;
    char* previous = g_random.table;
    SaveTypeWordLocked();
    const int separation = Separations[static_cast<std::size_t>(type)];
    g_random.table = state;
    g_random.type = type;
    g_random.degree = degree;
    g_random.separation = separation;
    if (type != LinearCongruentialType) {
        g_random.rear = static_cast<int>(rear);
        g_random.front = (static_cast<int>(rear) + separation) % degree;
    }
    return previous;
}

}

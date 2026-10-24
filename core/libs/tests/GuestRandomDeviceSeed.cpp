#include "prx/libc/include/HostEntropy.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern "C" {
std::int64_t APS5_VABI random_nid_postfix();
void APS5_VABI srandomdev_nid_postfix();
char* APS5_VABI initstate_nid_postfix(std::uint64_t, char*, std::int64_t);
char* APS5_VABI setstate_nid_postfix(char*);
}

static void* entropyBuffer;
static std::size_t entropySize;
static bool failEntropy;

void HostEntropy_nid_no_patch(void* buffer, std::size_t size) {
    entropyBuffer = buffer;
    entropySize = size;
    if (failEntropy) throw std::runtime_error("entropy unavailable");
    auto* bytes = static_cast<unsigned char*>(buffer);
    for (std::size_t i = 0; i < size; ++i) bytes[i] = static_cast<unsigned char>(i * 37 + 11);
}

static void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static std::uint32_t Word(const char* buffer, std::size_t index) {
    std::uint32_t value;
    std::memcpy(&value, buffer + sizeof(value) * index, sizeof(value));
    return value;
}

int main() {
    constexpr std::array<std::size_t, 5> sizes {8, 32, 64, 128, 256};
    constexpr std::array<std::size_t, 5> degrees {1, 7, 15, 31, 63};
    constexpr std::array<std::size_t, 5> separations {0, 3, 1, 3, 1};
    std::array<char, 272> state{};
    for (std::size_t type = 0; type < sizes.size(); ++type) {
        state.fill(static_cast<char>(0xa5));
        char* table = state.data() + 1;
        char* previous = initstate_nid_postfix(42, table, static_cast<std::int64_t>(sizes[type]));
        for (int i = 0; i < 5; ++i) random_nid_postfix();
        const std::uint32_t header = Word(table, 0);
        srandomdev_nid_postfix();
        Require(entropyBuffer == table + 4, "srandomdev fills the active state after its header");
        Require(entropySize == degrees[type] * 4, "srandomdev fills every generator word");
        Require(Word(table, 0) == header, "srandomdev leaves the saved header untouched");
        for (std::size_t i = 0; i < entropySize; ++i)
            Require(static_cast<unsigned char>(table[4 + i]) == static_cast<unsigned char>(i * 37 + 11),
                "srandomdev neither derives words from one seed nor advances the generator");
        Require(state[0] == static_cast<char>(0xa5) &&
            std::all_of(state.begin() + 1 + sizes[type], state.end(), [](char c) { return c == static_cast<char>(0xa5); }),
            "srandomdev preserves the buffer guards");
        const auto seeded = state;
        if (type != 0) {
            const std::uint32_t sum = Word(table, 1) + Word(table, 1 + separations[type]);
            Require(random_nid_postfix() == sum / 2, "srandomdev resets the front and rear positions");
            Require(Word(table, 1 + separations[type]) == sum, "random advances the newly seeded table");
        } else {
            std::int64_t x = static_cast<std::int32_t>(Word(table, 1));
            if (x == 0) x = 123459876;
            x = 16807 * (x % 127773) - 2836 * (x / 127773);
            if (x < 0) x += 0x7fffffff;
            const auto expected = x;
            Require(random_nid_postfix() == static_cast<std::int64_t>(expected), "the linear generator uses the entropy word");
        }
        setstate_nid_postfix(previous);
        Require(Word(table, 0) == type + (type == 0 ? 0 : 5), "the reseeded state saves its new position");
        state = seeded;
        const std::uint32_t resetHeader = static_cast<std::uint32_t>(type);
        std::memcpy(table, &resetHeader, sizeof(resetHeader));
        setstate_nid_postfix(table);
        const auto beforeFailure = state;
        failEntropy = true;
        bool threw = false;
        try {
            srandomdev_nid_postfix();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        failEntropy = false;
        Require(threw, "srandomdev propagates entropy failures");
        Require(state == beforeFailure, "an entropy failure before writing keeps the state");
        setstate_nid_postfix(previous);
    }
}

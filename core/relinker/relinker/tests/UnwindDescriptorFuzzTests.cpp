#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);

namespace {

std::uint32_t NextRandom(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

std::vector<std::uint8_t> BuildSeedUnwind() {
    std::vector<std::uint8_t> seed(256, 0);
    seed[0] = 0;
    seed[1] = 1;
    seed[2] = 0;
    seed[3] = 3;
    seed[4] = 0;

    seed[5] = 0x40;
    seed[6] = 0x10;
    seed[13] = 1;
    seed[17] = 0x20;
    seed[25] = 0x60;
    seed[26] = 0x10;

    seed[65] = 16;
    seed[73] = 1;
    seed[74] = 0;
    seed[75] = 1;
    seed[76] = 0x78;
    seed[77] = 16;

    seed[97] = 24;
    seed[101] = 36;
    seed[105] = 0x20;
    seed[113] = 0x40;

    return seed;
}

}

int main() {
    const auto seed = BuildSeedUnwind();
    std::size_t casesExecuted = 0;

    for (std::size_t len = 0; len <= seed.size(); len += (len < 16 ? 1 : 16)) {
        LLVMFuzzerTestOneInput(seed.data(), len);
        ++casesExecuted;
    }

    for (std::size_t offset = 0; offset < seed.size(); ++offset) {
        auto mutated = seed;
        mutated[offset] ^= 0xFF;
        LLVMFuzzerTestOneInput(mutated.data(), mutated.size());
        ++casesExecuted;

        mutated[offset] = 0;
        LLVMFuzzerTestOneInput(mutated.data(), mutated.size());
        ++casesExecuted;
    }

    std::uint32_t rng = 0xCAFEBABE;
    for (std::size_t iter = 0; iter < 1000; ++iter) {
        const std::size_t len = NextRandom(rng) % 1024;
        std::vector<std::uint8_t> buffer(len);
        for (std::size_t i = 0; i < len; ++i) {
            buffer[i] = static_cast<std::uint8_t>(NextRandom(rng));
        }
        if (len >= 8 && (NextRandom(rng) % 2) == 0) {
            buffer[1] = 1;
            buffer[2] = 0;
            buffer[3] = 3;
            buffer[4] = 0;
        }
        LLVMFuzzerTestOneInput(buffer.data(), buffer.size());
        ++casesExecuted;
    }

    std::cout << "All " << casesExecuted << " unwind descriptor fuzz harness executions completed successfully.\n";
    return 0;
}

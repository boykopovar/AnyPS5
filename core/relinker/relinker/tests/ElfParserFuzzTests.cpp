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

std::vector<std::uint8_t> BuildSeedElf() {
    std::vector<std::uint8_t> seed(0x200, 0);
    const std::uint8_t magic[] = {0x7f, 'E', 'L', 'F', 2, 1, 1, 0, 0};
    std::memcpy(seed.data(), magic, sizeof(magic));

    seed[0x10] = 3;
    seed[0x12] = 62;
    seed[0x14] = 1;
    seed[0x20] = 64;
    seed[0x28] = 180;
    seed[0x34] = 64;
    seed[0x36] = 56;
    seed[0x38] = 1;
    seed[0x3a] = 64;
    seed[0x3c] = 1;

    seed[64] = 1;
    seed[68] = 7;
    seed[72] = 0x80;
    seed[80] = 0x1000;
    seed[88] = 0x1000;
    seed[96] = 0x40;
    seed[104] = 0x80;
    seed[112] = 0x1000;

    return seed;
}

}

int main() {
    const auto seed = BuildSeedElf();
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

    std::uint32_t rng = 0x1337BEEF;
    for (std::size_t iter = 0; iter < 1000; ++iter) {
        const std::size_t len = NextRandom(rng) % 1024;
        std::vector<std::uint8_t> buffer(len);
        for (std::size_t i = 0; i < len; ++i) {
            buffer[i] = static_cast<std::uint8_t>(NextRandom(rng));
        }
        if (len >= 4 && (NextRandom(rng) % 3) == 0) {
            buffer[0] = 0x7f;
            buffer[1] = 'E';
            buffer[2] = 'L';
            buffer[3] = 'F';
        }
        LLVMFuzzerTestOneInput(buffer.data(), buffer.size());
        ++casesExecuted;
    }

    std::cout << "All " << casesExecuted << " ELF parser fuzz harness executions completed successfully.\n";
    return 0;
}

#include "prx/libSceAgc/DcbFlow/include/Control.hpp"
#include "prx/libSceAgc/Patch/include/Rewind.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

constexpr std::uint32_t Sentinel = 0xabcdef01u;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Storage {
    std::array<std::uint32_t, 8> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

    Storage() {
        words.fill(Sentinel);
    }
};

void testRewind() {
    check(sceAgcDcbRewindGetSize() == 8, "REWIND packet size");
    for (std::uint32_t state : {0u, 1u}) {
        Storage storage;
        auto* packet = sceAgcDcbRewind(&storage.buffer, state);
        check(packet == storage.words.data() && packet[0] == 0xc0005900u && packet[1] == state << 31u, "REWIND does not carry the initial state in bit 31");
        check(storage.buffer.cursor_up == packet + sceAgcDcbRewindGetSize() / sizeof(std::uint32_t), "REWIND did not advance the cursor by its size");
        check(storage.words[2] == Sentinel, "REWIND wrote past its two words");
    }
}

void testRewindStatePatch() {
    Storage storage;
    auto* packet = sceAgcDcbRewind(&storage.buffer, 0);
    check(sceAgcRewindPatchSetRewindState(packet, 1) == 0 && packet[0] == 0xc0005900u && packet[1] == 0x80000000u, "the rewind state patch did not set bit 31");
    check(sceAgcRewindPatchSetRewindState(packet, 0) == 0 && packet[0] == 0xc0005900u && packet[1] == 0, "the rewind state patch did not clear bit 31");
    check(sceAgcAsyncRewindPatchSetRewindState(packet, 1) == 0 && packet[1] == 0x80000000u, "the asynchronous rewind state patch did not set bit 31");
    check(storage.words[2] == Sentinel, "a rewind state patch wrote past the packet");
}

}

int main() {
    try {
        testRewind();
        testRewindStatePatch();
        std::puts("AGC rewind tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

#include "prx/libSceAgc/Cb/include/Sync.hpp"
#include "prx/libSceAgc/Patch/include/QueueEndOfPipeAction.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

struct Storage {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

std::array<std::uint32_t, 8> release(std::uint8_t action, const volatile Label* label) {
    Storage storage;
    const auto* packet = sceAgcCbReleaseMem(&storage.buffer, action, 0x0a5, 1, 2, label, 2, 0x1122334455667788ull, 0, 0, 0, 0);
    std::array<std::uint32_t, 8> words{};
    for (std::size_t index = 0; index < words.size(); ++index) words[index] = packet[index];
    return words;
}

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

}

int main() {
    alignas(8) static Label label{};
    auto packet = release(0x28, &label);
    const auto original = packet;
    for (const std::uint8_t action : {0x00u, 0x14u, 0x2eu, 0x2fu, 0x30u, 0x3fu}) {
        if (sceAgcQueueEndOfPipeActionPatchType(packet.data(), action) != 0) return fail("the end-of-pipe type patch failed");
        if (packet != release(action, &label)) return fail("a patched end-of-pipe action differs from a release built with that action");
    }
    if (sceAgcQueueEndOfPipeActionPatchType(packet.data(), 0x28) != 0 || packet != original) return fail("patching the action back did not restore the packet");
    std::puts("AGC end-of-pipe type patch tests passed");
    return 0;
}

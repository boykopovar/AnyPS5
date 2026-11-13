#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/DcbFlow/include/Sync.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <array>
#include <cstdint>
#include <cstdio>

namespace {

struct Storage {
    std::array<std::uint32_t, 64> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};

    std::uint64_t written() const { return static_cast<std::uint64_t>(buffer.cursor_up - words.data()) * sizeof(std::uint32_t); }
};

int fail(const char* message) {
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

}

int main() {
    alignas(8) std::uint64_t label = 0;
    for (const std::uint32_t size : {0u, 1u}) {
        const std::uint64_t expected = size == 0 ? 56 : 64;
        if (sceAgcAcbWaitOnAddressGetSize(size) != expected || sceAgcDcbWaitOnAddressGetSize(size) != expected) return fail("wait size mismatch");
        Storage acb;
        Storage dcb;
        sceAgcAcbWaitRegMem(&acb.buffer, static_cast<std::uint8_t>(size), 3, 0, &label, 1, ~0ull, 0x40);
        sceAgcDcbWaitRegMem(&dcb.buffer, static_cast<std::uint8_t>(size), 3, 0, 0, &label, 1, ~0ull, 0x40);
        if (acb.written() != expected || dcb.written() != expected) return fail("wait size differs from the packets the wait builders write");
    }
    for (const std::uint32_t size : {2u, 3u, 4u, 8u, 0xffu, 0x100u, 0x101u, 0xffffffffu}) {
        if (sceAgcAcbWaitOnAddressGetSize(size) != 0 || sceAgcDcbWaitOnAddressGetSize(size) != 0) return fail("an invalid wait size did not return 0");
    }
    std::puts("AGC wait size tests passed");
    return 0;
}

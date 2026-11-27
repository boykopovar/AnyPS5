#include "prx/libSceAgc/Cb/include/Sync.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void checkPacket(const std::uint32_t* packet, const std::array<std::uint32_t, 8>& expected, const char* message) {
    for (std::size_t index = 0; index < expected.size(); ++index) check(packet[index] == expected[index], message);
}

std::array<std::uint32_t, 8> releaseMem(std::uint8_t action, std::uint16_t gcrControl, std::uint8_t dst, std::uint8_t cachePolicy, std::uintptr_t address) {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    const auto* packet = sceAgcCbReleaseMem(&buffer, action, gcrControl, dst, cachePolicy, reinterpret_cast<const volatile Label*>(address), 0, 0, 0, 0, 0, 0);
    check(packet == words.data(), "release mem returned unexpected packet pointer");
    check(buffer.cursor_up == words.data() + 8, "release mem cursor advance does not match 8 dwords");
    std::array<std::uint32_t, 8> result{};
    for (std::size_t index = 0; index < result.size(); ++index) result[index] = packet[index];
    return result;
}

void testAgcbuildCbReleaseMemA() {
    checkPacket(releaseMem(0x11, 0x2222, 0x33, 0x44, 0x55555555).data(), {0xc0064900u, 0x00222511u, 0x00030000u, 0x55555554u, 0u, 0u, 0u, 0u}, "agcbuild_CbReleaseMem_a packet mismatch");
}

void testAgcbuildCbReleaseMemB() {
    checkPacket(releaseMem(1, 2, 3, 4, 5).data(), {0xc0064900u, 0x00002501u, 0x00030000u, 0x00000004u, 0u, 0u, 0u, 0u}, "agcbuild_CbReleaseMem_b packet mismatch");
}

void testAgcbuildCbReleaseMemC() {
    checkPacket(releaseMem(0x22, 0x4444, 0, 0, 0).data(), {0xc0064900u, 0x00444522u, 0u, 0u, 0u, 0u, 0u, 0u}, "agcbuild_CbReleaseMem_c packet mismatch");
}

}

int main() {
    try {
        testAgcbuildCbReleaseMemA();
        testAgcbuildCbReleaseMemB();
        testAgcbuildCbReleaseMemC();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC release mem tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

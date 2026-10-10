#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

extern "C" {
std::uint32_t APS5_VABI sceAgcDcbQueueEndOfShaderActionGetSize();
std::uint32_t APS5_VABI sceAgcAcbQueueEndOfShaderActionGetSize();
std::uint32_t* APS5_VABI sceAgcCbReleaseMem(CommandBuffer*, std::uint8_t, std::uint16_t, std::uint8_t, std::uint8_t, const volatile Label*, std::uint8_t, std::uint64_t, std::uint16_t, std::uint16_t, std::uint8_t, std::uint32_t);
}

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main() {
    try {
        std::array<std::uint32_t, 16> words{};
        CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
        const auto* packet = sceAgcCbReleaseMem(&buffer, 0x2f, 0, 0, 0, nullptr, 0, 0, 0, 0, 0, 0);
        const auto releaseBytes = static_cast<std::uint32_t>((buffer.cursor_up - packet) * sizeof(std::uint32_t));
        check(((packet[0] >> 8u) & 0xffu) == 0x49u && releaseBytes == 32, "end-of-shader RELEASE_MEM size mismatch");
        check(sceAgcDcbQueueEndOfShaderActionGetSize() == releaseBytes, "DCB end-of-shader action size differs from its RELEASE_MEM");
        check(sceAgcDcbQueueEndOfShaderActionGetSize() == sceAgcAcbQueueEndOfShaderActionGetSize(), "DCB and ACB end-of-shader action sizes differ");
        LibcRunShutdown_nid_postfix();
        std::puts("AGC end-of-shader action size tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

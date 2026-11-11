#include "prx/libSceAgc/DcbState/include/ContextState.hpp"
#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"
#include "prx/libSceAgc/DcbFlow/include/Control.hpp"
#include "prx/libSceAgc/Cb/include/Sync.hpp"
#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testEndOfShaderActionSize() {
    std::array<std::uint32_t, 16> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    const auto* packet = sceAgcCbReleaseMem(&buffer, 0x2f, 0, 0, 0, nullptr, 0, 0, 0, 0, 0, 0);
    const auto releaseBytes = static_cast<std::uint32_t>((buffer.cursor_up - packet) * sizeof(std::uint32_t));
    check(((packet[0] >> 8u) & 0xffu) == 0x49u && releaseBytes == 32, "end-of-shader RELEASE_MEM size mismatch");
    check(sceAgcDcbQueueEndOfShaderActionGetSize() == releaseBytes, "DCB end-of-shader action size differs from its RELEASE_MEM");
    check(sceAgcDcbQueueEndOfShaderActionGetSize() == sceAgcAcbQueueEndOfShaderActionGetSize(), "DCB and ACB end-of-shader action sizes differ");
}

void testAtomicGds() {
    constexpr std::uint32_t expectedBytes = 11u * sizeof(std::uint32_t);
    check(sceAgcDcbAtomicGdsGetSize() == expectedBytes, "DCB atomic GDS size does not match 11-dword packet");

    std::array<std::uint32_t, 32> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
    const auto* packet = sceAgcDcbAtomicGds_0900(&buffer, 2, 0x2e1u, 1, 3, 7, 0x100, 0x200, 0x00ff00ffu, 0x12345678u, 0x1122334455667788ull, 0x99aabbccddeeff00ull);
    check(packet != nullptr && packet == words.data(), "DCB atomic GDS returned unexpected packet pointer");
    check(buffer.cursor_up == words.data() + 11, "DCB atomic GDS cursor advance does not match 11 dwords");
    check(packet[0] == 0xc0091d00u, "DCB atomic GDS header opcode mismatch");

    std::array<std::uint32_t, 32> unversionedWords{};
    CommandBuffer unversionedBuffer{unversionedWords.data(), unversionedWords.data() + unversionedWords.size(), unversionedWords.data(), unversionedWords.data() + unversionedWords.size(), nullptr, nullptr, 0};
    const auto* unversionedPacket = sceAgcDcbAtomicGds(&unversionedBuffer, 2, 0x2e1u, 1, 3, 7, 0x100, 0x200, 0x00ff00ffu, 0x12345678u, 0x1122334455667788ull, 0x99aabbccddeeff00ull);
    check(unversionedPacket != nullptr && unversionedPacket == unversionedWords.data(), "unversioned DCB atomic GDS returned unexpected packet pointer");
    check(unversionedBuffer.cursor_up == unversionedWords.data() + 11, "unversioned DCB atomic GDS cursor advance does not match 11 dwords");
    check(packet[1] == 0x801500e1u, "DCB atomic GDS operation dword mismatch");
    check(unversionedPacket[1] == 0x80150061u, "unversioned DCB atomic GDS kept operation bit 7");
    check(std::equal(words.begin() + 2, words.begin() + 11, unversionedWords.begin() + 2), "versioned and unversioned DCB atomic GDS emitted different payloads");
}

}

int main() {
    try {
        testEndOfShaderActionSize();
        testAtomicGds();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC end-of-shader action and atomic GDS size tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

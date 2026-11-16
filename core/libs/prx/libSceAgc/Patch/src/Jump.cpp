#include "prx/libSceAgc/Patch/include/Jump.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::array<std::uint32_t, 4> PatchJump(const std::uint32_t* cmd, const volatile std::uint32_t* target, std::uint32_t sizeInDwords, const char* function) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), 4, function);
    std::array<std::uint32_t, 4> packet{};
    Agc::Command::Require(GuestCopyIn_nid_no_patch(packet.data(), cmd, sizeof(packet[0])), function, "packet is not readable");
    Agc::Command::ValidatePacket(packet.data(), 0x3fu, 4, function);
    Agc::Command::Require(GuestCopyIn_nid_no_patch(packet.data() + 1, cmd + 1, 3 * sizeof(packet[0])), function, "packet is not readable");
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    Agc::Command::CheckAddress(address, 4, function);
    Agc::Command::CheckBits(sizeInDwords, 0xfffffu, function);
    packet[1] = static_cast<std::uint32_t>(address);
    packet[2] = static_cast<std::uint32_t>(address >> 32u);
    packet[3] = (packet[3] & ~0xfffffu) | sizeInDwords;
    return packet;
}

void WriteJump(std::uint32_t* cmd, const std::array<std::uint32_t, 4>& packet, const char* function) {
    Agc::Command::Require(GuestCopyOut_nid_no_patch(cmd + 1, packet.data() + 1, 3 * sizeof(packet[0])), function, "packet is not writable");
}

}

extern "C" {

int APS5_VABI sceAgcJumpPatchSetTarget(uint32_t* cmd, const volatile uint32_t* target, uint32_t size_in_dwords) {
    WriteJump(cmd, PatchJump(cmd, target, size_in_dwords, __func__), __func__);
    return 0;
}

int APS5_VABI sceAgcJumpPatchSetTarget_0300(uint32_t* cmd, uint64_t cache_policy, const volatile uint32_t* target, uint32_t size_in_dwords) {
    Agc::Command::CheckBits(cache_policy, 3u, __func__);
    auto packet = PatchJump(cmd, target, size_in_dwords, __func__);
    packet[3] = (packet[3] & ~(3u << 28u)) | (static_cast<std::uint32_t>(cache_policy) << 28u);
    WriteJump(cmd, packet, __func__);
    return 0;
}

}

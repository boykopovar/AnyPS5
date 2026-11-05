#include "prx/libSceAgc/Patch/include/Jump.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

void PatchJump(std::uint32_t* cmd, const volatile std::uint32_t* target, std::uint32_t sizeInDwords, const char* function) {
    Agc::Command::ValidatePacket(cmd, 0x3fu, 4, function);
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    Agc::Command::CheckAddress(address, 4, function);
    Agc::Command::CheckBits(sizeInDwords, 0xfffffu, function);
    cmd[1] = static_cast<std::uint32_t>(address);
    cmd[2] = static_cast<std::uint32_t>(address >> 32u);
    cmd[3] = (cmd[3] & ~0xfffffu) | sizeInDwords;
}

}

extern "C" {

int APS5_VABI sceAgcJumpPatchSetTarget(uint32_t* cmd, const volatile uint32_t* target, uint32_t size_in_dwords) {
    PatchJump(cmd, target, size_in_dwords, __func__);
    return 0;
}

int APS5_VABI sceAgcJumpPatchSetTarget_0300(uint32_t* cmd, uint64_t cache_policy, const volatile uint32_t* target, uint32_t size_in_dwords) {
    Agc::Command::CheckBits(cache_policy, 3u, __func__);
    PatchJump(cmd, target, size_in_dwords, __func__);
    cmd[3] = (cmd[3] & ~(3u << 28u)) | (static_cast<std::uint32_t>(cache_policy) << 28u);
    return 0;
}

}

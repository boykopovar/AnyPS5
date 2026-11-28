#include "prx/libSceAgc/Patch/include/Branch.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <array>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

void PatchBranchTarget(std::uint32_t* cmd, std::uint32_t field, const volatile std::uint32_t* target, std::uint32_t sizeInDwords, const char* function) {
    Agc::Command::ValidatePacket(cmd, 0x3fu, 14, function);
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    Agc::Command::CheckAddress(address, 4, function);
    Agc::Command::CheckBits(sizeInDwords, 0xfffffu, function);
    cmd[field] = static_cast<std::uint32_t>(address);
    cmd[field + 1u] = static_cast<std::uint32_t>(address >> 32u);
    cmd[field + 2u] = (cmd[field + 2u] & ~0xfffffu) | sizeInDwords;
}

void PatchBranchTarget(std::uint32_t* cmd, std::uint32_t field, std::uint8_t cachePolicy, const volatile std::uint32_t* target, std::uint32_t sizeInDwords, const char* function) {
    Agc::Command::CheckBits(cachePolicy, 3, function);
    PatchBranchTarget(cmd, field, target, sizeInDwords, function);
    cmd[field + 2u] = (cmd[field + 2u] & ~(3u << 28u)) | (static_cast<std::uint32_t>(cachePolicy) << 28u);
}

}

extern "C" {

int APS5_VABI sceAgcBranchPatchSetCompareAddress(std::uint32_t* cmd, const volatile std::uint64_t* compareAddress) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), 4, __func__);
    std::uint32_t header = 0;
    Agc::Command::Require(GuestCopyIn_nid_no_patch(&header, cmd, sizeof(header)), __func__, "packet is not readable");
    Agc::Command::ValidatePacket(&header, 0x3fu, 14, __func__);
    const auto address = reinterpret_cast<std::uintptr_t>(compareAddress);
    Agc::Command::CheckAddress(address, 8, __func__);
    const std::array<std::uint32_t, 2> words{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
    Agc::Command::Require(GuestCopyOut_nid_no_patch(cmd + 2, words.data(), sizeof(words)), __func__, "packet is not writable");
    return 0;
}

int APS5_VABI sceAgcBranchPatchSetThenTarget(std::uint32_t* cmd, const volatile std::uint32_t* target, std::uint32_t sizeInDwords) {
    PatchBranchTarget(cmd, 8, target, sizeInDwords, __func__);
    return 0;
}

int APS5_VABI sceAgcBranchPatchSetElseTarget(std::uint32_t* cmd, const volatile std::uint32_t* target, std::uint32_t sizeInDwords) {
    PatchBranchTarget(cmd, 11, target, sizeInDwords, __func__);
    return 0;
}

int APS5_VABI sceAgcBranchPatchSetThenTarget_0300(std::uint32_t* cmd, std::uint8_t cachePolicy, const volatile std::uint32_t* target, std::uint32_t sizeInDwords) {
    PatchBranchTarget(cmd, 8, cachePolicy, target, sizeInDwords, __func__);
    return 0;
}

int APS5_VABI sceAgcBranchPatchSetElseTarget_0300(std::uint32_t* cmd, std::uint8_t cachePolicy, const volatile std::uint32_t* target, std::uint32_t sizeInDwords) {
    PatchBranchTarget(cmd, 11, cachePolicy, target, sizeInDwords, __func__);
    return 0;
}

}

#include "prx/libSceAgc/Patch/include/Branch.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <limits>
#include <optional>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() {
        errno = saved;
    }
};

void PatchBranchTarget(std::uint32_t* cmd, std::uint32_t field, std::optional<std::uint8_t> cachePolicy, const volatile std::uint32_t* target, std::uint32_t sizeInDwords, const char* function) {
    ErrnoGuard errnoGuard;
    if (cachePolicy.has_value()) {
        Agc::Command::CheckBits(*cachePolicy, 3, function);
    }
    const auto cmdAddress = reinterpret_cast<std::uintptr_t>(cmd);
    Agc::Command::CheckAddress(cmdAddress, 4, function);
    Agc::Command::Require(cmdAddress <= std::numeric_limits<std::uintptr_t>::max() - (static_cast<std::size_t>(field) + 3u) * sizeof(std::uint32_t), function, "packet address wraps");
    std::uint32_t header = 0;
    Agc::Command::Require(GuestCopyIn_nid_no_patch(&header, cmd, sizeof(header)), function, "packet is not readable");
    Agc::Command::ValidatePacket(&header, 0x3fu, 14, function);
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    Agc::Command::CheckAddress(address, 4, function);
    Agc::Command::CheckBits(sizeInDwords, 0xfffffu, function);
    std::array<std::uint32_t, 3> words{};
    Agc::Command::Require(GuestCopyIn_nid_no_patch(words.data(), cmd + field, sizeof(words)), function, "packet is not readable");
    words[0] = static_cast<std::uint32_t>(address);
    words[1] = static_cast<std::uint32_t>(address >> 32u);
    words[2] = (words[2] & ~0xfffffu) | sizeInDwords;
    if (cachePolicy.has_value()) {
        words[2] = (words[2] & ~(3u << 28u)) | (static_cast<std::uint32_t>(*cachePolicy) << 28u);
    }
    Agc::Command::Require(GuestCopyOut_nid_no_patch(cmd + field, words.data(), sizeof(words)), function, "packet is not writable");
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
    PatchBranchTarget(cmd, 8, std::nullopt, target, sizeInDwords, __func__);
    return 0;
}

int APS5_VABI sceAgcBranchPatchSetElseTarget(std::uint32_t* cmd, const volatile std::uint32_t* target, std::uint32_t sizeInDwords) {
    PatchBranchTarget(cmd, 11, std::nullopt, target, sizeInDwords, __func__);
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

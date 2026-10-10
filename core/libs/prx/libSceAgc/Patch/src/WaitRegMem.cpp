#include "prx/libSceAgc/Patch/include/WaitRegMem.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

void ValidateWriteData(const std::uint32_t* cmd, const char* function) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), 4, function);
    Agc::Command::Require(((cmd[0] >> 8u) & 0xffu) == 0x37u, function, "not a WRITE_DATA packet");
}

int SetWriteDataAddress(std::uint32_t* cmd, std::uint64_t address, const char* function) {
    ValidateWriteData(cmd, function);
    cmd[2] = static_cast<std::uint32_t>(address);
    cmd[3] = static_cast<std::uint32_t>(address >> 32u);
    return 0;
}

int SetWriteDataDst(std::uint32_t* cmd, std::uint8_t dst, bool compute, const char* function) {
    ValidateWriteData(cmd, function);
    Agc::Command::CheckBits(dst, compute ? 0xfu : 0x1fu, function);
    Agc::Command::Require(dst != 0 || (cmd[1] & (1u << 20u)) == 0, function, "register writes do not support write confirmation");
    const auto mask = compute ? 0xfu << 8u : (1u << 30u) | (0xfu << 8u);
    const auto destination = compute ? static_cast<std::uint32_t>(dst) << 8u : ((dst & 1u) << 30u) | ((dst & 0x1eu) << 7u);
    cmd[1] = (cmd[1] & ~mask) | destination;
    return 0;
}

int SetWriteDataCachePolicy(std::uint32_t* cmd, std::uint8_t cachePolicy, const char* function) {
    ValidateWriteData(cmd, function);
    Agc::Command::CheckBits(cachePolicy, 3, function);
    cmd[1] = (cmd[1] & ~(3u << 25u)) | (static_cast<std::uint32_t>(cachePolicy) << 25u);
    return 0;
}

}

extern "C" {

int APS5_VABI sceAgcWaitRegMemPatchAddress(std::uint32_t* cmd, const volatile void* address) {
    auto* wait = Agc::Command::ValidateWait(cmd, __func__);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    const auto alignment = ((wait[0] >> 8u) & 0xffu) == 0x3cu ? 4u : 8u;
    Agc::Command::CheckAddress(guestAddress, alignment, __func__);
    Agc::Command::CheckBits(guestAddress, 0xffffffffffffull, __func__);
    cmd[2] = (cmd[2] & 0xffff0000u) | static_cast<std::uint32_t>(guestAddress >> 32u);
    cmd[3] = static_cast<std::uint32_t>(guestAddress);
    wait[2] = static_cast<std::uint32_t>(guestAddress);
    wait[3] = static_cast<std::uint32_t>(guestAddress >> 32u);
    return 0;
}

int APS5_VABI sceAgcWaitRegMemPatchReference(std::uint32_t* cmd, std::uint64_t reference) {
    auto* wait = Agc::Command::ValidateWait(cmd, __func__);
    Agc::Command::CheckBits(reference, 0xffffffffu, __func__);
    wait[4] = static_cast<std::uint32_t>(reference);
    return 0;
}

int APS5_VABI sceAgcWaitRegMemPatchMask(std::uint32_t* cmd, std::uint64_t mask) {
    auto* wait = Agc::Command::ValidateWait(cmd, __func__);
    Agc::Command::CheckBits(mask, 0xffffffffu, __func__);
    wait[((wait[0] >> 8u) & 0xffu) == 0x3cu ? 5 : 6] = static_cast<std::uint32_t>(mask);
    return 0;
}

int APS5_VABI sceAgcWaitRegMemPatchCompareFunction(std::uint32_t* cmd, std::uint8_t compareFunction) {
    auto* wait = Agc::Command::ValidateWait(cmd, __func__);
    Agc::Command::Require(compareFunction <= 6, __func__, "invalid wait comparison");
    wait[1] = (wait[1] & ~7u) | compareFunction;
    return 0;
}

int APS5_VABI sceAgcWriteDataPatchSetAddressOrOffset(std::uint32_t* cmd, std::uint64_t address) {
    return SetWriteDataAddress(cmd, address, __func__);
}

int APS5_VABI sceAgcAsyncWriteDataPatchSetAddressOrOffset(std::uint32_t* cmd, std::uint64_t address) {
    return SetWriteDataAddress(cmd, address, __func__);
}

int APS5_VABI sceAgcWriteDataPatchSetDst(std::uint32_t* cmd, std::uint8_t dst) {
    return SetWriteDataDst(cmd, dst, false, __func__);
}

int APS5_VABI sceAgcAsyncWriteDataPatchSetDst(std::uint32_t* cmd, std::uint8_t dst) {
    return SetWriteDataDst(cmd, dst, true, __func__);
}

int APS5_VABI sceAgcWriteDataPatchSetCachePolicy(std::uint32_t* cmd, std::uint8_t cachePolicy) {
    return SetWriteDataCachePolicy(cmd, cachePolicy, __func__);
}

int APS5_VABI sceAgcAsyncWriteDataPatchSetCachePolicy(std::uint32_t* cmd, std::uint8_t cachePolicy) {
    return SetWriteDataCachePolicy(cmd, cachePolicy, __func__);
}

int APS5_VABI sceAgcAcquireMemSetEngine(std::uint32_t* cmd, std::uint32_t engine) {
    Agc::Command::ValidatePacket(cmd, 0x58u, 8, __func__);
    Agc::Command::CheckBits(engine, 1, __func__);
    cmd[1] = (cmd[1] & 0x7fffffffu) | (engine << 31u);
    return 0;
}

}


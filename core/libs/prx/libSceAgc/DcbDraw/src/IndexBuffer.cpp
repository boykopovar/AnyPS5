#include "prx/libSceAgc/DcbDraw/include/IndexBuffer.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetIndexBuffer(CommandBuffer* buf, std::uint64_t indexAddress) {
    return Agc::Command::Emit(buf, 0x26u, {static_cast<std::uint32_t>(indexAddress) & ~1u, static_cast<std::uint32_t>(indexAddress >> 32u)}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetIndexBufferGetSize() {
    return 12;
}

std::uint32_t* APS5_VABI sceAgcDcbSetIndexCount(CommandBuffer* buf, std::uint32_t indexCount) {
    return Agc::Command::Emit(buf, 0x13u, {indexCount}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetIndexCountGetSize() {
    return 8;
}

std::uint32_t* APS5_VABI sceAgcDcbSetIndexSize(CommandBuffer* buf, std::uint8_t indexSize, std::uint8_t cachePolicy) {
    const std::uint32_t size = indexSize & 3u;
    Agc::Command::Require(size <= 2, __func__, "invalid index element size");
    return Agc::Command::Emit(buf, 0x7au, {0x20000243u, 0x400u | size | ((cachePolicy & 3u) << 6u)}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetIndexSizeGetSize() {
    return 12;
}

std::uint32_t* APS5_VABI sceAgcDcbSetIndexSize_0600(CommandBuffer* buf, std::uint8_t indexSize, std::uint8_t cachePolicy, std::uint8_t perInstanceObjectId) {
    Agc::Command::CheckBits(perInstanceObjectId, 1, __func__);
    auto* packet = sceAgcDcbSetIndexSize(buf, indexSize, cachePolicy);
    packet[2] |= static_cast<std::uint32_t>(perInstanceObjectId) << 14u;
    return packet;
}

std::uint32_t* APS5_VABI sceAgcDcbSetIndexIndirectArgs(CommandBuffer* buf, std::uint64_t attributeAddress, std::uint32_t attributeIndex) {
    return Agc::Command::Emit(buf, 0x91u, {static_cast<std::uint32_t>(attributeAddress) & ~0xfu, static_cast<std::uint32_t>(attributeAddress >> 32u), attributeIndex & 0xffffu}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbSetIndexIndirectArgsGetSize() {
    return 16;
}

}

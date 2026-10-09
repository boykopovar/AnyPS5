#include "prx/libSceAgc/DcbDraw/include/IndexBuffer.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbSetIndexBuffer(CommandBuffer* buf, std::uint64_t indexAddress) {
    if (indexAddress != 0) Agc::Command::CheckAddress(indexAddress, 2, __func__);
    return Agc::Command::Emit(buf, 0x26u, {static_cast<std::uint32_t>(indexAddress), static_cast<std::uint32_t>(indexAddress >> 32u)}, __func__);
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
    Agc::Command::Require(indexSize <= 2, __func__, "invalid index element size");
    Agc::Command::CheckBits(cachePolicy, 3, __func__);
    return Agc::Command::Emit(buf, 0x7au, {0x20000243u, 0x400u | indexSize | (static_cast<std::uint32_t>(cachePolicy) << 6u)}, __func__);
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

std::uint32_t* APS5_VABI sceAgcDcbSetIndexIndirectArgs(CommandBuffer* buf, std::uint32_t dataOffsetInBytes) {
    (void)buf;
    (void)dataOffsetInBytes;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

std::uint32_t APS5_VABI sceAgcDcbSetIndexIndirectArgsGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

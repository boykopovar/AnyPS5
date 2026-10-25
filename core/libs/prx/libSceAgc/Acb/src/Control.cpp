#include "prx/libSceAgc/Acb/include/Control.hpp"

#include "prx/libSceAgc/Command/include/Control.hpp"
#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/DcbState/include/Marker.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Resource/include/Registration.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::uint32_t* RejectUnregisteredWorkloadStream(CommandBuffer* buf, std::uint32_t streamId, const char* function) {
    if (buf == nullptr || !AgcDriverWorkloadStreamRegistered_nid_postfix(streamId)) return nullptr;
    NotImplemented_nid_no_patch(function);
    return nullptr;
}

}

extern "C" {

std::uint32_t* APS5_VABI sceAgcAcbAtomicGds_0900(CommandBuffer* buf, std::uint32_t control, std::uint32_t mode, std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask, std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData) {
    const auto operation = (control & 0xffu) | ((control & 0x200u) << 7u) |
        ((control & 0x20u) << 13u) | ((control & 0x40u) << 14u) |
        ((control & 0x20u) == 0 && mode == 1 ? 0x20000u : 0u);
    return Agc::Command::Emit(buf, 0x1du, {
        operation, ((gdsOp & 1u) << 8u) | (src & 0x3fu), offset, index,
        mask & 0x00ff00ffu, data, static_cast<std::uint32_t>(compareData),
        static_cast<std::uint32_t>(compareData >> 32u), static_cast<std::uint32_t>(extraData),
        static_cast<std::uint32_t>(extraData >> 32u)}, __func__);
}

std::uint32_t* APS5_VABI sceAgcAcbJump(CommandBuffer* buf, std::uint8_t cachePolicy, const std::uint32_t* target, std::uint32_t sizeInDwords) {
    return Agc::Command::WriteJump(buf, 1, cachePolicy, target, sizeInDwords, __func__);
}

uint32_t APS5_VABI sceAgcAcbJumpGetSize(void) {
    return 16;
}

// Encoded as the custom DISPATCH_RESET NOP packet the driver consumes on compute queues.
uint32_t* APS5_VABI sceAgcAcbResetQueue(CommandBuffer* buf, uint32_t op, uint32_t value) {
    (void)value;
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    constexpr std::uint32_t OpcodeNop = 0x10;
    constexpr std::uint32_t CustomDispatchReset = 0x09;
    auto* packet = Agc::Command::Allocate(buf, 2, __func__);
    packet[0] = Agc::Command::Header(OpcodeNop, 2, CustomDispatchReset << 2);
    packet[1] = op;
    return packet;
}

std::uint32_t* APS5_VABI sceAgcAcbRewind(CommandBuffer* buf, std::uint32_t initialState) {
    return Agc::Command::WriteRewind(buf, initialState, __func__);
}

std::uint32_t APS5_VABI sceAgcAcbRewindGetSize() {
    return 8;
}

std::uint32_t* APS5_VABI sceAgcAcbWaitUntilSafeForRendering(CommandBuffer* buf, std::uint32_t videoOutHandle, std::uint32_t displayBufferIndex) {
    auto* packet = Agc::Command::Allocate(buf, AgcDriver::RenderingWaitPacketWords, __func__);
    packet[0] = AgcDriver::RenderingWaitPacketHeader;
    packet[1] = videoOutHandle;
    packet[2] = displayBufferIndex;
    packet[3] = 0;
    return packet;
}

std::uint32_t* APS5_VABI sceAgcAcbSetFlip(CommandBuffer* buf, std::uint32_t videoOutHandle, std::int32_t displayBufferIndex, std::uint32_t flipMode, std::int64_t flipArg) {
    auto* packet = Agc::Command::Allocate(buf, AgcDriver::FlipPacketWords, __func__);
    packet[0] = AgcDriver::FlipPacketHeader;
    packet[1] = videoOutHandle;
    packet[2] = static_cast<std::uint32_t>(displayBufferIndex);
    packet[3] = flipMode;
    packet[4] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(flipArg));
    packet[5] = static_cast<std::uint32_t>(static_cast<std::uint64_t>(flipArg) >> 32u);
    return packet;
}

std::uint32_t* APS5_VABI sceAgcAcbSetWorkloadsActive(CommandBuffer* buf, std::uint32_t streamId, const std::uint32_t* workloadIds, std::uint32_t workloadCount) {
    (void)workloadIds;
    (void)workloadCount;
    return RejectUnregisteredWorkloadStream(buf, streamId, __func__);
}

std::uint32_t* APS5_VABI sceAgcAcbSetWorkloadComplete(CommandBuffer* buf, std::uint32_t streamId, std::uint32_t workloadId) {
    (void)workloadId;
    return RejectUnregisteredWorkloadStream(buf, streamId, __func__);
}

std::uint32_t* APS5_VABI sceAgcAcbSetWorkloadStreamInactive(CommandBuffer* buf, std::uint32_t streamId) {
    return RejectUnregisteredWorkloadStream(buf, streamId, __func__);
}

uint32_t* APS5_VABI sceAgcAcbPushMarker(CommandBuffer* buf, const char* str, uint32_t color) {
    (void)color;
    return Agc::Marker::Push(buf, str, __func__);
}

uint32_t* APS5_VABI sceAgcAcbPushMarkerSpan(CommandBuffer* buf, const char* str, uint32_t length, uint32_t color) {
    (void)color;
    return Agc::Marker::PushSpan(buf, str, length, __func__);
}

uint32_t* APS5_VABI sceAgcAcbSetMarkerSpan(CommandBuffer* buf, const char* str, uint32_t length, uint32_t color) {
    (void)color;
    auto* packet = Agc::Marker::PushSpan(buf, str, length, __func__);
    Agc::Marker::Pop(buf, __func__);
    return packet;
}

uint32_t* APS5_VABI sceAgcAcbPopMarker(CommandBuffer* buf) {
    return Agc::Marker::Pop(buf, __func__);
}

uint32_t* APS5_VABI sceAgcAcbSetMarker(CommandBuffer* buf, const char* str, uint32_t color) {
    (void)color;
    auto* packet = Agc::Marker::Push(buf, str, __func__);
    Agc::Marker::Pop(buf, __func__);
    return packet;
}

}

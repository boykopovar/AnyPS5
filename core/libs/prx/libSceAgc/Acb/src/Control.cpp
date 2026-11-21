#include "prx/libSceAgc/Acb/include/Control.hpp"

#include "prx/libSceAgc/Command/include/Control.hpp"
#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include <limits>
#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/DcbState/include/Marker.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Resource/include/Registration.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::uint32_t OpcodeNop = 0x10;
constexpr std::uint32_t ActivePacketWords = 18;
constexpr std::uint32_t CompletePacketWords = 12;
constexpr std::uint32_t InactivePacketWords = 9;
constexpr std::uint32_t MaxWorkloadId = 63;
constexpr std::uint32_t MaxActiveCount = 63;

bool ReserveReset(CommandBuffer* buffer, std::uint32_t words, bool checkCallback) {
    const auto available = [&] {
        const auto up = reinterpret_cast<std::uintptr_t>(buffer->cursor_up);
        const auto down = reinterpret_cast<std::uintptr_t>(buffer->cursor_down);
        Agc::Command::Require(((up | down) & 3u) == 0 && up <= down, __func__, "invalid command buffer cursors");
        const auto count = (down - up) / sizeof(std::uint32_t);
        return count >= buffer->reserved_dw ? count - buffer->reserved_dw : 0;
    };
    if (available() >= words) return true;
    if (buffer->callback == nullptr || words > std::numeric_limits<std::uint32_t>::max() - buffer->reserved_dw) return false;
    const auto grown = buffer->callback(buffer, words + buffer->reserved_dw, buffer->user_data);
    return (!checkCallback || grown) && available() >= words;
}

std::uint32_t* NewWorkloadPacket(CommandBuffer* buf, std::uint32_t words, const char* function) {
    auto* packet = Agc::Command::Allocate(buf, words, function);
    std::memset(packet, 0, words * sizeof(std::uint32_t));
    packet[0] = Agc::Command::Header(OpcodeNop, words, 0);
    return packet;
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
    return Agc::Command::WriteJump(buf, true, 1, cachePolicy, target, sizeInDwords, __func__);
}

uint32_t APS5_VABI sceAgcAcbJumpGetSize(void) {
    return 16;
}

std::uint32_t* APS5_VABI sceAgcAcbResetQueue(CommandBuffer* buf, std::uint32_t flags, std::uint32_t instance) {
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    flags &= 0xffffu;
    if ((flags & 0x8000u) == 0 && instance >= 2) NotImplemented_nid_no_patch(__func__);
    const auto state = Agc::Command::GetQueueResetState(instance, (flags & 0x8000u) != 0);
    instance = state.instance;
    const auto marker = 0xce000000u | (!state.external ? 0x200000u : 0u) | flags;
    const auto prefixWords = sceAgcDriverUserDataGetPacketSize(4);
    if (!ReserveReset(buf, prefixWords, false)) return nullptr;
    auto* result = buf->cursor_up;
    const auto version = Agc::Command::GetQueueResetState(instance, false).version;
    buf->cursor_up += sceAgcDriverUserDataWritePacket(buf->cursor_up, 1, marker, &version, sizeof(version));
    if ((flags & 0x80u) != 0) {
        if (!ReserveReset(buf, 2, true)) return nullptr;
        auto* event = buf->cursor_up;
        buf->cursor_up += 2;
        event[0] = Agc::Command::Header(0x46u, 2);
        event[1] = 0x407u;
    }
    if ((flags & 2u) != 0) {
        const auto commands = Agc::Command::GetQueueResetState(instance, false).commands;
        if (commands.empty()) return nullptr;
        if (!ReserveReset(buf, static_cast<std::uint32_t>(commands.size()), true)) return nullptr;
        auto* restore = buf->cursor_up;
        buf->cursor_up += commands.size();
        std::memcpy(restore, commands.data(), commands.size() * sizeof(std::uint32_t));
    }
    if ((flags & 0x140u) != 0) {
        const auto cache = (flags & 0x100u) != 0 ? 0xc3a1u : 0xc1u;
        if (sceAgcAcbAcquireMem(buf, cache, nullptr, 0, 0) == nullptr) return nullptr;
    }
    const auto suffixWords = sceAgcDriverUserDataGetPacketSize(0);
    if (!ReserveReset(buf, suffixWords, false)) return nullptr;
    buf->cursor_up += sceAgcDriverUserDataWritePacket(buf->cursor_up, 1, (marker & 0xcf600000u) | 0x800000u, nullptr, 0);
    return result;
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
    if (buf == nullptr || workloadIds == nullptr || workloadCount == 0 || workloadCount > MaxActiveCount || !AgcDriverWorkloadStreamRegistered_nid_postfix(streamId)) return nullptr;
    std::uint64_t mask = 0;
    for (std::uint32_t index = 0; index < workloadCount; ++index) {
        const auto id = workloadIds[index];
        if (id > MaxWorkloadId) return nullptr;
        const std::uint64_t bit = std::uint64_t{1} << id;
        if ((mask & bit) != 0) return nullptr;
        mask |= bit;
    }
    auto* packet = NewWorkloadPacket(buf, ActivePacketWords, __func__);
    packet[1] = streamId;
    packet[2] = static_cast<std::uint32_t>(mask);
    packet[3] = static_cast<std::uint32_t>(mask >> 32u);
    return packet;
}

std::uint32_t* APS5_VABI sceAgcAcbSetWorkloadComplete(CommandBuffer* buf, std::uint32_t streamId, std::uint32_t workloadId) {
    if (buf == nullptr || workloadId > MaxWorkloadId || !AgcDriverWorkloadStreamRegistered_nid_postfix(streamId)) return nullptr;
    const std::uint64_t clearMask = ~(std::uint64_t{1} << workloadId);
    auto* packet = NewWorkloadPacket(buf, CompletePacketWords, __func__);
    packet[1] = streamId;
    packet[2] = workloadId;
    packet[3] = static_cast<std::uint32_t>(clearMask);
    packet[4] = static_cast<std::uint32_t>(clearMask >> 32u);
    return packet;
}

std::uint32_t* APS5_VABI sceAgcAcbSetWorkloadStreamInactive(CommandBuffer* buf, std::uint32_t streamId) {
    if (buf == nullptr || !AgcDriverWorkloadStreamRegistered_nid_postfix(streamId)) return nullptr;
    auto* packet = NewWorkloadPacket(buf, InactivePacketWords, __func__);
    packet[1] = streamId;
    return packet;
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

std::uint32_t* APS5_VABI sceAgcAcbAtomicGds(CommandBuffer* buf, std::uint32_t control, std::uint32_t mode, std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask, std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData) {
    return sceAgcAcbAtomicGds_0900(buf, control & ~0x80u, mode, gdsOp, src, offset, index, mask, data, compareData, extraData);
}

}

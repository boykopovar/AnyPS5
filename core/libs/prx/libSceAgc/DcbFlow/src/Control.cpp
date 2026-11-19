#include "prx/libSceAgc/DcbFlow/include/Control.hpp"

#include "prx/libSceAgc/Command/include/Control.hpp"
#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include <cstring>
#include <initializer_list>
#include <limits>

namespace {

bool ReserveDrawReset(CommandBuffer* buffer, std::uint32_t words, bool checkCallback = true) {
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

bool DrawResetPacket(CommandBuffer* buffer, std::initializer_list<std::uint32_t> words) {
    if (!ReserveDrawReset(buffer, static_cast<std::uint32_t>(words.size()))) return false;
    std::memcpy(buffer->cursor_up, words.begin(), words.size() * sizeof(std::uint32_t));
    buffer->cursor_up += words.size();
    return true;
}

bool DrawResetRegisters(CommandBuffer* buffer, std::uint32_t header, std::span<const ShaderRegister> registers) {
    const auto pointer = reinterpret_cast<std::uintptr_t>(registers.data());
    return DrawResetPacket(buffer, {header, static_cast<std::uint32_t>(pointer) & ~3u, static_cast<std::uint32_t>(pointer >> 32u), 0x80000000u, static_cast<std::uint32_t>(registers.size()) & 0x3fffu});
}

}


extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbAtomicGds_0900(CommandBuffer* buf, std::uint32_t engine, std::uint32_t control, std::uint32_t mode, std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask, std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData) {
    const auto operation = (control & 0xffu) | ((control & 0x200u) << 7u) |
        ((control & 0x20u) << 13u) | ((control & 0x40u) << 14u) |
        ((control & 0x20u) == 0 && mode == 1 ? 0x20000u : 0u) | ((engine & 3u) << 30u);
    return Agc::Command::Emit(buf, 0x1du, {
        operation, ((gdsOp & 1u) << 8u) | (src & 0x3fu), offset, index,
        mask & 0x00ff00ffu, data, static_cast<std::uint32_t>(compareData),
        static_cast<std::uint32_t>(compareData >> 32u), static_cast<std::uint32_t>(extraData),
        static_cast<std::uint32_t>(extraData >> 32u)}, __func__);
}

std::uint32_t* APS5_VABI sceAgcDcbAtomicGds(CommandBuffer* buf, std::uint32_t engine, std::uint32_t control, std::uint32_t mode, std::uint32_t gdsOp, std::uint32_t src, std::uint16_t offset, std::uint16_t index, std::uint32_t mask, std::uint32_t data, std::uint64_t compareData, std::uint64_t extraData) {
    return sceAgcDcbAtomicGds_0900(buf, engine, control & ~0x80u, mode, gdsOp, src, offset, index, mask, data, compareData, extraData);
}


uint32_t* APS5_VABI sceAgcDcbJump(CommandBuffer* buf, uint8_t mode, uint8_t cache_policy, const uint32_t* target, uint32_t size_in_dwords) {
    return Agc::Command::WriteJump(buf, false, mode, cache_policy, target, size_in_dwords, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbJumpGetSize() {
    return 16;
}

std::uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, std::uint32_t flags, std::uint32_t instance) {
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    flags &= 0xffffu;
    if ((flags & 0x400u) != 0 || ((flags & 0x8000u) == 0 && instance >= 2)) NotImplemented_nid_no_patch(__func__);
    const auto initial = Agc::Command::GetQueueResetState(instance, (flags & 0x8000u) != 0);
    instance = initial.instance;
    const auto marker = 0xce000000u | (!initial.external ? 0x200000u : 0u) | flags;
    if (!ReserveDrawReset(buf, 1)) return nullptr;
    auto* result = buf->cursor_up;
    *buf->cursor_up++ = 0xffff1000u;
    if (!ReserveDrawReset(buf, sceAgcDriverUserDataGetPacketSize(4), false)) return nullptr;
    const auto version = Agc::Command::GetQueueResetState(instance, false).version;
    buf->cursor_up += sceAgcDriverUserDataWritePacket(buf->cursor_up, 0, marker, &version, sizeof(version));
    if ((flags & 0x80u) != 0 && !DrawResetPacket(buf, {0xc0004600u, 0x407u})) return nullptr;
    if ((flags & 0x100u) != 0 && !DrawResetPacket(buf, {0xc0004600u, 0x410u})) return nullptr;
    if ((flags & 1u) != 0 && !DrawResetRegisters(buf, 0xc0039f00u, Agc::Command::GetQueueResetState(instance, false).context)) return nullptr;
    if ((flags & 2u) != 0 && !DrawResetRegisters(buf, 0xc0036300u, Agc::Command::GetQueueResetState(instance, false).shader)) return nullptr;
    if ((flags & 0x804u) != 0) {
        const auto state = Agc::Command::GetQueueResetState(instance, false);
        auto registers = state.userConfig;
        if ((flags & 4u) == 0) registers = registers.last(state.counterRegisters);
        else if ((flags & 0x800u) == 0) registers = registers.first(registers.size() - state.counterRegisters);
        const auto pfp = (static_cast<std::uint32_t>(sceKernelGetMainSocId()) & 0xffffffe0u) == 0x840f00u;
        if (!DrawResetRegisters(buf, 0xc0036400u | (pfp ? 4u : 0u), registers)) return nullptr;
    }
    if ((flags & 8u) != 0 && !DrawResetPacket(buf, {0xc0002f00u, 1u})) return nullptr;
    if ((flags & 0x10u) != 0 && !DrawResetPacket(buf, {0xc0017a00u, 0x20000243u, 0x480u})) return nullptr;
    if ((flags & 0x10u) != 0 && !DrawResetPacket(buf, {0xc0012600u, 0u, 0u})) return nullptr;
    if ((flags & 0x10u) != 0 && !DrawResetPacket(buf, {0xc0001300u, 0xffffffffu})) return nullptr;
    if ((flags & 0x20u) != 0 && !DrawResetPacket(buf, {0xc0021102u, 1u, 0u, 0u})) return nullptr;
    if ((flags & 0x20u) != 0 && !DrawResetPacket(buf, {0xc0021100u, 1u, 0u, 0u})) return nullptr;
    if ((flags & 0x40u) != 0) {
        if (!DrawResetPacket(buf, {0xc0065800u, 0x86007fc0u, 0xfffffffeu, 0xffu, 1u, 0u, 0x19u, 0xc3e1u})) return nullptr;
        if (!DrawResetPacket(buf, {0xc0004600u, 0x2eu})) return nullptr;
        if (!DrawResetPacket(buf, {0xc0004600u, 0x2cu})) return nullptr;
    }
    if ((flags & 0x200u) != 0 && !DrawResetPacket(buf, {0xc0024600u, 0x138u, 0x07fffc00u, 0u})) return nullptr;
    if (!ReserveDrawReset(buf, sceAgcDriverUserDataGetPacketSize(0), false)) return nullptr;
    buf->cursor_up += sceAgcDriverUserDataWritePacket(buf->cursor_up, 0, (marker & 0xcf600000u) | 0x800000u, nullptr, 0);
    return result;
}

std::uint32_t* APS5_VABI sceAgcDcbClearState(CommandBuffer* buf, std::uint32_t command) {
    return Agc::Command::Emit(buf, 0x12u, {command & 0xfu}, __func__);
}

uint32_t* APS5_VABI sceAgcDcbRewind(CommandBuffer* buf, uint32_t initial_state) {
    return Agc::Command::WriteRewind(buf, initial_state, __func__);
}

uint32_t APS5_VABI sceAgcDcbRewindGetSize(void) {
    return 8;
}

uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer* buf, uint32_t video_out_handle, uint32_t display_buffer_index) {
    auto* packet = Agc::Command::Allocate(buf, AgcDriver::RenderingWaitPacketWords, __func__);
    packet[0] = AgcDriver::RenderingWaitPacketHeader;
    packet[1] = video_out_handle;
    packet[2] = display_buffer_index;
    packet[3] = 0;
    return packet;
}

}

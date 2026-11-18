#include "prx/libSceAgc/DcbState/include/ContextState.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <limits>

namespace {

bool ReserveContextPacket(CommandBuffer* buffer, std::uint32_t words) {
    const auto available = [&] {
        const auto up = reinterpret_cast<std::uintptr_t>(buffer->cursor_up);
        const auto down = reinterpret_cast<std::uintptr_t>(buffer->cursor_down);
        const auto bottom = reinterpret_cast<std::uintptr_t>(buffer->bottom);
        const auto top = reinterpret_cast<std::uintptr_t>(buffer->top);
        Agc::Command::Require(((bottom | top | up | down) & 3u) == 0 && bottom <= up && up <= down && down <= top, __func__, "invalid command buffer cursors");
        const auto count = (down - up) / sizeof(std::uint32_t);
        return count >= buffer->reserved_dw ? count - buffer->reserved_dw : 0;
    };
    if (available() >= words) return true;
    if (buffer->callback == nullptr || buffer->reserved_dw > std::numeric_limits<std::uint32_t>::max() - words) return false;
    return buffer->callback(buffer, buffer->reserved_dw + words, buffer->user_data) && available() >= words;
}

}

extern "C" {

uint32_t* APS5_VABI sceAgcDcbContextStateOp(CommandBuffer* buf, uint32_t operation) {
    if (operation == 3) {
        Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
        if (!ReserveContextPacket(buf, 3)) return nullptr;
        auto* first = buf->cursor_up;
        first[0] = Agc::Command::Header(0x28u, 3);
        first[1] = 0;
        first[2] = 0x80018001u;
        buf->cursor_up += 3;
        if (!ReserveContextPacket(buf, 2)) return nullptr;
        buf->cursor_up[0] = Agc::Command::Header(0x12u, 2);
        buf->cursor_up[1] = 1;
        buf->cursor_up += 2;
        return sceAgcDcbContextStateOp_0100(buf, 0) == nullptr ? nullptr : first;
    }
    if (operation != 1) {
        return sceAgcDcbContextStateOp_0100(buf, operation);
    }
    Agc::Command::Reserve(buf, 5, __func__);
    auto* first = Agc::Command::Emit(buf, 0x28u, {0u, 0x80018001u}, __func__);
    Agc::Command::Emit(buf, 0x12u, {operation}, __func__);
    return first;
}

uint64_t APS5_VABI sceAgcDcbContextStateOpGetSize(uint32_t operation) {
    if (operation > 3) {
        return 0;
    }
    if (operation == 0) {
        return 5 * sizeof(std::uint32_t);
    }
    if (operation == 3) {
        return 32 * sizeof(std::uint32_t);
    }
    return 27 * sizeof(std::uint32_t);
}

uint32_t* APS5_VABI sceAgcDcbContextStateOp_0100(CommandBuffer* buf, uint32_t operation) {
    Agc::Command::Require(buf != nullptr, __func__, "null command buffer");
    Agc::Command::Require(operation <= 3, __func__, "invalid context state operation");
    const auto* function = __func__;
    std::uint32_t* first = nullptr;
    const auto append = [&](std::uint32_t count) {
        auto* packet = Agc::Command::WriteNop(buf, count, function);
        if (first == nullptr) {
            first = packet;
            packet[0] = Agc::Command::Header(0x10u, count, 0x68u);
            packet[1] = operation;
        }
    };
    if (operation == 0) {
        const auto registers = Agc::Command::GetCurrentDefaultCxRegisters();
        if (!ReserveContextPacket(buf, 5)) return nullptr;
        auto* packet = buf->cursor_up;
        const auto pointer = reinterpret_cast<std::uintptr_t>(registers.data());
        packet[0] = Agc::Command::Header(0x9fu, 5);
        packet[1] = static_cast<std::uint32_t>(pointer) & ~3u;
        packet[2] = static_cast<std::uint32_t>(pointer >> 32u);
        packet[3] = 0x80000000u;
        packet[4] = static_cast<std::uint32_t>(registers.size()) & 0x3fffu;
        buf->cursor_up += 5;
        return packet;
    }
    if (operation == 2) {
        append(3);
    }
    Agc::Command::Reserve(buf, 22, function);
    append(5);
    append(8);
    append(9);
    if (operation != 2) {
        append(3);
    }
    append(2);
    if (operation == 3) {
        append(5);
    }
    return first;
}

std::uint32_t APS5_VABI sceAgcDcbSetBaseDispatchIndirectArgsGetSize() {
    return 16;
}

std::uint32_t APS5_VABI sceAgcDcbSetBaseDrawIndirectArgsGetSize() {
    return 16;
}

std::uint32_t APS5_VABI sceAgcDcbQueueEndOfShaderActionGetSize() {
    return 32;
}

std::uint32_t APS5_VABI sceAgcDcbGetLodStatsGetSize() {
    return 20;
}

}

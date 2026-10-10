#include "prx/libSceAgc/DcbState/include/ContextState.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbContextStateOp(CommandBuffer* buf, uint32_t operation) {
    (void)buf;
    (void)operation;
    throw std::runtime_error("sceAgcDcbContextStateOp: not implemented");
}

uint64_t APS5_VABI sceAgcDcbContextStateOpGetSize(uint32_t operation) {
    Agc::Command::Require(operation <= 3, __func__, "invalid context state operation");
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
        append(5);
        return first;
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

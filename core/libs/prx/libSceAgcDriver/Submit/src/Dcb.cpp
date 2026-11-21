#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <cstring>
#include <iterator>
#include <limits>
#include <stdexcept>

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

int AgcDriver::SubmitMany(std::uint32_t* const* addresses, const std::uint32_t* sizes, std::uint32_t count, std::uint32_t queue) {
    if (count == 0) return 0;
    if (count > std::numeric_limits<std::uint32_t>::max() / sizeof(std::uint32_t*)) {
        throw std::runtime_error("AGC driver: multi-submit count overflow");
    }
    if (!addresses || !sizes) throw std::runtime_error("AGC driver: null multi-submit arrays");
    AgcDriver::GuestMemory::CheckRange(addresses, count * sizeof(*addresses), alignof(std::uint32_t*));
    AgcDriver::GuestMemory::CheckRange(sizes, count * sizeof(*sizes), alignof(std::uint32_t));
    for (std::uint32_t i = 0; i < count; ++i) {
        if (sizes[i] != 0 && addresses[i] == nullptr) {
            throw std::runtime_error("AGC driver: null multi-submit command buffer");
        }
        const Packet packet{addresses[i], sizes[i], 0, {}};
        AgcDriver::Submit(&packet, queue);
    }
    return 0;
}

namespace {

std::uint32_t PayloadDwords(std::uint32_t bytes) {
    return static_cast<std::uint32_t>((std::uint64_t{bytes} + 3) >> 2);
}

std::uint32_t WorkloadDataPacketDwords(std::uint32_t bytes) {
    if (bytes == 0) return 3;
    const std::uint32_t payload = PayloadDwords(bytes);
    return payload == 1 ? 4 : payload + 7;
}

constexpr std::uint32_t UserDataRegisterPacket = 0xC0017900u;
constexpr std::uint32_t UserDataInlineDword = 0x10000u;
constexpr std::uint32_t UserDataRegister = 0x342u;
constexpr std::uint32_t UserDataWritePacket = 0xC0003700u;
constexpr std::uint32_t UserDataWriteControl = 0x06010000u;
constexpr std::uint32_t UserDataWriteRegister = 0xC343u;
constexpr std::uint32_t UserDataDrawQueue = 4u;
constexpr std::uint32_t UserDataPopMarker = 0xC2000000u;
constexpr std::uint32_t UserDataPushMarker = 0xC1000000u;
constexpr std::uint32_t UserDataSetMarker = 0xC0000000u;

std::uint32_t WriteUserData(std::uint32_t* commands, std::uint32_t asyncCompute, std::uint32_t value, const std::uint8_t* data, std::uint32_t size) {
    const bool inlineData = data != nullptr && size != 0;
    AgcDriver::GuestMemory::CheckRange(commands, std::size_t{inlineData ? WorkloadDataPacketDwords(size) : 3u} * sizeof(std::uint32_t), alignof(std::uint32_t), true);
    if (inlineData) AgcDriver::GuestMemory::CheckRange(data, size, 1);
    const std::uint32_t queue = asyncCompute == 0 ? UserDataDrawQueue : 0u;
    commands[0] = UserDataRegisterPacket + (inlineData ? UserDataInlineDword : 0u) + queue;
    commands[1] = UserDataRegister;
    commands[2] = value;
    auto* next = commands + 3;
    if (inlineData) {
        const auto first = std::min(size, 4u);
        commands[3] = 0;
        std::memcpy(commands + 3, data, first);
        size -= first;
        data += first;
        next = commands + 4;
    }
    if (data == nullptr || size == 0) return static_cast<std::uint32_t>(next - commands);
    const std::uint64_t padded = std::uint64_t{size} + 3u;
    next[3] = 0;
    next[4] = 0;
    next[0] = UserDataWritePacket + queue + (((static_cast<std::uint32_t>(padded) << 14u) + 0x20000u) & 0x3FFF0000u);
    next[1] = UserDataWriteControl;
    next[2] = UserDataWriteRegister;
    next[4 + static_cast<std::uint32_t>((padded >> 2u) - 1u)] = 0;
    std::memcpy(next + 4, data, size);
    next += 4 + (padded >> 2u);
    return static_cast<std::uint32_t>(next - commands);
}

std::uint32_t MarkerValue(std::uint32_t kind, std::uint32_t nameSize, std::uint32_t color) {
    const auto dwords = ((nameSize << 13u) + 0x6000u) & 0x7F8000u;
    const auto packedColor = ((color >> 1u) & 0x7C00u) | ((color >> 14u) & 0x3E0u) | (color >> 27u);
    return kind + dwords + packedColor;
}

}

extern "C" {

std::uint32_t APS5_VABI sceAgcDriverGetWaitRenderingPacketSizeInDwords() {
    return AgcDriver::RenderingWaitPacketWords;
}

std::uint32_t APS5_VABI sceAgcDriverGetSetFlipPacketSizeInDwords() {
    return AgcDriver::FlipPacketWords;
}

std::uint32_t APS5_VABI sceAgcDriverGetSetWorkloadsActivePacketSize() {
    return WorkloadDataPacketDwords(8) + 9;
}

std::uint32_t APS5_VABI sceAgcDriverGetSetWorkloadCompletePacketSize() {
    return WorkloadDataPacketDwords(0) + 9;
}

std::uint32_t APS5_VABI sceAgcDriverGetSetWorkloadStreamInactivePacketSize() {
    return WorkloadDataPacketDwords(8);
}

std::uint32_t APS5_VABI sceAgcDriverNopDataGetPacketSize(std::uint32_t bytes) {
    return PayloadDwords(bytes) + 2;
}

std::uint32_t APS5_VABI sceAgcDriverUserDataGetPacketSize(std::uint32_t bytes) {
    return WorkloadDataPacketDwords(bytes);
}

std::uint32_t APS5_VABI sceAgcDriverUserDataWritePacket(std::uint32_t* commands, std::uint32_t asyncCompute, std::uint32_t value, const void* data, std::uint32_t size) {
    return WriteUserData(commands, asyncCompute, value, static_cast<const std::uint8_t*>(data), size);
}

std::uint32_t APS5_VABI sceAgcDriverUserDataWritePushMarker(std::uint32_t* commands, std::uint32_t asyncCompute, const char* name, std::uint32_t nameSize, std::uint32_t color) {
    return WriteUserData(commands, asyncCompute, MarkerValue(UserDataPushMarker, nameSize, color), reinterpret_cast<const std::uint8_t*>(name), nameSize);
}

std::uint32_t APS5_VABI sceAgcDriverUserDataWriteSetMarker(std::uint32_t* commands, std::uint32_t asyncCompute, const char* name, std::uint32_t nameSize, std::uint32_t color) {
    return WriteUserData(commands, asyncCompute, MarkerValue(UserDataSetMarker, nameSize, color), reinterpret_cast<const std::uint8_t*>(name), nameSize);
}

std::uint32_t APS5_VABI sceAgcDriverUserDataWritePopMarker(std::uint32_t* commands, std::uint32_t asyncCompute) {
    return WriteUserData(commands, asyncCompute, UserDataPopMarker, nullptr, 0);
}

std::uint32_t APS5_VABI sceAgcDriverWaitUntilSafeForRendering(std::uint32_t** command,
    std::uint32_t capacity, std::uint32_t mode, std::uint32_t handle, int index) {
    if (!command || capacity < AgcDriver::RenderingWaitPacketWords || mode != 0 || index < 0)
        throw std::invalid_argument("AGC driver: invalid rendering wait arguments");
    AgcDriver::GuestMemory::CheckRange(command, sizeof(*command), alignof(std::uint32_t*), true);
    AgcDriver::GuestMemory::CheckRange(*command, AgcDriver::RenderingWaitPacketWords * sizeof(std::uint32_t), alignof(std::uint32_t), true);
    const std::uint32_t words[] = {AgcDriver::RenderingWaitPacketHeader, handle, static_cast<std::uint32_t>(index), mode};
    std::copy(std::begin(words), std::end(words), *command);
    *command += AgcDriver::RenderingWaitPacketWords;
    return 0;
}

int APS5_VABI sceAgcDriverSubmitDcb(const Packet* packet) {
    AgcDriver::Submit(packet, 0);
    return 0;
}

int APS5_VABI sceAgcDriverSubmitMultiDcbs(std::uint32_t* const* dcbGpuAddrs, const std::uint32_t* dcbSizesInDwords, std::uint32_t count) {
    return AgcDriver::SubmitMany(dcbGpuAddrs, dcbSizesInDwords, count, 0);
}

int APS5_VABI sceAgcDriverAgrSubmitDcb(const Packet* packet) {
    AgcDriver::Submit(packet, 0);
    return 0;
}

int APS5_VABI sceAgcDriverAgrSubmitMultiDcbs(std::uint32_t* const* dcbGpuAddrs, const std::uint32_t* dcbSizesInDwords, std::uint32_t count) {
    return AgcDriver::SubmitMany(dcbGpuAddrs, dcbSizesInDwords, count, 0);
}

}

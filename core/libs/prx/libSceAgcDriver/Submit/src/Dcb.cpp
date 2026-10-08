#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <iterator>

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

int AgcDriver::SubmitMany(std::uint32_t* const* addresses, const std::uint32_t* sizes, std::uint32_t count, std::uint32_t queue) {
    if (count == 0) return 0;
    if (!addresses || !sizes) throw std::runtime_error("AGC driver: null multi-submit arrays");
    AgcDriver::GuestMemory::CheckRange(addresses, count * sizeof(*addresses), alignof(std::uint32_t*));
    AgcDriver::GuestMemory::CheckRange(sizes, count * sizeof(*sizes), alignof(std::uint32_t));
    for (std::uint32_t i = 0; i < count; ++i) {
        const Packet packet{addresses[i], sizes[i], 0, {}};
        AgcDriver::Submit(&packet, queue);
    }
    return 0;
}

extern "C" {

std::uint32_t APS5_VABI sceAgcDriverGetWaitRenderingPacketSizeInDwords() {
    return AgcDriver::RenderingWaitPacketWords;
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

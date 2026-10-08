#include "prx/libSceAgcDriver/Submit/include/CommandBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDriverSubmitCommandBuffer(void* queue_context, const Packet* packet) {
    if (queue_context == nullptr || packet == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": null queue context or command buffer");
    }
    AgcDriver::Submit(packet, 0);
    return 0;
}

int APS5_VABI sceAgcDriverSubmitMultiCommandBuffers(void* queue_context, uint32_t* const* command_buffers, const uint32_t* sizes_in_dwords, uint32_t count) {
    if (count == 0) return 0;
    if (queue_context == nullptr || command_buffers == nullptr || sizes_in_dwords == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": null queue context or command buffer arrays");
    }
    AgcDriver::GuestMemory::CheckRange(command_buffers, count * sizeof(*command_buffers), alignof(std::uint32_t*));
    AgcDriver::GuestMemory::CheckRange(sizes_in_dwords, count * sizeof(*sizes_in_dwords), alignof(std::uint32_t));
    for (uint32_t i = 0; i < count; ++i) {
        const Packet packet{command_buffers[i], sizes_in_dwords[i], 0, {}};
        AgcDriver::Submit(&packet, 0);
    }
    return 0;
}

}

#include "prx/libSceAgcDriver/Submit/include/CommandBuffer.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDriverSubmitCommandBuffer(void* queue_context, const Packet* packet) {
    const auto queue = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(queue_context));
    if (queue != 0 && (queue < 0x20 || queue >= 0x58)) {
        throw std::runtime_error(std::string(__func__) + ": unsupported queue");
    }
    AgcDriver::Submit(packet, queue);
    return 0;
}

int APS5_VABI sceAgcDriverSubmitMultiCommandBuffers(void* queue_context, uint32_t* const* command_buffers, const uint32_t* sizes_in_dwords, uint32_t count) {
    if (count == 0) return 0;
    const auto queue = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(queue_context));
    if (queue != 0 && (queue < 0x20 || queue >= 0x58)) {
        throw std::runtime_error(std::string(__func__) + ": unsupported queue");
    }
    return AgcDriver::SubmitMany(command_buffers, sizes_in_dwords, count, queue);
}

}

#include "prx/libSceAgcDriver/Submit/include/CommandBuffer.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionRing.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

bool readQueueId(const void* queue_context, std::uint32_t& queue) {
    const auto base = reinterpret_cast<std::uintptr_t>(queue_context);
    if (base > std::numeric_limits<std::uintptr_t>::max() - 4 - sizeof(queue)) return false;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&queue, reinterpret_cast<const void*>(base + 4), sizeof(queue));
    errno = savedErrno;
    return copied;
}

void validateQueue(std::uint32_t queue, const char* caller) {
    if (queue != 0 && (queue < 0x20 || queue >= AgcDriver::DriverDetail::UnmeasuredSubmissionQueueEnd)) {
        throw std::runtime_error(std::string(caller) + ": unsupported queue");
    }
}

}

extern "C" {

int APS5_VABI sceAgcDriverSubmitCommandBuffer(void* queue_context, const Packet* packet) {
    if (!queue_context || !packet) return EINVAL;
    std::uint32_t queue = 0;
    if (!readQueueId(queue_context, queue)) return EINVAL;
    validateQueue(queue, __func__);
    AgcDriver::Submit(packet, queue);
    return 0;
}

int APS5_VABI sceAgcDriverSubmitMultiCommandBuffers(void* queue_context, uint32_t* const* command_buffers, const uint32_t* sizes_in_dwords, uint32_t count) {
    if (count == 0) return 0;
    if (!queue_context || !command_buffers || !sizes_in_dwords) return EINVAL;
    std::uint32_t queue = 0;
    if (!readQueueId(queue_context, queue)) return EINVAL;
    validateQueue(queue, __func__);
    return AgcDriver::SubmitMany(command_buffers, sizes_in_dwords, count, queue);
}

}

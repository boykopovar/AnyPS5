#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestArena.hpp"
#ifndef _WIN32
#include "prx/libc/include/specifics/linux/HostMapping.hpp"
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

namespace AgcDriver::Graphics {
namespace {
namespace {
void ResidentBuffer::FlushChanged(const Context& context) {
    auto& devices = Devices();
    std::vector<std::shared_ptr<ResidentMemory>> unzeroed;
    {
        std::lock_guard lock(devices.mutex);
        for (const auto& [key, held] : devices.memories) {
            if (std::get<0>(key) != context.device) continue;
            if (auto memory = held.lock(); memory != nullptr && !memory->Zeroed()) unzeroed.push_back(std::move(memory));
        }
    }
    for (auto& memory : unzeroed) recordTransfer(context, [&](VkCommandBuffer commands, std::vector<std::shared_ptr<void>>& kept) { memory->FillZero(commands, kept); });
    std::vector<std::weak_ptr<ResidentBuffer>> waiting;
    {
        std::lock_guard lock(devices.pendingMutex);
        waiting.swap(devices.pending);
    }
    for (std::size_t index = 0; index < waiting.size(); ++index) {
        const auto held = waiting[index].lock();
        if (held == nullptr) continue;
        if (held->context.device != context.device) {
            std::lock_guard lock(devices.pendingMutex);
            devices.pending.push_back(waiting[index]);
            continue;
        }
        try {
            held->flush();
        } catch (...) {
            std::lock_guard lock(devices.pendingMutex);
            devices.pending.insert(devices.pending.end(), waiting.begin() + static_cast<std::ptrdiff_t>(index) + 1, waiting.end());
            throw;
        }
    }
}

void ResidentBuffer::submit(const std::vector<VkSparseMemoryBind>& binds) {
    const bool ownQueue = context.sparseQueue != VK_NULL_HANDLE && context.sparseQueueMutex != nullptr;
    const VkSparseBufferMemoryBindInfo bufferBinds{buffer, static_cast<std::uint32_t>(binds.size()), binds.data()};
    VkBindSparseInfo bind{VK_STRUCTURE_TYPE_BIND_SPARSE_INFO};
    bind.bufferBindCount = 1;
    bind.pBufferBinds = &bufferBinds;
    const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    if (const auto result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &fence); result != VK_SUCCESS) fail("vkCreateFence of a resident heap mirror bind", result);
    VkResult submitted = VK_SUCCESS;
    VkResult waited = VK_SUCCESS;
    {
        std::unique_lock<std::mutex> queueLock;
        std::unique_lock<GuestMemory::GpuMutexType> gpuLock;
        if (ownQueue) queueLock = std::unique_lock(*context.sparseQueueMutex);
        else gpuLock = std::unique_lock(GuestMemory::GpuMutex());
        submitted = context.Function<PFN_vkQueueBindSparse>("vkQueueBindSparse")(ownQueue ? context.sparseQueue : context.queue, 1, &bind, fence);
        if (submitted == VK_SUCCESS) waited = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    if (submitted != VK_SUCCESS) fail("vkQueueBindSparse of a resident heap mirror", submitted);
    if (waited != VK_SUCCESS) fail("vkWaitForFences of a resident heap mirror bind", waited);
}

}
}
}

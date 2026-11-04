#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "AllocationFailure.hpp"
#include "CommandBatchFailure.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <new>
#include <string>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::Tests::SetAllocationFailureCountdown;

enum class Fault { None, Timing, End, Submit };

struct Probe {
    PFN_vkGetDeviceProcAddr real = nullptr;
    Fault fault = Fault::None;
    VkFence fence = VK_NULL_HANDLE;
    bool pending = false;
    unsigned submitted = 0;
    unsigned queries = 0;
    unsigned idleWaits = 0;
    unsigned fenceWaits = 0;
    unsigned premature = 0;
    unsigned safetyWaits = 0;
    unsigned nativePending = 0;
    unsigned freed = 0;
    unsigned destroyed = 0;
};

Probe probe;
VkDevice activeDevice = VK_NULL_HANDLE;
VkQueue activeQueue = VK_NULL_HANDLE;

void GuardRelease(VkDevice device, VkQueue queue) {
    if (!probe.pending) return;
    ++probe.premature;
    ++probe.safetyWaits;
    if (probe.real != nullptr) {
        const auto status = reinterpret_cast<PFN_vkGetFenceStatus>(probe.real(device, "vkGetFenceStatus"))(device, probe.fence);
        if (status == VK_NOT_READY) ++probe.nativePending;
        else if (status != VK_SUCCESS) std::terminate();
        const auto wait = reinterpret_cast<PFN_vkQueueWaitIdle>(probe.real(device, "vkQueueWaitIdle"));
        if (wait(queue) != VK_SUCCESS) std::terminate();
    }
    probe.pending = false;
}

VKAPI_ATTR VkResult VKAPI_CALL AllocateCommands(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* commands) {
    *commands = reinterpret_cast<VkCommandBuffer>(std::uintptr_t{4});
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL CreateFence(VkDevice, const VkFenceCreateInfo*, const VkAllocationCallbacks*, VkFence* fence) {
    *fence = reinterpret_cast<VkFence>(std::uintptr_t{5});
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL BeginCommands(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return VK_SUCCESS; }

VKAPI_ATTR VkResult VKAPI_CALL EndCommands(VkCommandBuffer commands) {
    if (probe.fault == Fault::End) return VK_ERROR_OUT_OF_HOST_MEMORY;
    if (probe.real == nullptr) return VK_SUCCESS;
    return reinterpret_cast<PFN_vkEndCommandBuffer>(probe.real(activeDevice, "vkEndCommandBuffer"))(commands);
}

VKAPI_ATTR VkResult VKAPI_CALL Submit(VkQueue queue, std::uint32_t count, const VkSubmitInfo* submissions, VkFence fence) {
    if (probe.fault == Fault::Submit) return VK_ERROR_OUT_OF_HOST_MEMORY;
    const auto result = probe.real != nullptr ? reinterpret_cast<PFN_vkQueueSubmit>(probe.real(activeDevice, "vkQueueSubmit"))(queue, count, submissions, fence) : VK_SUCCESS;
    if (result == VK_SUCCESS) {
        ++probe.submitted;
        probe.pending = true;
        probe.fence = fence;
        if (probe.fault == Fault::Timing) SetAllocationFailureCountdown(0);
    }
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL FenceStatus(VkDevice device, VkFence fence) {
    ++probe.queries;
    const auto result = probe.real != nullptr ? reinterpret_cast<PFN_vkGetFenceStatus>(probe.real(device, "vkGetFenceStatus"))(device, fence) : VK_NOT_READY;
    if (result == VK_SUCCESS) probe.pending = false;
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL QueueIdle(VkQueue queue) {
    ++probe.idleWaits;
    const auto result = probe.real != nullptr ? reinterpret_cast<PFN_vkQueueWaitIdle>(probe.real(activeDevice, "vkQueueWaitIdle"))(queue) : VK_SUCCESS;
    if (result == VK_SUCCESS) probe.pending = false;
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL WaitFences(VkDevice device, std::uint32_t count, const VkFence* fences, VkBool32 all, std::uint64_t timeout) {
    ++probe.fenceWaits;
    const auto result = probe.real != nullptr ? reinterpret_cast<PFN_vkWaitForFences>(probe.real(device, "vkWaitForFences"))(device, count, fences, all, timeout) : VK_SUCCESS;
    if (result == VK_SUCCESS) probe.pending = false;
    return result;
}

VKAPI_ATTR void VKAPI_CALL FreeCommands(VkDevice device, VkCommandPool pool, std::uint32_t count, const VkCommandBuffer* commands) {
    GuardRelease(device, activeQueue);
    probe.freed += count;
    if (probe.real != nullptr) reinterpret_cast<PFN_vkFreeCommandBuffers>(probe.real(device, "vkFreeCommandBuffers"))(device, pool, count, commands);
}

VKAPI_ATTR void VKAPI_CALL DestroyFence(VkDevice device, VkFence fence, const VkAllocationCallbacks* allocator) {
    GuardRelease(device, activeQueue);
    ++probe.destroyed;
    if (probe.real != nullptr) reinterpret_cast<PFN_vkDestroyFence>(probe.real(device, "vkDestroyFence"))(device, fence, allocator);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL DeviceProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkEndCommandBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(EndCommands);
    if (std::strcmp(name, "vkQueueSubmit") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Submit);
    if (std::strcmp(name, "vkGetFenceStatus") == 0) return reinterpret_cast<PFN_vkVoidFunction>(FenceStatus);
    if (std::strcmp(name, "vkQueueWaitIdle") == 0) return reinterpret_cast<PFN_vkVoidFunction>(QueueIdle);
    if (std::strcmp(name, "vkWaitForFences") == 0) return reinterpret_cast<PFN_vkVoidFunction>(WaitFences);
    if (std::strcmp(name, "vkFreeCommandBuffers") == 0) return reinterpret_cast<PFN_vkVoidFunction>(FreeCommands);
    if (std::strcmp(name, "vkDestroyFence") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyFence);
    if (probe.real != nullptr) return probe.real(device, name);
    if (std::strcmp(name, "vkAllocateCommandBuffers") == 0) return reinterpret_cast<PFN_vkVoidFunction>(AllocateCommands);
    if (std::strcmp(name, "vkCreateFence") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreateFence);
    if (std::strcmp(name, "vkBeginCommandBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(BeginCommands);
    return nullptr;
}

bool RunCase(const Context& original, Fault fault, bool wait, bool native) {
    probe = {};
    probe.real = native ? original.deviceProc : nullptr;
    probe.fault = fault;
    activeDevice = original.device;
    activeQueue = original.queue;
    auto context = original;
    context.deviceProc = DeviceProc;
    std::unique_ptr<Buffer> output;
    constexpr std::size_t bytes = 256;
    constexpr std::uint32_t pattern = 0x5a3c71e9u;
    if (native) {
        output = std::make_unique<Buffer>(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        std::memset(output->Bytes().data(), 0, bytes);
    }
    bool allocationFailed = false;
    bool apiFailed = false;
    bool waitFailed = false;
    {
        AgcDriver::FrameTiming frame(1, true);
        AgcDriver::PerformanceContext timing(&frame);
        try {
            CommandBatch batch(context);
            if (output) {
                context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(batch.Handle(), output->Handle(), 0, bytes, pattern);
                RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            }
            try {
                batch.Submit();
            } catch (const std::bad_alloc&) {
                if (!wait) throw;
                allocationFailed = true;
            }
            SetAllocationFailureCountdown(-1);
            if (wait && probe.submitted != 0) {
                try {
                    batch.Wait();
                } catch (const std::exception&) {
                    waitFailed = true;
                }
            }
        } catch (const std::bad_alloc&) {
            allocationFailed = true;
        } catch (const std::runtime_error& error) {
            apiFailed = std::string(error.what()).find(fault == Fault::End ? "vkEndCommandBuffer" : "vkQueueSubmit graphics") != std::string::npos;
        }
        SetAllocationFailureCountdown(-1);
    }
    bool pixels = true;
    if (output && probe.submitted != 0) {
        output->Invalidate();
        const auto* words = reinterpret_cast<const std::uint32_t*>(output->Bytes().data());
        pixels = std::all_of(words, words + bytes / sizeof(*words), [](auto word) { return word == pattern; });
    }
    const bool rejected = fault == Fault::End || fault == Fault::Submit;
    const bool complete = rejected ? probe.queries == 0 && probe.idleWaits == 0 && probe.fenceWaits == 0 : wait ? probe.fenceWaits == 1 : probe.queries == 1;
    const bool passed = allocationFailed == (fault == Fault::Timing) && apiFailed == rejected && !waitFailed && !probe.pending && probe.submitted == (rejected ? 0u : 1u) && probe.premature == 0 && probe.freed == 1 && probe.destroyed == 1 && complete && pixels;
    std::printf("command batch fault=%u wait=%u allocation failed=%u submitted=%u premature=%u safety waits=%u queries=%u idle waits=%u fence waits=%u readbacks=%u correct=%u wait failed=%u native pending=%u\n", static_cast<unsigned>(fault), wait, allocationFailed, probe.submitted, probe.premature, probe.safetyWaits, probe.queries, probe.idleWaits, probe.fenceWaits, output && probe.submitted != 0 ? 1u : 0u, pixels, waitFailed, probe.nativePending);
    return passed;
}

}

void AgcDriver::Tests::RunCommandBatchFailureTests(const AgcDriver::Graphics::Context& context, bool native) {
    unsigned failures = 0;
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    for (const auto fault : {Fault::Timing, Fault::None}) {
        for (const auto wait : {false, true}) if (!RunCase(context, fault, wait, native)) ++failures;
    }
    for (const auto fault : {Fault::End, Fault::Submit}) if (!RunCase(context, fault, false, native)) ++failures;
    Require(failures == 0, "command batch failure handling released pending work or lost its submission state");
}

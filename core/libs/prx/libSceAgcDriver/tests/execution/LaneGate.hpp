#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_LANEGATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_EXECUTION_LANEGATE_HPP

#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

namespace AgcDriver::Tests {

class TimelineGate {
public:
    explicit TimelineGate(VulkanDevice& device) : device(device), context(device.graphicsContext()) {
        signal = context.Function<PFN_vkSignalSemaphoreKHR>("vkSignalSemaphoreKHR");
        destroy = context.Function<PFN_vkDestroySemaphore>("vkDestroySemaphore");
        const VkSemaphoreTypeCreateInfoKHR type{VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO_KHR, nullptr, VK_SEMAPHORE_TYPE_TIMELINE_KHR, 0};
        const VkSemaphoreCreateInfo info{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, &type};
        Graphics::Check(context.Function<PFN_vkCreateSemaphore>("vkCreateSemaphore")(context.device, &info, nullptr, &semaphore), "create timeline gate");
        try {
            watchdog = std::thread([this] {
                std::unique_lock lock(releaseMutex);
                if (!releaseCondition.wait_for(lock, std::chrono::seconds(5), [this] { return released; })) {
                    expired = true;
                    releaseLocked();
                }
            });
        } catch (...) {
            destroy(context.device, semaphore, nullptr);
            throw;
        }
    }

    ~TimelineGate() {
        {
            std::lock_guard lock(releaseMutex);
            releaseLocked();
        }
        watchdog.join();
        try {
            std::unique_lock lock(GuestMemory::GpuMutex(), std::defer_lock);
            if (!GuestMemory::GpuMutex().HeldByThisThread()) lock.lock();
            device.WaitIdle();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "timeline gate cleanup: %s\n", error.what());
        }
        destroy(context.device, semaphore, nullptr);
    }

    TimelineGate(const TimelineGate&) = delete;
    TimelineGate& operator=(const TimelineGate&) = delete;

    void Record(VkCommandBuffer commands) {
        Graphics::Require(commands != VK_NULL_HANDLE, "timeline gate needs an open command buffer");
        auto* recorder = Graphics::Recorder::Active();
        Graphics::Require(recorder != nullptr && recorder->AsyncLaneActive(), "timeline gate needs an active native async recorder");
        recorder->HoldNextSubmit(semaphore, 1);
    }

    void Release() {
        std::lock_guard lock(releaseMutex);
        releaseLocked();
        Graphics::Check(releaseResult, "release timeline gate");
        Graphics::Require(!expired, "timeline gate exceeded its bounded hold");
    }

private:
    void releaseLocked() {
        if (released) return;
        const VkSemaphoreSignalInfoKHR info{VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO_KHR, nullptr, semaphore, 1};
        releaseResult = signal(context.device, &info);
        released = true;
        releaseCondition.notify_all();
        if (releaseResult != VK_SUCCESS) std::fprintf(stderr, "timeline gate signal failed: %d\n", static_cast<int>(releaseResult));
    }

    VulkanDevice& device;
    Graphics::Context context;
    PFN_vkSignalSemaphoreKHR signal = nullptr;
    PFN_vkDestroySemaphore destroy = nullptr;
    VkSemaphore semaphore = VK_NULL_HANDLE;
    std::mutex releaseMutex;
    std::condition_variable releaseCondition;
    std::thread watchdog;
    bool released = false;
    bool expired = false;
    VkResult releaseResult = VK_SUCCESS;
};

}

#endif

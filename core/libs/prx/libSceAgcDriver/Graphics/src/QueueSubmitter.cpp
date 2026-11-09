#include "prx/libSceAgcDriver/Graphics/include/QueueSubmitter.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadPriority.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <chrono>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

struct Submitters {
    std::mutex mutex;
    std::map<VkQueue, QueueSubmitter*> byQueue;
};

Submitters& AllSubmitters() {
    static Submitters* const submitters = new Submitters();
    return *submitters;
}

}

bool QueueSubmitter::Enabled() {
    return RuntimeSwitches::Set(RuntimeSwitch::AsyncSubmit);
}

QueueSubmitter& QueueSubmitter::For(VkDevice device, VkQueue queue, PFN_vkQueueSubmit submit, PFN_vkDeviceWaitIdle waitIdle) {
    auto& all = AllSubmitters();
    std::lock_guard lock(all.mutex);
    auto& entry = all.byQueue[queue];
    if (entry == nullptr) entry = new QueueSubmitter(device, queue, submit, waitIdle);
    return *entry;
}

QueueSubmitter* QueueSubmitter::Existing(VkQueue queue) {
    auto& all = AllSubmitters();
    std::lock_guard lock(all.mutex);
    const auto found = all.byQueue.find(queue);
    return found != all.byQueue.end() ? found->second : nullptr;
}

QueueSubmitter::QueueSubmitter(VkDevice device, VkQueue queue, PFN_vkQueueSubmit submit, PFN_vkDeviceWaitIdle waitIdle) :
    device(device), queue(queue), submit(submit), waitIdle(waitIdle) {
    thread = std::thread([this] {
        RaiseWorkerThreadPriority("queue submitter");
        run();
    });
    thread.detach();
}

void QueueSubmitter::RethrowFailure() {
    std::exception_ptr stored;
    {
        std::lock_guard lock(mutex);
        stored = failure;
    }
    if (stored != nullptr) std::rethrow_exception(stored);
}

void QueueSubmitter::Enqueue(Job job) {
    bool wake = false;
    {
        std::lock_guard lock(mutex);
        if (failure != nullptr) std::rethrow_exception(failure);
        jobs.push_back(std::move(job));
        wake = sleeping;
    }
    if (wake) ready.notify_one();
}

void QueueSubmitter::Drain() {
    std::unique_lock lock(mutex);
    drained.wait(lock, [&] { return jobs.empty() && !busy; });
}

std::uint64_t QueueSubmitter::Submitted() const {
    std::lock_guard lock(mutex);
    return submitted;
}

void QueueSubmitter::run() {
    for (;;) {
        Job job;
        {
            std::unique_lock lock(mutex);
            while (jobs.empty()) {
                sleeping = true;
                ready.wait(lock);
                sleeping = false;
            }
            job = std::move(jobs.front());
            jobs.pop_front();
            busy = true;
        }
        VkCommandBuffer commands = job.commands;
        std::exception_ptr encodeFailure;
        if (job.encode != nullptr) {
            if (!job.encode->Finished()) {
                const auto waitStart = std::chrono::steady_clock::now();
                job.encode->Wait();
                CommandEncoders::CountWait(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - waitStart).count()));
            }
            if (job.encode->Failed()) encodeFailure = job.encode->Error();
            else commands = job.encode->Commands();
        }
        VkSubmitInfo submission{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        if (commands != VK_NULL_HANDLE) {
            submission.commandBufferCount = 1;
            submission.pCommandBuffers = &commands;
        }
        VkTimelineSemaphoreSubmitInfoKHR timeline{VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR};
        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        if (job.wait != VK_NULL_HANDLE) {
            submission.waitSemaphoreCount = 1;
            submission.pWaitSemaphores = &job.wait;
            submission.pWaitDstStageMask = &waitStage;
            timeline.waitSemaphoreValueCount = 1;
            timeline.pWaitSemaphoreValues = &job.waitValue;
        }
        if (job.signal != VK_NULL_HANDLE) {
            submission.signalSemaphoreCount = 1;
            submission.pSignalSemaphores = &job.signal;
            timeline.signalSemaphoreValueCount = 1;
            timeline.pSignalSemaphoreValues = &job.signalValue;
        }
        if (job.wait != VK_NULL_HANDLE || job.signal != VK_NULL_HANDLE) submission.pNext = &timeline;
        VkResult result;
        {
            std::lock_guard queueLock(queueMutex);
            result = submit(queue, 1, &submission, job.fence);
        }
        if (result == VK_ERROR_DEVICE_LOST && waitIdle != nullptr) {
            const auto idle = waitIdle(device);
            if (idle != VK_SUCCESS && idle != VK_ERROR_DEVICE_LOST) std::terminate();
        }
        job.encode.reset();
        {
            std::lock_guard lock(mutex);
            busy = false;
            ++submitted;
            if (encodeFailure != nullptr && failure == nullptr) failure = encodeFailure;
            if (result != VK_SUCCESS && failure == nullptr) failure = std::make_exception_ptr(std::runtime_error("AGC graphics: vkQueueSubmit recorder (queue submitter): Vulkan result " + std::to_string(static_cast<int>(result))));
        }
        drained.notify_all();
    }
}

QueueAccess::QueueAccess(VkQueue queue) : submitter(QueueSubmitter::Existing(queue)) {
    if (submitter == nullptr) return;
    submitter->Drain();
    submitter->QueueMutex().lock();
}

QueueAccess::~QueueAccess() {
    if (submitter != nullptr) submitter->QueueMutex().unlock();
}

}

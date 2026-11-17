#include "prx/libSceAgcDriver/Graphics/include/QueueSubmitter.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadPriority.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libc/include/HostThread.hpp"
#include <cstdio>
#include <exception>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

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

std::atomic<std::size_t> liveSubmitters{0};
constexpr std::size_t MaxQueuedJobs = 64;

}

bool QueueSubmitter::Enabled() {
    return RuntimeSwitches::Set(RuntimeSwitch::AsyncSubmit);
}

QueueSubmitter& QueueSubmitter::For(VkDevice device, VkQueue queue, PFN_vkQueueSubmit submit) {
    auto& all = AllSubmitters();
    QueueSubmitter* stale = nullptr;
    QueueSubmitter* result = nullptr;
    {
        std::lock_guard lock(all.mutex);
        const auto found = all.byQueue.find(queue);
        if (found != all.byQueue.end() && found->second->device == device) return *found->second;
        std::unique_ptr<QueueSubmitter> created(new QueueSubmitter(device, queue, submit));
        if (found != all.byQueue.end()) {
            stale = found->second;
            found->second = created.get();
        } else {
            all.byQueue.emplace(queue, created.get());
            liveSubmitters.fetch_add(1, std::memory_order_release);
        }
        result = created.release();
    }
    if (stale != nullptr) stale->stop();
    return *result;
}

QueueSubmitter* QueueSubmitter::Existing(VkQueue queue) {
    if (queue == VK_NULL_HANDLE || liveSubmitters.load(std::memory_order_acquire) == 0) return nullptr;
    auto& all = AllSubmitters();
    std::lock_guard lock(all.mutex);
    const auto found = all.byQueue.find(queue);
    return found != all.byQueue.end() ? found->second : nullptr;
}

void QueueSubmitter::RetireDevice(VkDevice device) {
    auto& all = AllSubmitters();
    std::vector<QueueSubmitter*> retired;
    {
        std::lock_guard lock(all.mutex);
        for (const auto& entry : all.byQueue) {
            if (entry.second->device == device) retired.push_back(entry.second);
        }
    }
    for (auto* submitter : retired) submitter->stop();
    std::lock_guard lock(all.mutex);
    for (auto* submitter : retired) {
        const auto found = all.byQueue.find(submitter->queue);
        if (found == all.byQueue.end() || found->second != submitter) continue;
        all.byQueue.erase(found);
        liveSubmitters.fetch_sub(1, std::memory_order_release);
    }
}

std::uint64_t QueueSubmitter::FailedSubmits(VkDevice device) {
    if (liveSubmitters.load(std::memory_order_acquire) == 0) return 0;
    auto& all = AllSubmitters();
    std::lock_guard lock(all.mutex);
    std::uint64_t count = 0;
    for (const auto& entry : all.byQueue) {
        if (entry.second->device == device) count += entry.second->failed.load(std::memory_order_acquire);
    }
    return count;
}

QueueSubmitter::~QueueSubmitter() {
    stop();
}

QueueSubmitter::QueueSubmitter(VkDevice device, VkQueue queue, PFN_vkQueueSubmit submit) :
    device(device), queue(queue), submit(submit) {
    thread = Libc::StartHostThread([this] {
        try {
            RaiseWorkerThreadPriority("queue submitter");
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[gpu] queue submitter: %s\n", error.what());
        }
        run();
    });
}

void QueueSubmitter::stop() {
    {
        std::lock_guard lock(mutex);
        stopping = true;
    }
    ready.notify_one();
    if (thread.joinable()) thread.join();
}

std::uint64_t QueueSubmitter::Enqueue(const Job& job) {
    bool wake = false;
    std::uint64_t position = 0;
    {
        std::unique_lock lock(mutex);
        drained.wait(lock, [&] { return jobs.size() < MaxQueuedJobs || stopping; });
        if (stopping) throw std::runtime_error("AGC graphics: a batch was queued on the queue submitter of a retired device");
        jobs.push_back(job);
        position = ++enqueued;
        wake = sleeping;
    }
    if (wake) ready.notify_one();
    return position;
}

void QueueSubmitter::WaitSubmitReturned(std::uint64_t position) const {
    for (auto seen = returned.load(std::memory_order_acquire); seen < position; seen = returned.load(std::memory_order_acquire)) returned.wait(seen, std::memory_order_acquire);
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
        std::uint64_t position = 0;
        {
            std::unique_lock lock(mutex);
            while (jobs.empty()) {
                if (stopping) return;
                sleeping = true;
                ready.wait(lock);
                sleeping = false;
            }
            job = jobs.front();
            jobs.pop_front();
            position = ++taken;
            busy = true;
        }
        VkSubmitInfo submission{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submission.commandBufferCount = 1;
        submission.pCommandBuffers = &job.commands;
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
        if (job.result != nullptr) *job.result = result;
        if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) failed.fetch_add(1, std::memory_order_release);
        returned.store(position, std::memory_order_release);
        returned.notify_all();
        {
            std::lock_guard lock(mutex);
            busy = false;
            ++submitted;
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

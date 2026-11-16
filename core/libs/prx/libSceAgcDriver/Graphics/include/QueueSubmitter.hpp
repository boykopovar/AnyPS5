#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_QUEUESUBMITTER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_QUEUESUBMITTER_HPP

#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include <vulkan/vulkan.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <thread>

namespace AgcDriver::Graphics {

class QueueSubmitter {
public:
    struct Job {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkSemaphore wait = VK_NULL_HANDLE;
        std::uint64_t waitValue = 0;
        VkSemaphore signal = VK_NULL_HANDLE;
        std::uint64_t signalValue = 0;
        VkResult* result = nullptr;
    };

    static bool Enabled();
    static QueueSubmitter& For(VkDevice device, VkQueue queue, PFN_vkQueueSubmit submit);
    static QueueSubmitter* Existing(VkQueue queue);
    static void RetireDevice(VkDevice device);
    static std::uint64_t FailedSubmits();

    std::uint64_t Enqueue(const Job& job);
    bool Returned(std::uint64_t position) const { return returned.load(std::memory_order_acquire) >= position; }
    void WaitSubmitReturned(std::uint64_t position) const;
    void Drain();
    AgcDriver::Mutex& QueueMutex() { return queueMutex; }
    std::uint64_t Submitted() const;

private:
    QueueSubmitter(VkDevice device, VkQueue queue, PFN_vkQueueSubmit submit);
    void run();
    void stop();

    VkDevice device;
    VkQueue queue;
    PFN_vkQueueSubmit submit;
    AgcDriver::Mutex queueMutex;
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable drained;
    std::deque<Job> jobs;
    bool busy = false;
    bool sleeping = false;
    bool stopping = false;
    std::uint64_t submitted = 0;
    std::uint64_t enqueued = 0;
    std::uint64_t taken = 0;
    std::atomic<std::uint64_t> returned{0};
    std::thread thread;
};

class QueueAccess {
public:
    explicit QueueAccess(VkQueue queue);
    ~QueueAccess();
    QueueAccess(const QueueAccess&) = delete;
    QueueAccess& operator=(const QueueAccess&) = delete;

private:
    QueueSubmitter* submitter;
};

}

#endif

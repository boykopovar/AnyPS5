#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_QUEUESUBMITTER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_QUEUESUBMITTER_HPP

#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include <vulkan/vulkan.h>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
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
    };

    static bool Enabled();
    static QueueSubmitter& For(VkDevice device, VkQueue queue, PFN_vkQueueSubmit submit, PFN_vkDeviceWaitIdle waitIdle);
    static QueueSubmitter* Existing(VkQueue queue);

    void Enqueue(const Job& job);
    void Drain();
    void RethrowFailure();
    AgcDriver::Mutex& QueueMutex() { return queueMutex; }
    std::uint64_t Submitted() const;

private:
    QueueSubmitter(VkDevice device, VkQueue queue, PFN_vkQueueSubmit submit, PFN_vkDeviceWaitIdle waitIdle);
    void run();

    VkDevice device;
    VkQueue queue;
    PFN_vkQueueSubmit submit;
    PFN_vkDeviceWaitIdle waitIdle;
    AgcDriver::Mutex queueMutex;
    mutable std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable drained;
    std::deque<Job> jobs;
    bool busy = false;
    bool sleeping = false;
    std::uint64_t submitted = 0;
    std::exception_ptr failure;
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

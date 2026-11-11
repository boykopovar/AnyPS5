#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RECORDINGPOOL_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RECORDINGPOOL_HPP

#include "prx/libSceAgcDriver/Graphics/include/OrderedAssembly.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace AgcDriver::Graphics {

class RecordingPool {
public:
    struct Inheritance {
        VkFormat depthFormat = VK_FORMAT_UNDEFINED;
        VkFormat stencilFormat = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        std::uint32_t colorAttachments = 0;
    };
    struct Result {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        std::shared_ptr<void> release;
        std::exception_ptr error;
    };

    explicit RecordingPool(const Context& context);
    ~RecordingPool() { Stop(); }
    RecordingPool(const RecordingPool&) = delete;
    RecordingPool& operator=(const RecordingPool&) = delete;

    void Start(std::uint32_t threads);
    void Stop();

    void Record(OrderedAssembly<Result>& assembly, std::uint64_t sequence, const Inheritance& inheritance, std::function<void(VkCommandBuffer)> record);
    static std::uint64_t RecordedBuffers();

private:
    struct Task {
        OrderedAssembly<Result>* assembly = nullptr;
        std::uint64_t sequence = 0;
        Inheritance inheritance;
        std::function<void(VkCommandBuffer)> record;
    };
    struct Returned;
    struct Release;
    void run(VkCommandPool pool, const std::shared_ptr<Returned>& returned);
    VkCommandBuffer acquire(VkCommandPool pool, Returned& returned) const;

    VkDevice device = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    PFN_vkCreateCommandPool createPool = nullptr;
    PFN_vkDestroyCommandPool destroyPool = nullptr;
    PFN_vkAllocateCommandBuffers allocateBuffers = nullptr;
    PFN_vkResetCommandBuffer resetBuffer = nullptr;
    PFN_vkBeginCommandBuffer beginBuffer = nullptr;
    PFN_vkEndCommandBuffer endBuffer = nullptr;
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<Task> tasks;
    std::vector<std::thread> workers;
    std::vector<VkCommandPool> pools;
    bool closing = false;
};

RecordingPool& DeviceRecordingPool(const Context& context);

}

#endif

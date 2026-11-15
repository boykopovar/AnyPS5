#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "VulkanTestDevice.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;

bool WaitIdleWaitsFor(AgcDriver::VulkanDevice& device, std::mutex& queueMutex) {
    std::unique_lock held(queueMutex);
    std::atomic<bool> done{false};
    std::thread waiter([&] {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        device.WaitIdle();
        done.store(true, std::memory_order_release);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const bool waited = !done.load(std::memory_order_acquire);
    held.unlock();
    waiter.join();
    Require(done.load(std::memory_order_acquire), "WaitIdle did not finish once the queue was released");
    return waited;
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        std::size_t checked = 0;
        if (context.settleQueue != VK_NULL_HANDLE && context.settleQueueMutex != nullptr) {
            Require(WaitIdleWaitsFor(*device, *context.settleQueueMutex), "vkDeviceWaitIdle ran while another thread held the settle queue");
            ++checked;
        }
        if (context.sparseQueue != VK_NULL_HANDLE && context.sparseQueueMutex != nullptr) {
            Require(WaitIdleWaitsFor(*device, *context.sparseQueueMutex), "vkDeviceWaitIdle ran while another thread held the sparse queue");
            ++checked;
        }
        if (checked == 0) {
            std::cout << "the device has no settle or sparse queue; nothing to check\n";
            return VulkanTestSkipped;
        }
        std::cout << "wait idle queue lock tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

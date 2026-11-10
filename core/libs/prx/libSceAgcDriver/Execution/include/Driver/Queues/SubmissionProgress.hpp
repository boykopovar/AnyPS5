#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_SUBMISSIONPROGRESS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_SUBMISSIONPROGRESS_HPP

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <utility>

namespace AgcDriver { class VulkanDevice; }

namespace AgcDriver::DriverDetail {

class SubmissionProgress {
public:
    using Clock = std::chrono::steady_clock;
    using Ready = std::pair<std::shared_ptr<VulkanDevice>, std::uint64_t>;

    void Publish(std::shared_ptr<VulkanDevice> device, std::uint64_t epoch) {
        {
            std::lock_guard lock(mutex);
            result = {std::move(device), epoch};
            ready = true;
        }
        changed.notify_one();
    }

    Ready WaitUntil(Clock::time_point deadline) {
        std::unique_lock lock(mutex);
        if (!changed.wait_until(lock, deadline, [&] { return ready; })) return {};
        return result;
    }

private:
    std::mutex mutex;
    std::condition_variable changed;
    Ready result;
    bool ready = false;
};

}

#endif

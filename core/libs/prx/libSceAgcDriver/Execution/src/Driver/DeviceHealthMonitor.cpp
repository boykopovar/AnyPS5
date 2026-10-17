#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace Sce::AgcDriver {

enum class StabilityState : uint32_t {
    Nominal = 0,
    ResourcePressure,
    HangDetected,
    DeviceLostRecovered
};

struct DeviceHealthMetrics {
    std::atomic<uint64_t> timeoutCount{0};
    std::atomic<uint64_t> memoryFallbackCount{0};
    std::atomic<uint64_t> nullPointerTraps{0};
    std::atomic<StabilityState> currentState{StabilityState::Nominal};
};

class DeviceHealthMonitor {
public:
    static DeviceHealthMonitor& Instance() {
        static DeviceHealthMonitor instance;
        return instance;
    }

    void RecordTimeout(std::string_view context) {
        metrics.timeoutCount.fetch_add(1, std::memory_order_relaxed);
        metrics.currentState.store(StabilityState::HangDetected, std::memory_order_release);
    }

    void RecordMemoryFallback() {
        metrics.memoryFallbackCount.fetch_add(1, std::memory_order_relaxed);
        if (metrics.currentState.load(std::memory_order_acquire) == StabilityState::Nominal) {
            metrics.currentState.store(StabilityState::ResourcePressure, std::memory_order_release);
        }
    }

    void RecordNullPointerTrap() {
        metrics.nullPointerTraps.fetch_add(1, std::memory_order_relaxed);
    }

    StabilityState GetState() const {
        return metrics.currentState.load(std::memory_order_acquire);
    }

    bool IsStable() const {
        return metrics.currentState.load(std::memory_order_acquire) == StabilityState::Nominal;
    }

    void ResetHealth() {
        metrics.currentState.store(StabilityState::Nominal, std::memory_order_release);
    }

private:
    DeviceHealthMonitor() = default;
    ~DeviceHealthMonitor() = default;
    DeviceHealthMonitor(const DeviceHealthMonitor&) = delete;
    DeviceHealthMonitor& operator=(const DeviceHealthMonitor&) = delete;

    DeviceHealthMetrics metrics;
};

} // namespace Sce::AgcDriver

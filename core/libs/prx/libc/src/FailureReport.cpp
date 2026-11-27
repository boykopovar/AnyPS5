#include "prx/libc/include/FailureReport.hpp"
#include <array>
#include <atomic>
#include <stdexcept>

namespace {

using Report = void (*)() noexcept;

std::array<std::atomic<Report>, 8> reports{};
std::atomic<bool> ran{false};
std::atomic_flag registryLock{};

class RegistryLockGuard final {
public:
    RegistryLockGuard() noexcept {
        while (registryLock.test_and_set(std::memory_order_acquire)) {}
    }

    ~RegistryLockGuard() noexcept {
        registryLock.clear(std::memory_order_release);
    }

    RegistryLockGuard(const RegistryLockGuard&) = delete;
    RegistryLockGuard& operator=(const RegistryLockGuard&) = delete;
};

}

extern "C" void LibcRegisterFailureReport_nid_no_patch(Report report) {
    if (report == nullptr) throw std::invalid_argument("libc: null failure report");
    const RegistryLockGuard guard;
    for (auto& slot : reports) {
        if (slot.load(std::memory_order_relaxed) == report) return;
    }
    for (auto& slot : reports) {
        if (slot.load(std::memory_order_relaxed) == nullptr) {
            slot.store(report, std::memory_order_release);
            return;
        }
    }
    throw std::runtime_error("libc: more than 8 failure reports registered");
}

extern "C" void LibcUnregisterFailureReport_nid_no_patch(Report report) noexcept {
    if (report == nullptr) return;
    const RegistryLockGuard guard;
    for (auto& slot : reports) {
        if (slot.load(std::memory_order_relaxed) == report) {
            slot.store(nullptr, std::memory_order_release);
        }
    }
}

extern "C" void LibcRunFailureReports_nid_no_patch() noexcept {
    if (ran.exchange(true, std::memory_order_acq_rel)) return;
    for (auto& slot : reports) {
        if (const auto report = slot.load(std::memory_order_acquire)) report();
    }
}

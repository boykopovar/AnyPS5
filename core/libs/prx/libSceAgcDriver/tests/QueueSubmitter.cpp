#include "prx/libSceAgcDriver/Graphics/include/QueueSubmitter.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using AgcDriver::Graphics::QueueAccess;
using AgcDriver::Graphics::QueueSubmitter;

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Handle>
Handle Fake(std::uintptr_t value) {
    return reinterpret_cast<Handle>(value);
}

std::mutex submitsMutex;
std::vector<std::uintptr_t> firstSubmits;
std::atomic<int> secondSubmits{0};
std::atomic<int> slowSubmits{0};
std::atomic<bool> slowInside{false};

VKAPI_ATTR VkResult VKAPI_CALL FirstSubmit(VkQueue, std::uint32_t count, const VkSubmitInfo* infos, VkFence) {
    std::lock_guard lock(submitsMutex);
    for (std::uint32_t index = 0; index < count; ++index) firstSubmits.push_back(reinterpret_cast<std::uintptr_t>(infos[index].pCommandBuffers[0]));
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL SecondSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) {
    secondSubmits.fetch_add(1, std::memory_order_relaxed);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL SlowSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) {
    slowInside.store(true, std::memory_order_release);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    slowSubmits.fetch_add(1, std::memory_order_acq_rel);
    slowInside.store(false, std::memory_order_release);
    return VK_SUCCESS;
}

std::atomic<int> failingCalls{0};

VKAPI_ATTR VkResult VKAPI_CALL FailingSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) {
    failingCalls.fetch_add(1, std::memory_order_relaxed);
    return VK_ERROR_OUT_OF_DEVICE_MEMORY;
}

QueueSubmitter::Job JobFor(std::uintptr_t commands, VkResult* result = nullptr, std::uint64_t serial = 0) {
    return {Fake<VkCommandBuffer>(commands), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, serial != 0 ? Fake<VkSemaphore>(0x7700) : VK_NULL_HANDLE, serial, result};
}

void ordered() {
    const auto device = Fake<VkDevice>(0x1000);
    const auto queue = Fake<VkQueue>(0x2000);
    auto& submitter = QueueSubmitter::For(device, queue, FirstSubmit);
    Require(QueueSubmitter::Existing(queue) == &submitter, "a queue does not find its submitter");
    Require(&QueueSubmitter::For(device, queue, FirstSubmit) == &submitter, "a second request for the same device and queue made another submitter");
    std::uint64_t last = 0;
    for (std::uintptr_t commands = 1; commands <= 64; ++commands) last = submitter.Enqueue(JobFor(commands));
    Require(last == 64, "the submit positions do not count the queued jobs");
    submitter.WaitSubmitReturned(last);
    Require(submitter.Returned(last), "a waited position does not read as returned");
    {
        std::lock_guard lock(submitsMutex);
        Require(firstSubmits.size() == 64, "not every queued job was submitted");
        for (std::size_t index = 0; index < firstSubmits.size(); ++index) Require(firstSubmits[index] == index + 1, "jobs were submitted out of order");
    }
    submitter.Drain();
    Require(submitter.Submitted() == 64, "the submitter does not count its submits");
    QueueSubmitter::RetireDevice(device);
}

void retiredWithItsDevice() {
    const auto first = Fake<VkDevice>(0x3000);
    const auto second = Fake<VkDevice>(0x4000);
    const auto reused = Fake<VkQueue>(0x5000);
    const auto other = Fake<VkQueue>(0x6000);
    {
        std::lock_guard lock(submitsMutex);
        firstSubmits.clear();
    }
    auto& old = QueueSubmitter::For(first, reused, FirstSubmit);
    auto& kept = QueueSubmitter::For(second, other, SecondSubmit);
    old.WaitSubmitReturned(old.Enqueue(JobFor(7)));
    QueueSubmitter::RetireDevice(first);
    Require(QueueSubmitter::Existing(reused) == nullptr, "a submitter outlived its device");
    Require(QueueSubmitter::Existing(other) == &kept, "retiring one device removed another device's submitter");
    bool refused = false;
    try {
        old.Enqueue(JobFor(9));
    } catch (const std::runtime_error&) {
        refused = true;
    }
    Require(refused, "a retired submitter accepted a job");
    auto& fresh = QueueSubmitter::For(second, reused, SecondSubmit);
    Require(&fresh != &old, "a reused queue handle found the submitter of a destroyed device");
    fresh.WaitSubmitReturned(fresh.Enqueue(JobFor(8)));
    Require(secondSubmits.load(std::memory_order_relaxed) == 1, "a reused queue handle did not submit through its own device's function");
    {
        std::lock_guard lock(submitsMutex);
        Require(firstSubmits.size() == 1 && firstSubmits[0] == 7, "a destroyed device's submit function was called again");
    }
    QueueSubmitter::RetireDevice(second);
    Require(QueueSubmitter::Existing(reused) == nullptr && QueueSubmitter::Existing(other) == nullptr, "a submitter outlived its device");
    QueueSubmitter::RetireDevice(second);
}

void staleDeviceReplaced() {
    const auto gone = Fake<VkDevice>(0x7000);
    const auto next = Fake<VkDevice>(0x8000);
    const auto queue = Fake<VkQueue>(0x9000);
    auto& stale = QueueSubmitter::For(gone, queue, FirstSubmit);
    auto& replacement = QueueSubmitter::For(next, queue, SecondSubmit);
    Require(&replacement != &stale, "a queue handle of another device kept the old submitter");
    Require(QueueSubmitter::Existing(queue) == &replacement, "the replacement submitter is not registered");
    const int before = secondSubmits.load(std::memory_order_relaxed);
    replacement.WaitSubmitReturned(replacement.Enqueue(JobFor(10)));
    Require(secondSubmits.load(std::memory_order_relaxed) == before + 1, "the replacement did not submit through its own function");
    QueueSubmitter::RetireDevice(next);
    QueueSubmitter::RetireDevice(gone);
    Require(QueueSubmitter::Existing(queue) == nullptr, "a submitter outlived its device");
}

void drainedBeforeAccessAndRetirement() {
    const auto device = Fake<VkDevice>(0xa000);
    const auto queue = Fake<VkQueue>(0xb000);
    auto& submitter = QueueSubmitter::For(device, queue, SlowSubmit);
    for (std::uintptr_t commands = 1; commands <= 8; ++commands) submitter.Enqueue(JobFor(commands));
    {
        const QueueAccess access(queue);
        Require(slowSubmits.load(std::memory_order_acquire) == 8, "queue access did not drain the queued submits");
        Require(!slowInside.load(std::memory_order_acquire), "queue access overlapped a submit");
    }
    for (std::uintptr_t commands = 9; commands <= 16; ++commands) submitter.Enqueue(JobFor(commands));
    QueueSubmitter::RetireDevice(device);
    Require(slowSubmits.load(std::memory_order_acquire) == 16, "retiring a device dropped its queued submits");
}

void failureReachesItsJob() {
    const auto device = Fake<VkDevice>(0xc000);
    const auto queue = Fake<VkQueue>(0xd000);
    auto& submitter = QueueSubmitter::For(device, queue, FailingSubmit);
    Require(!QueueSubmitter::FailedThrough(device, 100), "a device without a failed submit reports one");
    VkResult result = VK_SUCCESS;
    submitter.WaitSubmitReturned(submitter.Enqueue(JobFor(1, &result, 5)));
    Require(result == VK_ERROR_OUT_OF_DEVICE_MEMORY, "a failed submit did not reach its job");
    Require(QueueSubmitter::FailedSubmits(device) == 1, "a failed submit was not counted for its device");
    Require(QueueSubmitter::FailedSubmits(Fake<VkDevice>(0xe000)) == 0, "a failed submit was counted for another device");
    Require(QueueSubmitter::FailedThrough(device, 5) && QueueSubmitter::FailedThrough(device, 6), "a wait at or past a failed serial does not see the failure");
    Require(!QueueSubmitter::FailedThrough(device, 4), "a wait before a failed serial sees the failure");
    VkResult next = VK_ERROR_UNKNOWN;
    submitter.WaitSubmitReturned(submitter.Enqueue(JobFor(2, &next, 6)));
    Require(next == VK_ERROR_OUT_OF_DEVICE_MEMORY, "a job after a failed submit did not take the failure");
    Require(failingCalls.load(std::memory_order_relaxed) == 1, "the submitter kept submitting after a failed submit");
    QueueSubmitter::RetireDevice(device);
    Require(QueueSubmitter::FailedSubmits(device) == 0, "a retired device still counts failed submits");
}

}

int main() {
    try {
        Require(QueueSubmitter::Existing(Fake<VkQueue>(0x2000)) == nullptr, "a queue without a submitter found one");
        ordered();
        retiredWithItsDevice();
        staleDeviceReplaced();
        drainedBeforeAccessAndRetirement();
        failureReachesItsJob();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    std::printf("queue submitter: 5 cases passed\n");
    return 0;
}

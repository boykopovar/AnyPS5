#include "prx/libSceAgcDriver/Graphics/include/QueueSubmitter.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

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

VKAPI_ATTR VkResult VKAPI_CALL FirstSubmit(VkQueue, std::uint32_t count, const VkSubmitInfo* infos, VkFence) {
    std::lock_guard lock(submitsMutex);
    for (std::uint32_t index = 0; index < count; ++index) firstSubmits.push_back(reinterpret_cast<std::uintptr_t>(infos[index].pCommandBuffers[0]));
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL SecondSubmit(VkQueue, std::uint32_t, const VkSubmitInfo*, VkFence) {
    secondSubmits.fetch_add(1, std::memory_order_relaxed);
    return VK_SUCCESS;
}

QueueSubmitter::Job JobFor(std::uintptr_t commands) {
    return {Fake<VkCommandBuffer>(commands), VK_NULL_HANDLE, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 0};
}

void ordered() {
    const auto device = Fake<VkDevice>(0x1000);
    const auto queue = Fake<VkQueue>(0x2000);
    auto& submitter = QueueSubmitter::For(device, queue, FirstSubmit, nullptr);
    Require(QueueSubmitter::Existing(queue) == &submitter, "a queue does not find its submitter");
    std::uint64_t last = 0;
    for (std::uintptr_t commands = 1; commands <= 64; ++commands) last = submitter.Enqueue(JobFor(commands));
    Require(last == 64, "the submit positions do not count the queued jobs");
    submitter.WaitSubmitReturned(last);
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
    auto& old = QueueSubmitter::For(first, reused, FirstSubmit, nullptr);
    auto& kept = QueueSubmitter::For(second, other, SecondSubmit, nullptr);
    old.WaitSubmitReturned(old.Enqueue(JobFor(7)));
    QueueSubmitter::RetireDevice(first);
    Require(QueueSubmitter::Existing(reused) == nullptr, "a submitter outlived its device");
    Require(QueueSubmitter::Existing(other) == &kept, "retiring one device removed another device's submitter");
    auto& fresh = QueueSubmitter::For(second, reused, SecondSubmit, nullptr);
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

}

int main() {
    try {
        ordered();
        retiredWithItsDevice();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    std::printf("queue submitter: 2 cases passed\n");
    return 0;
}

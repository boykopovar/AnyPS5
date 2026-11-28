#include "prx/libSceAgcDriver/Graphics/include/OrderedAssembly.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RecordingPool.hpp"
#include "AllocationFailure.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>
#include <thread>

using namespace AgcDriver::Graphics;
using AgcDriver::Tests::SetAllocationFailureCountdown;

namespace {

std::atomic<std::uintptr_t> nextHandle{0x1000};
std::atomic<bool> failAfterReset{false};
std::atomic<unsigned> gatedBegins{0};
std::atomic<unsigned> arrivedBegins{0};

VKAPI_ATTR VkResult VKAPI_CALL createPool(VkDevice, const VkCommandPoolCreateInfo*, const VkAllocationCallbacks*, VkCommandPool* pool) {
    *pool = reinterpret_cast<VkCommandPool>(nextHandle.fetch_add(16));
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL destroyPool(VkDevice, VkCommandPool, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL allocateBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer* commands) {
    *commands = reinterpret_cast<VkCommandBuffer>(nextHandle.fetch_add(16));
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL resetBuffer(VkCommandBuffer, VkCommandBufferResetFlags) {
    if (failAfterReset.exchange(false)) SetAllocationFailureCountdown(0);
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL beginBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) {
    const auto gate = gatedBegins.load();
    if (gate == 0) return VK_SUCCESS;
    arrivedBegins.fetch_add(1);
    while (arrivedBegins.load() < gate) std::this_thread::yield();
    return VK_SUCCESS;
}
VKAPI_ATTR VkResult VKAPI_CALL endBuffer(VkCommandBuffer) { return VK_SUCCESS; }

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL deviceProc(VkDevice, const char* name) {
    const std::string_view function(name);
    if (function == "vkCreateCommandPool") return reinterpret_cast<PFN_vkVoidFunction>(createPool);
    if (function == "vkDestroyCommandPool") return reinterpret_cast<PFN_vkVoidFunction>(destroyPool);
    if (function == "vkAllocateCommandBuffers") return reinterpret_cast<PFN_vkVoidFunction>(allocateBuffers);
    if (function == "vkResetCommandBuffer") return reinterpret_cast<PFN_vkVoidFunction>(resetBuffer);
    if (function == "vkBeginCommandBuffer") return reinterpret_cast<PFN_vkVoidFunction>(beginBuffer);
    if (function == "vkEndCommandBuffer") return reinterpret_cast<PFN_vkVoidFunction>(endBuffer);
    return nullptr;
}

struct RecordFailure {};

int failures = 0;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    ++failures;
}

RecordingPool::Result Run(RecordingPool& pool, std::function<void(VkCommandBuffer)> record) {
    OrderedAssembly<RecordingPool::Result> assembly;
    const auto sequence = assembly.Reserve();
    pool.Record(assembly, sequence, {}, std::move(record));
    return assembly.Take(sequence);
}

void ClearWorkerCountdown(RecordingPool& pool) {
    const auto result = Run(pool, [](VkCommandBuffer) { SetAllocationFailureCountdown(-1); });
    Expect(result.error == nullptr && result.release != nullptr, "the clearing recording failed");
}

}

int main(int argc, char** argv) {
    const std::string_view mode = argc > 1 ? argv[1] : "";
    Context context{};
    context.device = reinterpret_cast<VkDevice>(nextHandle.fetch_add(16));
    context.deviceProc = deviceProc;
    RecordingPool pool(context);
    pool.Start(1);

    if (mode == "--publish") {
        auto result = Run(pool, [](VkCommandBuffer) { SetAllocationFailureCountdown(1); });
        Expect(result.error == nullptr, "publishing a recorded secondary failed");
        Expect(result.commands != VK_NULL_HANDLE && result.release != nullptr, "a published secondary lost its buffer or release");
        result.release.reset();
        ClearWorkerCountdown(pool);
    } else if (mode == "--release") {
        auto result = Run(pool, [](VkCommandBuffer) {});
        Expect(result.error == nullptr && result.release != nullptr, "the recording failed");
        SetAllocationFailureCountdown(0);
        result.release.reset();
        SetAllocationFailureCountdown(-1);
        const auto reused = Run(pool, [](VkCommandBuffer) {});
        Expect(reused.commands == result.commands, "a released secondary was not returned to its worker");
    } else if (mode == "--failed-record") {
        failAfterReset = true;
        const auto result = Run(pool, [](VkCommandBuffer) { throw RecordFailure{}; });
        Expect(result.error != nullptr && result.commands == VK_NULL_HANDLE, "a failed recording was not reported");
        Expect(!failAfterReset, "a failed recording did not reset its buffer");
        ClearWorkerCountdown(pool);
    } else if (mode == "--injected-failures") {
        constexpr unsigned Workers = 4;
        RecordingPool racing(context);
        racing.Start(Workers);
        bool exact = true;
        for (unsigned round = 0; round < 2000 && exact; ++round) {
            arrivedBegins = 0;
            gatedBegins = Workers;
            racing.FailNextRecordingsForTest(1);
            OrderedAssembly<RecordingPool::Result> assembly;
            std::array<std::uint64_t, Workers> sequences{};
            for (auto& sequence : sequences) {
                sequence = assembly.Reserve();
                racing.Record(assembly, sequence, {}, [](VkCommandBuffer) {});
            }
            unsigned failed = 0;
            for (const auto sequence : sequences) failed += assembly.Take(sequence).error != nullptr ? 1u : 0u;
            gatedBegins = 0;
            exact = failed == 1 && Run(racing, [](VkCommandBuffer) {}).error == nullptr;
        }
        Expect(exact, "one injected recording failure did not fail exactly one recording");
        racing.FailNextRecordingsForTest(1);
        Expect(Run(pool, [](VkCommandBuffer) {}).error == nullptr, "an injected recording failure reached another pool");
        Expect(Run(racing, [](VkCommandBuffer) {}).error != nullptr, "an injected recording failure was lost");
        racing.Stop();
    } else {
        std::fprintf(stderr, "FAIL: unknown mode\n");
        return 2;
    }
    pool.Stop();
    if (failures != 0) return 1;
    std::puts("recording pool allocation tests passed");
    return 0;
}

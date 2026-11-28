#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "EncoderReplayGate.hpp"
#include "VulkanTestDevice.hpp"
#ifdef APS5_AGC_ALLOCATION_COUNT
#include "prx/libSceAgcDriver/tests/AllocationCounter.hpp"
#else
#include "prx/libSceAgcDriver/tests/AllocationFailure.hpp"
#endif
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;

#if defined(__GLIBC__) || defined(APS5_AGC_ALLOCATION_COUNT)
constexpr bool DriverAllocationsCanFail = true;
#else
constexpr bool DriverAllocationsCanFail = false;
#endif

void FailDriverAllocations() {
#ifdef APS5_AGC_ALLOCATION_COUNT
    FailAllocationsAfter_nid_no_patch(0);
#else
    AgcDriver::Tests::SetAllocationFailureCountdown(0);
#endif
}

void StopDriverAllocationFailures() {
#ifdef APS5_AGC_ALLOCATION_COUNT
    StopAllocationFailures_nid_no_patch();
#else
    AgcDriver::Tests::SetAllocationFailureCountdown(-1);
#endif
}

bool KeepFails(Recorder& recorder) {
    auto probe = std::make_shared<int>(0);
    FailDriverAllocations();
    try {
        recorder.Keep(std::move(probe));
    } catch (const std::bad_alloc&) {
        StopDriverAllocationFailures();
        return true;
    }
    StopDriverAllocationFailures();
    return false;
}

void SampleOrderOwnersOutliveAFailedRetention(const Context& context, Recorder& recorder) {
    std::vector<std::shared_ptr<void>> owners;
    std::vector<std::weak_ptr<void>> watched;
    for (std::size_t index = 0; index < 4; ++index) {
        auto owner = std::make_shared<std::size_t>(index);
        watched.push_back(owner);
        owners.push_back(std::move(owner));
    }
    bool threw = false;
    bool observed = false;
    std::atomic<bool> aliveAtReplay{false};
    const auto commands = recorder.Commands();
    {
        EncoderReplayGate gate([&] { aliveAtReplay.store(std::ranges::none_of(watched, [](const std::weak_ptr<void>& owner) { return owner.expired(); }), std::memory_order_release); });
        try {
            Recorder::EncodingWindow window(recorder);
            Require(window.Encoded(), "the sample-order window was not encoded");
            const auto unwindGuard = recorder.UnwindGuard();
            const VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT};
            context.functions->cmdPipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            std::size_t kept = 0;
            while (!KeepFails(recorder)) Require(++kept < (std::size_t{1} << 20), "an injected allocation failure never reached the recorder's kept list");
            FailDriverAllocations();
            try {
                KeepSampleOrderOwners(recorder, owners);
            } catch (...) {
                StopDriverAllocationFailures();
                throw;
            }
            StopDriverAllocationFailures();
        } catch (const std::bad_alloc&) {
            threw = true;
        }
        observed = gate.Observed();
    }
    Require(threw, "retaining sample-order owners into a full kept list did not reach the injected allocation failure");
    Require(observed && aliveAtReplay.load(std::memory_order_acquire), "a sample-order owner was released by a failed retention before the encoder replayed the commands that use it");
}

}

int main() {
    try {
        if (!DriverAllocationsCanFail) {
            std::printf("skipped, this build cannot fail allocations inside the driver\n");
            return VulkanTestSkipped;
        }
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        auto* recorder = Recorder::Active();
        Require(recorder != nullptr, "the device has no active recorder");
        if (!recorder->Encodes()) {
            std::printf("skipped, the command encoder is off\n");
            return VulkanTestSkipped;
        }
        Require(context.functions != nullptr && context.functions->cmdPipelineBarrier != nullptr, "the encoded device table is unavailable");
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        SampleOrderOwnersOutliveAFailedRetention(context, *recorder);
        recorder->Sync();
        std::cout << "sample-order retention test passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

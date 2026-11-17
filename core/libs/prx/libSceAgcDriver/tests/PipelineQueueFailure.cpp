#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "AllocationFailure.hpp"
#include "Testing.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <new>
#include <string_view>
#include <thread>
#include <utility>

using namespace AgcDriver::Graphics;
using AgcDriver::Tests::SetAllocationFailureCountdown;
using namespace std::chrono_literals;

namespace {

void Schedule(VkDevice device, std::function<void()> work) {
    OptimizedPipeline pipeline;
    pipeline.device = device;
    pipeline.relink = std::move(work);
    pipeline.deferred = true;
    pipeline.Schedule();
}

}

int main(int argc, char** argv) {
    Require(argc == 2, "expected wait, clear or stop");
    const std::string_view mode(argv[1]);
    Require(mode == "wait" || mode == "clear" || mode == "stop", "unknown drain mode");
    const auto device = reinterpret_cast<VkDevice>(std::uintptr_t{1});
    const auto otherDevice = reinterpret_cast<VkDevice>(std::uintptr_t{2});
    std::atomic<unsigned> completed{0};
    Schedule(device, [&] { ++completed; });
    WaitForOptimizedPipelines(device);
    Require(completed == 1, "the initial job did not run");

    bool allocationFailed = false;
    OptimizedPipeline fresh;
    fresh.device = otherDevice;
    fresh.relink = [&] { ++completed; };
    fresh.deferred = true;
    SetAllocationFailureCountdown(0);
    try { fresh.Schedule(); } catch (const std::bad_alloc&) { allocationFailed = true; }
    SetAllocationFailureCountdown(-1);
    Require(allocationFailed, "the new device allocation did not fail");
    WaitForOptimizedPipelines(otherDevice);
    Require(completed == 1, "a rejected job ran");

    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
    Schedule(device, [&] {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
    });
    {
        std::unique_lock lock(mutex);
        Require(changed.wait_for(lock, 3s, [&] { return entered; }), "the worker did not start");
    }

    allocationFailed = false;
    unsigned accepted = 0;
    for (unsigned attempt = 0; attempt < 512 && !allocationFailed; ++attempt) {
        OptimizedPipeline pipeline;
        pipeline.device = device;
        pipeline.relink = [&] { ++completed; };
        pipeline.deferred = true;
        SetAllocationFailureCountdown(0);
        try {
            pipeline.Schedule();
            ++accepted;
        } catch (const std::bad_alloc&) {
            allocationFailed = true;
        }
        SetAllocationFailureCountdown(-1);
    }
    Require(allocationFailed, "the queued jobs never needed another allocation");
    Require(completed == 1, "queued jobs ran while the worker was held");
    {
        std::lock_guard lock(mutex);
        released = true;
    }
    changed.notify_all();

    bool drained = false;
    std::thread drain([&] {
        if (mode == "stop") StopPipelineLibraryWorker();
        if (mode == "clear") ClearPipelineLibraries(device);
        else WaitForOptimizedPipelines(device);
        {
            std::lock_guard lock(mutex);
            drained = true;
        }
        changed.notify_all();
    });
    {
        std::unique_lock lock(mutex);
        Require(changed.wait_for(lock, 3s, [&] { return drained; }), "a failed queue allocation left an unfinished job");
    }
    drain.join();
    if (mode != "stop") {
        Require(completed == accepted + 1, "an accepted job was lost or ran twice");
        Schedule(device, [&] { ++completed; });
        Schedule(otherDevice, [&] { ++completed; });
        WaitForOptimizedPipelines(device);
        WaitForOptimizedPipelines(otherDevice);
        Require(completed == accepted + 3, "the queue did not recover after allocation failure");
        StopPipelineLibraryWorker();
    }
    WaitForOptimizedPipelines(device);
    ClearPipelineLibraries(device);
    ClearPipelineLibraries(otherDevice);
    Require(PipelineLibraryWorkers() == 0, "the pipeline worker did not stop");
    std::printf("%.*s: rejected new-device and queue-growth allocations; %u jobs accepted before rejection, queue drained\n",
        static_cast<int>(mode.size()), mode.data(), accepted);
    return 0;
}

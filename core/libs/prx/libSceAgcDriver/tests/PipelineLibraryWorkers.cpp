#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <utility>

using namespace AgcDriver::Graphics;
using namespace std::chrono_literals;

namespace {

void Check(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::fflush(stderr);
    std::_Exit(1);
}

void Schedule(VkDevice device, std::function<void()> work) {
    OptimizedPipeline pipeline;
    pipeline.device = device;
    pipeline.relink = std::move(work);
    pipeline.deferred = true;
    pipeline.Schedule();
}

}

int main(int argc, char** argv) {
    const auto device = reinterpret_cast<VkDevice>(std::uintptr_t{1});
    if (argc == 2 && std::string_view(argv[1]) == "invalid") {
        bool rejected = false;
        try {
            Schedule(device, [] {});
        } catch (const std::runtime_error& error) {
            rejected = std::string_view(error.what()).find("APS5_PIPELINE_LTO_THREADS") != std::string_view::npos;
        }
        Check(rejected, "an invalid worker count was accepted");
        Check(PipelineLibraryWorkers() == 0, "a worker started for an invalid worker count");
        std::printf("invalid worker count rejected before any worker started\n");
        return 0;
    }
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool released = false;
    std::atomic<unsigned> completed{0};
    if (argc == 2 && std::string_view(argv[1]) == "burst") {
        Schedule(device, [&] { ++completed; });
        WaitForOptimizedPipelines(device);
        Check(completed == 1 && PipelineLibraryWorkers() == 1, "the first job did not run on one worker");
        Schedule(device, [&] {
            std::unique_lock lock(mutex);
            changed.wait(lock, [&] { return released; });
            ++completed;
        });
        Schedule(device, [&] {
            std::lock_guard lock(mutex);
            ++completed;
            changed.notify_all();
        });
        {
            std::unique_lock lock(mutex);
            Check(changed.wait_for(lock, 3s, [&] { return completed == 2; }), "a burst queued behind the idle worker it woke with a second worker allowed");
            released = true;
        }
        changed.notify_all();
        WaitForOptimizedPipelines(device);
        Check(completed == 3 && PipelineLibraryWorkers() == 2, "the burst did not run once each on two workers");
        StopPipelineLibraryWorker();
        ClearPipelineLibraries(device);
        Check(PipelineLibraryWorkers() == 0, "the pipeline workers did not stop");
        std::printf("two jobs queued at once behind one idle worker ran on two workers\n");
        return 0;
    }
    Check(argc == 1, "expected no argument, invalid or burst");
    Schedule(device, [&] {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
        ++completed;
    });
    {
        std::unique_lock lock(mutex);
        Check(changed.wait_for(lock, 3s, [&] { return entered; }), "the first worker did not start");
    }

    Schedule(device, [&] {
        ++completed;
        changed.notify_all();
    });
    {
        std::unique_lock lock(mutex);
        Check(changed.wait_for(lock, 3s, [&] { return completed == 1; }), "a job waited behind a held job with a second worker allowed");
    }
    Check(PipelineLibraryWorkers() == 2, "the queue did not start a second worker");

    Schedule(device, [&] { ++completed; });
    {
        std::lock_guard lock(mutex);
        released = true;
    }
    changed.notify_all();
    WaitForOptimizedPipelines(device);
    Check(completed == 3, "a job was lost or ran twice");
    Check(PipelineLibraryWorkers() == 2, "the queue started more workers than allowed");

    StopPipelineLibraryWorker();
    ClearPipelineLibraries(device);
    Check(PipelineLibraryWorkers() == 0, "the pipeline workers did not stop");
    std::printf("two workers: a job finished while another was held, three jobs ran once each, both workers stopped\n");
    return 0;
}

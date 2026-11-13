#include "VulkanTestDevice.hpp"
#include "LaneGate.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("lane open serial: " + message);
}

using Gate = AgcDriver::Tests::TimelineGate;

}

int main() {
    using AgcDriver::Graphics::Recorder;
#ifdef _WIN32
    _putenv_s("APS5_ASYNC_LANE", "51");
#else
    setenv("APS5_ASYNC_LANE", "51", 1);
#endif
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        {
            std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
            auto* recorder = Recorder::Active();
            Require(recorder != nullptr, "no active recorder");
            if (!recorder->AsyncLaneActive()) {
                std::puts("skipped: the device has no eligible compute-only queue for the async lane");
                lock.unlock();
                device.reset();
                AgcDriverShutdown_nid_postfix();
                return VulkanTestSkipped;
            }
            recorder->Sync();
            Gate gate(*device);
            {
                const Recorder::LaneScope async(1);
                static_cast<void>(recorder->Commands());
            }
            const auto before = recorder->Submissions();
            gate.Record(recorder->Commands());
            const auto retired = recorder->OpenSerial();
            {
                const Recorder::LaneScope async(1);
                recorder->Submit();
            }
            Require(recorder->Submissions() == before + 1 && recorder->Recording(), "the async lane's batch was not submitted alone");
            recorder->FinishUpTo(retired);
            Require(recorder->CompletedThrough() < retired, "a resource retired at serial " + std::to_string(retired) + " by the main lane's open batch is free (completed through " + std::to_string(recorder->CompletedThrough()) + ") once the async lane's batch, opened earlier and submitted first with that serial, completed");
            recorder->Submit();
            Require(recorder->Submissions() == before + 2, "the main lane's batch was not submitted");
            Require(recorder->CompletedThrough() < retired, "a resource retired at serial " + std::to_string(retired) + " is free (completed through " + std::to_string(recorder->CompletedThrough()) + ") while the main lane's batch that recorded its use runs on the GPU with serial " + std::to_string(retired + 1));
            Require((recorder->NewestSubmitted() + 1) == retired + 2, "the use serial did not follow the submissions");
            gate.Release();
            recorder->Sync();
            Require(recorder->CompletedThrough() >= retired, "a resource retired at serial " + std::to_string(retired) + " stays held (completed through " + std::to_string(recorder->CompletedThrough()) + ") after both lanes completed");
            std::printf("lane open serial: retired at %llu, held until both lanes' batches completed\n", static_cast<unsigned long long>(retired));
        }
        device.reset();
        std::puts("lane open serial tests passed");
        AgcDriverShutdown_nid_postfix();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}

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
#include <array>
#include <chrono>
#include <cstddef>
#include <thread>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("lane forward edge: " + message);
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
                gate.Record(recorder->Commands());
                recorder->Submit();
            }
            static_cast<void>(recorder->Commands());
            {
                const Recorder::LaneScope async(1);
                const Recorder::LaneSwitch toMain(recorder, 0);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            const auto unsignaled = recorder->UnsignaledBatches();
            Require(unsignaled == 1, "the main lane's batch, recorded before the async lane's work switched to the main lane, waited on the GPU for the async lane's later batch (" + std::to_string(unsignaled) + " batches unsignaled while the async lane was held, expected 1): a cross-lane edge held back work recorded before it");
            gate.Release();
            recorder->Sync();
            std::puts("lane forward edge: the main lane's earlier work ran while the async lane was held");
        }
        device.reset();
        std::puts("lane forward edge tests passed");
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

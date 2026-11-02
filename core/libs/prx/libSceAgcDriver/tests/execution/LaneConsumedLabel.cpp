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
    if (!condition) throw std::runtime_error("lane consumed label: " + message);
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
            alignas(16) static std::uint32_t label = 0;
            const auto address = reinterpret_cast<std::uintptr_t>(&label);
            {
                const Recorder::LaneScope async(1);
                static_cast<void>(recorder->Commands());
            }
            gate.Record(recorder->Commands());
            const std::array<std::byte, 4> value{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
            recorder->NoteLabel(address, value, 1, 0x20);
            {
                const Recorder::LaneScope async(1);
                Recorder::NoteConsumedLabel(address, 4);
                {
                    const Recorder::LaneScope main(0);
                    static_cast<void>(recorder->Commands());
                }
                static_cast<void>(recorder->Commands());
                recorder->Submit();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            const auto unsignaled = recorder->UnsignaledBatches();
            Require(unsignaled == 2, "the async lane's batch recorded after it consumed a label of the main lane's open batch did not wait for that batch on the GPU (" + std::to_string(unsignaled) + " of the batches unsignaled while the main lane was held, expected 2): a command recorded into the main lane in between took the label's edge");
            gate.Release();
            recorder->Sync();
            std::puts("lane consumed label: the async lane's later work waited for the main lane's label batch");
        }
        device.reset();
        std::puts("lane consumed label tests passed");
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

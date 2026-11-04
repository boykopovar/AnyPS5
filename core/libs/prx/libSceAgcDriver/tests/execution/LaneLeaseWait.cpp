#include "VulkanTestDevice.hpp"
#include "LaneGate.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include <atomic>
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
    if (!condition) throw std::runtime_error("lane lease wait: " + message);
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
                AgcDriver::Graphics::CountLeaseOutcome(false, recorder->Submissions() + 1);
            }
            static_cast<void>(recorder->Commands());
            recorder->Submit();
            std::atomic<bool> finished{false};
            lock.unlock();
            std::thread waiter([&] {
                std::lock_guard waiterLock(AgcDriver::GuestMemory::GpuMutex());
                AgcDriver::Graphics::FinishLeaseWork();
                finished.store(true);
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            const bool early = finished.load();
            gate.Release();
            waiter.join();
            lock.lock();
            Require(!early, "FinishLeaseWork returned while the async lane's batch that recorded the lease (opened before the main lane's submission, so its serial comes later) was still held: a guest unmap could then free memory that batch writes");
            recorder->Sync();
            std::puts("lane lease wait: the lease wait covered the async lane's open batch");
        }
        device.reset();
        std::puts("lane lease wait tests passed");
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

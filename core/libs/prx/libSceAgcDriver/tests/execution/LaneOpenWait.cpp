#include "VulkanTestDevice.hpp"
#include "LaneGate.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::Graphics::Recorder;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("lane open wait: " + message);
}

using Gate = AgcDriver::Tests::TimelineGate;

enum class Waiter { SyncThrough, FinishUpTo };

const char* WaiterName(Waiter waiter) {
    return waiter == Waiter::SyncThrough ? "SyncThrough" : "FinishUpTo";
}

bool RunCase(AgcDriver::VulkanDevice& device, int awaitedLane, Waiter waiter) {
    static std::vector<std::uint8_t> written(4096);
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(written.data()));
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> ranges{{{address + 256, address + 272}}};
    const int otherLane = 1 - awaitedLane;
    std::unique_lock lock(AgcDriver::GuestMemory::GpuMutex());
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "no active recorder");
    recorder->Sync();
    Gate gate(device);
    std::uint64_t awaitedSerial = 0;
    {
        const Recorder::LaneScope scope(awaitedLane);
        gate.Record(recorder->Commands());
        recorder->NotePendingWrite(ranges[0].first, static_cast<std::size_t>(ranges[0].second - ranges[0].first));
        recorder->Submit();
        awaitedSerial = recorder->NewestSubmitted();
    }
    {
        const Recorder::LaneScope scope(otherLane);
        static_cast<void>(recorder->Commands());
    }
    const auto submittedBefore = (recorder->NewestSubmitted() + 1);
    std::atomic<bool> releasedBySubmit{false};
    std::atomic<bool> stop{false};
    std::exception_ptr failure;
    lock.unlock();
    std::thread watcher([&] {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(2000);
        while (!stop.load() && std::chrono::steady_clock::now() < deadline) {
            if ((recorder->NewestSubmitted() + 1) > submittedBefore) {
                releasedBySubmit.store(true);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        gate.Release();
    });
    std::thread waiting([&] {
        try {
            std::lock_guard waiterLock(AgcDriver::GuestMemory::GpuMutex());
            if (waiter == Waiter::SyncThrough) {
                recorder->SyncThrough(ranges[0].first, ranges[0].second - ranges[0].first);
            } else {
                recorder->FinishUpTo(awaitedSerial);
            }
        } catch (...) {
            failure = std::current_exception();
        }
    });
    waiting.join();
    stop.store(true);
    watcher.join();
    if (failure) std::rethrow_exception(failure);
    lock.lock();
    recorder->Sync();
    return releasedBySubmit.load();
}

}

int main() {
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
        }
        int failures = 0;
        for (const auto waiter : {Waiter::SyncThrough, Waiter::FinishUpTo}) {
            for (const int lane : {1, 0}) {
                const bool submitted = RunCase(*device, lane, waiter);
                if (submitted) {
                    std::printf("lane open wait: %s on a lane-%d batch submitted lane %d's open batch first\n", WaiterName(waiter), lane, 1 - lane);
                } else {
                    ++failures;
                    std::fprintf(stderr, "lane open wait: %s blocked on a lane-%d batch while lane %d's open batch, which the awaited work can depend on, was never submitted (deadlock; the watcher released the gate after 2 s)\n", WaiterName(waiter), lane, 1 - lane);
                }
            }
        }
        Require(failures == 0, std::to_string(failures) + " of 4 waits blocked in front of the other lane's unsubmitted batch");
        device.reset();
        std::puts("lane open wait tests passed");
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

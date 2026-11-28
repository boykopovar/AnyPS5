#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "VulkanTestDevice.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static void UseDevice(std::shared_ptr<VulkanDevice> device) { Driver::Get().device = std::move(device); }
    static std::uint64_t Accept() {
        auto& driver = Driver::Get();
        std::lock_guard lock(driver.mutex);
        driver.workers[0];
        return ++driver.accepted;
    }
    static std::uint32_t IdleWaiters() {
        auto& driver = Driver::Get();
        std::lock_guard lock(driver.mutex);
        return driver.idleWaiters;
    }
    static void WaitIdle() {
        auto& driver = Driver::Get();
        std::unique_lock lock(driver.mutex);
        const auto target = driver.accepted;
        ++driver.idleWaiters;
        driver.changed.wait(lock, [&] { return driver.failure != nullptr || driver.stopping || driver.completed >= target; });
        --driver.idleWaiters;
        driver.rethrowFailure();
        driver.checkStopping();
    }
    static void CompleteDeferred(std::uint64_t serial) { Driver::Get().completeSubmission(serial, serial, {}, nullptr, nullptr); }
};

}

namespace {

using AgcDriver::DriverDetail::DriverTestAccess;
using AgcDriver::Graphics::Recorder;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main() {
    try {
        std::shared_ptr<AgcDriver::VulkanDevice> device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        DriverTestAccess::UseDevice(device);
        const auto serial = DriverTestAccess::Accept();
        std::promise<std::string> outcome;
        auto waited = outcome.get_future();
        std::thread waiter([&] {
            try {
                DriverTestAccess::WaitIdle();
                outcome.set_value("returned without a failure");
            } catch (const std::exception& error) {
                outcome.set_value(error.what());
            }
        });
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (DriverTestAccess::IdleWaiters() == 0 && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        if (DriverTestAccess::IdleWaiters() == 0) {
            std::cerr << "deferred completion failure: the idle waiter never started waiting\n";
            std::_Exit(1);
        }
        {
            std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
            Require(Recorder::Active() != nullptr, "deferred completion failure: the device has no active recorder");
            static_cast<void>(Recorder::Active()->Commands());
            Recorder::CountRecordedWork();
        }
        Recorder::FailNextSubmitsForTest(1u);
        std::string thrown = "nothing";
        try {
            DriverTestAccess::CompleteDeferred(serial);
        } catch (const std::exception& error) {
            thrown = error.what();
        }
        Recorder::FailNextSubmitsForTest(0u);
        if (waited.wait_for(std::chrono::seconds(20)) != std::future_status::ready) {
            std::cerr << "deferred completion failure: the idle waiter was not woken by the failed completion\n";
            std::_Exit(1);
        }
        const auto seen = waited.get();
        waiter.join();
        Require(thrown.find("injected failure") != std::string::npos, "deferred completion failure: the completion's submit did not fail ('" + thrown + "')");
        Require(seen.find("injected failure") != std::string::npos, "deferred completion failure: an idle waiter woken by a failed completion saw '" + seen + "' instead of its failure");
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (...) {
        }
        std::puts("deferred completion failure tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

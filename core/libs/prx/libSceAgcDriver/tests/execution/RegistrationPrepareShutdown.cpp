#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("registration prepare shutdown: " + message);
}

void CheckShutdownWaits() {
    std::mutex mutex;
    std::condition_variable changed;
    bool started = false;
    bool released = false;
    std::atomic<bool> finished{false};
    std::atomic<bool> returned{false};
    std::atomic<bool> finishedAtReturn{false};
    AgcDriver::DriverDetail::Driver::Get();
    AgcDriver::DriverDetail::SubmitRegistrationPreparation([&] {
        std::unique_lock lock(mutex);
        started = true;
        changed.notify_all();
        changed.wait(lock, [&] { return released; });
        finished.store(true);
    });
    {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return started; });
    }
    std::thread shutdown([&] {
        LibcRunShutdown_nid_postfix();
        finishedAtReturn.store(finished.load());
        returned.store(true);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const bool returnedEarly = returned.load();
    {
        std::lock_guard lock(mutex);
        released = true;
    }
    changed.notify_all();
    shutdown.join();
    Require(!returnedEarly, "shutdown returned while a registration preparation was still running");
    Require(finishedAtReturn.load(), "shutdown returned before the running registration preparation finished");
}

void CheckLateSubmissionRunsInline() {
    const auto caller = std::this_thread::get_id();
    std::thread::id ran;
    AgcDriver::DriverDetail::SubmitRegistrationPreparation([&] { ran = std::this_thread::get_id(); });
    Require(ran == caller, "a registration preparation submitted after shutdown did not run on the caller's thread");
}

}

int main() {
    try {
        CheckShutdownWaits();
        CheckLateSubmissionRunsInline();
        std::puts("registration prepare shutdown tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

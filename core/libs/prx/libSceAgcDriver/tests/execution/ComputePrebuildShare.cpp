#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error("compute prebuild share: " + message);
}

unsigned PrebuildThreads() {
    const char* text = std::getenv("APS5_PREBUILD_THREADS");
    return text != nullptr ? std::max(1u, static_cast<unsigned>(std::strtoul(text, nullptr, 10))) : std::max(2u, std::thread::hardware_concurrency() / 2u);
}

struct Gate {
    std::mutex mutex;
    std::condition_variable changed;
    bool open = false;
    bool libraryBuilt = false;
    unsigned running = 0;
    unsigned most = 0;
    unsigned finished = 0;
};

struct OpenOnExit {
    std::shared_ptr<Gate> gate;
    ~OpenOnExit() {
        {
            std::lock_guard lock(gate->mutex);
            gate->open = true;
        }
        gate->changed.notify_all();
        AgcDriver::DriverDetail::AwaitLibraryPrebuilds();
    }
};

void CheckLibraryPrebuildRunsBesideBlockedComputePrebuilds() {
    const unsigned threads = PrebuildThreads();
    const unsigned limit = std::max(1u, threads / 2u);
    const auto gate = std::make_shared<Gate>();
    const OpenOnExit release{gate};
    for (unsigned index = 0; index < threads; ++index) {
        AgcDriver::DriverDetail::SubmitComputePrebuild([gate] {
            std::unique_lock lock(gate->mutex);
            ++gate->running;
            gate->most = std::max(gate->most, gate->running);
            gate->changed.notify_all();
            gate->changed.wait(lock, [&] { return gate->open; });
            --gate->running;
            ++gate->finished;
            gate->changed.notify_all();
        });
    }
    std::unique_lock lock(gate->mutex);
    Require(gate->changed.wait_for(lock, std::chrono::seconds(10), [&] { return gate->running == limit; }), std::to_string(gate->running) + " of " + std::to_string(threads) + " blocked compute prebuilds started, not " + std::to_string(limit));
    lock.unlock();
    AgcDriver::DriverDetail::SubmitLibraryPrebuild([gate] {
        std::lock_guard built(gate->mutex);
        gate->libraryBuilt = true;
        gate->changed.notify_all();
    });
    lock.lock();
    const bool built = gate->changed.wait_for(lock, std::chrono::seconds(10), [&] { return gate->libraryBuilt; });
    Require(built, "a library prebuild queued behind " + std::to_string(threads) + " blocked compute prebuilds did not run while they waited");
    Require(gate->running == limit, std::to_string(gate->running) + " compute prebuilds ran beside the library prebuild, not " + std::to_string(limit));
    gate->open = true;
    gate->changed.notify_all();
    Require(gate->changed.wait_for(lock, std::chrono::seconds(10), [&] { return gate->finished == threads; }), "only " + std::to_string(gate->finished) + " of " + std::to_string(threads) + " compute prebuilds finished after they were released");
    Require(gate->most == limit, std::to_string(gate->most) + " compute prebuilds ran at once, more than " + std::to_string(limit));
}

}

int main() {
    try {
        AgcDriver::DriverDetail::Driver::Get();
        CheckLibraryPrebuildRunsBesideBlockedComputePrebuilds();
        AgcDriver::DriverDetail::CloseRegistrationPreparation();
        std::puts("compute prebuild share tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

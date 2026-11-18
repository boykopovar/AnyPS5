#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <thread>
#include "prx/libc/include/Shutdown.hpp"
#include "AudioOutShutdown.hpp"

namespace {

std::atomic<std::uint32_t> inside{0};
std::atomic<std::uint32_t> background{0};
std::atomic<bool> stopping{false};

void Leave() {
    if (inside.fetch_sub(1) == 1) inside.notify_all();
}

void Stop() {
    stopping.store(true);
    for (auto count = inside.load(); count != 0; count = inside.load()) inside.wait(count);
    for (auto count = background.load(); count != 0; count = background.load()) background.wait(count);
}

void RegisterStop() {
    static const bool registered = (LibcRegisterShutdown_nid_postfix(Stop), true);
    static_cast<void>(registered);
}

}

AudioOutCall::AudioOutCall() {
    RegisterStop();
    inside.fetch_add(1);
    if (stopping.load() || LibcShutdownToken_nid_postfix().stop_requested()) {
        Leave();
        throw ProcessShutdown{};
    }
}

AudioOutCall::~AudioOutCall() {
    Leave();
}

void AudioOutRunInBackground(std::function<void()> task, std::chrono::microseconds grace) {
    RegisterStop();
    background.fetch_add(1);
    auto finished = std::make_shared<std::promise<void>>();
    auto done = finished->get_future();
    try {
        std::thread([task = std::move(task), finished] {
            task();
            finished->set_value();
            if (background.fetch_sub(1) == 1) background.notify_all();
        }).detach();
    } catch (...) {
        background.fetch_sub(1);
        throw;
    }
    done.wait_for(grace);
}

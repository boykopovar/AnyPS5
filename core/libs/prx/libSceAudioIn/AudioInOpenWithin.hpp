#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

namespace audioin::detail {

struct OpenWithinResult {
    std::uint32_t device = 0;
    bool timedOut = false;
};

template <typename TOpen, typename TClose, typename TAbandoned = void (*)(), typename TSettled = void (*)()>
OpenWithinResult openWithin(std::chrono::milliseconds limit, TOpen open, TClose close, TAbandoned abandoned = +[] {},
                            TSettled settled = +[] {}) {
    struct State {
        std::mutex mutex;
        std::condition_variable done;
        bool finished = false;
        bool abandoned = false;
        std::uint32_t device = 0;
    };
    auto state = std::make_shared<State>();
    std::thread([state, open = std::move(open), close = std::move(close), settled = std::move(settled)]() mutable {
        const std::uint32_t device = open();
        std::lock_guard lock(state->mutex);
        if (state->abandoned) {
            if (device != 0) close(device);
            settled();
            return;
        }
        state->device = device;
        state->finished = true;
        state->done.notify_all();
    }).detach();
    std::unique_lock lock(state->mutex);
    if (!state->done.wait_for(lock, limit, [&state] { return state->finished; })) {
        state->abandoned = true;
        abandoned();
        return {0, true};
    }
    return {state->device, false};
}

}

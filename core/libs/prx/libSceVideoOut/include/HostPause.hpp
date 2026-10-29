#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_HOSTPAUSE_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_HOSTPAUSE_HPP

#include "prx/libSceVideoOut/include/HostPauseSettings.hpp"
#include "SDL_events.h"
#include <atomic>
#include <chrono>
#include <functional>

class HostPause {
public:
    using Clock = std::function<std::chrono::steady_clock::time_point()>;

    struct Change {
        bool changed = false;
        bool paused = false;
        std::chrono::steady_clock::duration pausedFor{};
    };

    explicit HostPause(HostPauseSettings settings, Clock clock = [] { return std::chrono::steady_clock::now(); });

    Change HandleEvent(const SDL_Event& event, unsigned windowId);
    bool Paused() const { return paused.load(std::memory_order_acquire); }

private:
    Change update();

    HostPauseSettings settings;
    Clock clock;
    bool keyPaused = false;
    bool focusLost = false;
    std::chrono::steady_clock::time_point pausedAt{};
    std::atomic<bool> paused{false};
};

#endif

#include "prx/libSceVideoOut/include/HostPause.hpp"

#include <utility>

HostPause::HostPause(HostPauseSettings settings, Clock clock) : settings(settings), clock(std::move(clock)) {}

HostPause::Change HostPause::HandleEvent(const SDL_Event& event, unsigned windowId) {
    if (settings.key != SDL_SCANCODE_UNKNOWN && event.type == SDL_KEYDOWN && event.key.windowID == windowId && event.key.repeat == 0 && event.key.keysym.scancode == settings.key) {
        keyPaused = !keyPaused;
    }
    return update();
}

HostPause::Change HostPause::update() {
    const bool wanted = keyPaused;
    Change change;
    change.paused = wanted;
    if (wanted == paused.load(std::memory_order_relaxed)) return change;
    change.changed = true;
    if (wanted) {
        pausedAt = clock();
    } else {
        change.pausedFor = clock() - pausedAt;
    }
    paused.store(wanted, std::memory_order_release);
    return change;
}

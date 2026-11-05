#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_TESTS_PADSIMULATION_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_TESTS_PADSIMULATION_HPP

#include "prx/libScePad/include/KeyboardMouse.hpp"

#include <chrono>
#include <cmath>
#include <vector>

#include "SDL_keyboard.h"

namespace PadSimulation {

struct TimedEvent {
    double ms = 0.0;
    SDL_Event event{};
    bool setMouseMode = false;
    bool mouseMode = false;
    bool release = false;
};

struct Sample {
    double ms = 0.0;
    PadInputState state;
};

inline SDL_Event MotionEvent(int deltaX, int deltaY) {
    SDL_Event event{};
    event.type = SDL_MOUSEMOTION;
    event.motion.xrel = deltaX;
    event.motion.yrel = deltaY;
    return event;
}

inline SDL_Event KeyEvent(SDL_Scancode key, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.key.keysym.scancode = key;
    event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    return event;
}

inline SDL_Event ButtonEvent(Uint8 button, bool down) {
    SDL_Event event{};
    event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
    event.button.button = button;
    event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    return event;
}

inline SDL_Event FocusLostEvent() {
    SDL_Event event{};
    event.type = SDL_WINDOWEVENT;
    event.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    return event;
}

inline SDL_Event WheelEvent(int direction) {
    SDL_Event event{};
    event.type = SDL_MOUSEWHEEL;
    event.wheel.y = direction;
    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
    return event;
}

inline TimedEvent At(double ms, const SDL_Event& event) {
    return {ms, event, false, false, false};
}

inline TimedEvent MouseModeAt(double ms, bool enabled) {
    return {ms, SDL_Event{}, true, enabled, false};
}

inline TimedEvent ReleaseAt(double ms) {
    return {ms, SDL_Event{}, false, false, true};
}

inline void AppendMotion(std::vector<TimedEvent>& events, double startMs, double durationMs, double countsPerSecondX, double countsPerSecondY, double reportHz = 1000.0) {
    const double periodMs = 1000.0 / reportHz;
    double owedX = 0.0;
    double owedY = 0.0;
    for (double ms = startMs + periodMs; ms <= startMs + durationMs + 1e-9; ms += periodMs) {
        owedX += countsPerSecondX * periodMs / 1000.0;
        owedY += countsPerSecondY * periodMs / 1000.0;
        const int deltaX = static_cast<int>(std::trunc(owedX));
        const int deltaY = static_cast<int>(std::trunc(owedY));
        owedX -= deltaX;
        owedY -= deltaY;
        if (deltaX != 0 || deltaY != 0) events.push_back(At(ms, MotionEvent(deltaX, deltaY)));
    }
}

inline std::vector<Sample> Simulate(const Pad::InputConfiguration& configuration, const std::vector<TimedEvent>& events, double pollMs, double endMs) {
    using Clock = Pad::KeyboardMouse::Clock;
    const auto origin = Clock::time_point{} + std::chrono::hours(1);
    const auto at = [origin](double ms) {
        return origin + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double, std::milli>(ms));
    };
    Pad::KeyboardMouse keyboardMouse(configuration);
    keyboardMouse.Advance(at(0.0));
    std::vector<Sample> samples;
    std::size_t next = 0;
    const auto steps = static_cast<long long>(std::floor(endMs / pollMs + 1e-9));
    for (long long step = 1; step <= steps; ++step) {
        const double ms = static_cast<double>(step) * pollMs;
        const auto now = at(ms);
        for (; next < events.size() && events[next].ms <= ms + 1e-9; ++next) {
            const auto& timed = events[next];
            if (timed.release) {
                keyboardMouse.Release();
                continue;
            }
            if (timed.setMouseMode) {
                keyboardMouse.SetMouseMode(timed.mouseMode);
                continue;
            }
            const auto requests = keyboardMouse.HandleEvent(timed.event, now);
            if (requests.toggleMouse) keyboardMouse.SetMouseMode(!keyboardMouse.MouseMode());
            if (requests.releaseMouse) keyboardMouse.SetMouseMode(false);
        }
        keyboardMouse.Advance(now);
        samples.push_back({ms, keyboardMouse.Compose(PadInputState{})});
    }
    return samples;
}

}

#endif

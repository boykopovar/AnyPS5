#include "prx/libScePad/include/KeyboardMouse.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

Pad::MouseStick::MouseStick(const InputSettings& settings) : settings(settings) {}

void Pad::MouseStick::Move(int deltaX, int deltaY) {
    pendingX += deltaX;
    pendingY += deltaY;
}

void Pad::MouseStick::Advance(double seconds) {
    if (!(seconds > 0.0)) return;
    pendingSeconds += seconds;
    if (pendingSeconds < MouseMinimumStepSeconds) return;
    const double step = pendingSeconds;
    pendingSeconds = 0.0;
    const bool moved = pendingX != 0.0 || pendingY != 0.0;
    const double scale = settings.mouseSensitivity / (MouseFullTiltCountsPerSecond * step);
    const double targetX = pendingX * scale;
    const double targetY = pendingY * scale * settings.mouseVerticalScale;
    pendingX = 0.0;
    pendingY = 0.0;
    idleSeconds = moved ? 0.0 : idleSeconds + step;
    if (idleSeconds * 1000.0 >= settings.mouseDecayMs) {
        speedX = 0.0;
        speedY = 0.0;
        return;
    }
    const double keep = settings.mouseSmoothingMs > 0.0 ? std::exp(-step * 1000.0 / settings.mouseSmoothingMs) : 0.0;
    speedX = targetX + (speedX - targetX) * keep;
    speedY = targetY + (speedY - targetY) * keep;
    const double speed = std::hypot(speedX, speedY);
    if (speed > settings.mouseMaxSpeed) {
        speedX *= settings.mouseMaxSpeed / speed;
        speedY *= settings.mouseMaxSpeed / speed;
    }
}

void Pad::MouseStick::Reset() {
    pendingX = 0.0;
    pendingY = 0.0;
    pendingSeconds = 0.0;
    speedX = 0.0;
    speedY = 0.0;
    idleSeconds = 0.0;
}

double Pad::MouseStick::Speed() const {
    return std::hypot(speedX, speedY);
}

std::array<double, 2> Pad::MouseStick::Deflection() const {
    const double speed = std::hypot(speedX, speedY);
    if (speed <= 0.0) return {0.0, 0.0};
    const double shaped = std::pow(std::min(speed, 1.0), settings.mouseCurve);
    const double deflection = settings.mouseDeadzoneOffset + (1.0 - settings.mouseDeadzoneOffset) * shaped;
    return {speedX / speed * deflection, speedY / speed * deflection};
}

std::uint8_t Pad::StickByte(double deflection) {
    const double clamped = std::clamp(deflection, -1.0, 1.0);
    return static_cast<std::uint8_t>(std::lround(clamped < 0.0 ? 128.0 + clamped * 128.0 : 128.0 + clamped * 127.0));
}

Pad::KeyboardMouse::KeyboardMouse(InputConfiguration configuration)
    : configuration(std::move(configuration)),
      pressed(this->configuration.bindings.size()),
      wheelReleaseTimes(this->configuration.bindings.size()),
      mouse(this->configuration.settings) {}

const Pad::InputConfiguration& Pad::KeyboardMouse::Configuration() const {
    return configuration;
}

bool Pad::KeyboardMouse::MouseMode() const {
    return mouseMode;
}

bool Pad::KeyboardMouse::Advance(Clock::time_point now) {
    if (!advanced) {
        advanced = true;
        lastAdvance = now;
    }
    if (now > lastAdvance) {
        const double seconds = std::chrono::duration<double>(now - lastAdvance).count();
        lastAdvance = now;
        if (mouseMode) mouse.Advance(seconds);
    }
    bool released = false;
    for (std::size_t index = 0; index < configuration.bindings.size(); ++index) {
        if (configuration.bindings[index].wheelDirection == 0 || !pressed[index] || now < wheelReleaseTimes[index]) continue;
        pressed[index] = false;
        wheelReleaseTimes[index] = {};
        released = true;
    }
    return released;
}

void Pad::KeyboardMouse::SetMouseMode(bool enabled) {
    mouseMode = enabled;
    mouse.Reset();
}

void Pad::KeyboardMouse::Release() {
    std::fill(pressed.begin(), pressed.end(), false);
    std::fill(wheelReleaseTimes.begin(), wheelReleaseTimes.end(), Clock::time_point{});
}

Pad::InputRequests Pad::KeyboardMouse::HandleEvent(const SDL_Event& event, Clock::time_point now) {
    InputRequests requests;
    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event != SDL_WINDOWEVENT_FOCUS_LOST && event.window.event != SDL_WINDOWEVENT_CLOSE) return requests;
        Release();
        requests.releaseMouse = mouseMode;
        return requests;
    }
    if (event.type == SDL_MOUSEMOTION) {
        if (mouseMode) mouse.Move(event.motion.xrel, event.motion.yrel);
        return requests;
    }
    if (event.type == SDL_MOUSEWHEEL) {
        int direction = (event.wheel.y > 0) - (event.wheel.y < 0);
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) direction = -direction;
        if (direction == 0) return requests;
        const auto releaseTime = now + std::chrono::milliseconds(WheelPressDurationMs);
        for (std::size_t index = 0; index < configuration.bindings.size(); ++index) {
            const auto& binding = configuration.bindings[index];
            if (binding.wheelDirection == 0) continue;
            pressed[index] = binding.wheelDirection == direction;
            wheelReleaseTimes[index] = pressed[index] ? releaseTime : Clock::time_point{};
        }
        return requests;
    }
    const bool keyboard = event.type == SDL_KEYDOWN || event.type == SDL_KEYUP;
    const bool mouseButton = event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP;
    if (!keyboard && !mouseButton) return requests;
    if (keyboard && event.key.repeat != 0) return requests;
    const bool down = event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN;
    for (std::size_t index = 0; index < configuration.bindings.size(); ++index) {
        const auto& binding = configuration.bindings[index];
        const bool matches = keyboard
            ? binding.key != SDL_SCANCODE_UNKNOWN && binding.key == event.key.keysym.scancode
            : binding.mouseButton != MouseButton::None && binding.mouseButton == static_cast<MouseButton>(event.button.button);
        if (!matches) continue;
        if (binding.control == InputControl::ToggleFullscreen && keyboard && down && !pressed[index]) requests.toggleFullscreen = true;
        if (binding.control == InputControl::TogglePerformanceOverlay && keyboard && down && !pressed[index]) requests.togglePerformanceOverlay = true;
        if (binding.control == InputControl::ToggleMouse && down && !pressed[index]) requests.toggleMouse = true;
        pressed[index] = down;
    }
    return requests;
}

PadInputState Pad::KeyboardMouse::Compose(const PadInputState& controller) const {
    PadInputState state = controller;
    std::array<bool, 4> negative{};
    std::array<bool, 4> positive{};
    for (std::size_t index = 0; index < configuration.bindings.size(); ++index) {
        if (!pressed[index]) continue;
        const auto& binding = configuration.bindings[index];
        switch (binding.control) {
            case InputControl::Button:
                state.buttons |= static_cast<std::uint32_t>(binding.button);
                if (binding.button == PadButton::L2) state.analogButtonsL2 = 255;
                if (binding.button == PadButton::R2) state.analogButtonsR2 = 255;
                break;
            case InputControl::LeftStickLeft: negative[0] = true; break;
            case InputControl::LeftStickRight: positive[0] = true; break;
            case InputControl::LeftStickUp: negative[1] = true; break;
            case InputControl::LeftStickDown: positive[1] = true; break;
            case InputControl::RightStickLeft: negative[2] = true; break;
            case InputControl::RightStickRight: positive[2] = true; break;
            case InputControl::RightStickUp: negative[3] = true; break;
            case InputControl::RightStickDown: positive[3] = true; break;
            case InputControl::TouchLeft: state.touchLeft = true; break;
            case InputControl::TouchRight: state.touchRight = true; break;
            case InputControl::ToggleMouse: break;
            case InputControl::ToggleFullscreen: break;
            case InputControl::TogglePerformanceOverlay: break;
        }
    }
    for (std::size_t axis = 0; axis < state.sticks.size(); ++axis) {
        if (negative[axis] || positive[axis]) state.sticks[axis] = negative[axis] == positive[axis] ? 128 : negative[axis] ? 0 : 255;
    }
    if (mouseMode) {
        const auto deflection = mouse.Deflection();
        state.sticks[2] = StickByte(deflection[0]);
        state.sticks[3] = StickByte(deflection[1]);
    }
    if (state.analogButtonsL2 != 0) state.buttons |= static_cast<std::uint32_t>(PadButton::L2);
    if (state.analogButtonsR2 != 0) state.buttons |= static_cast<std::uint32_t>(PadButton::R2);
    return state;
}

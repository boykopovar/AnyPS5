#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#include "SDL.h"
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include "prx/libc/include/General.hpp"

PadInput::PadInput()
    : bindings(Pad::LoadInputMapping()), pressed(bindings.size()), wheelReleaseTimes(bindings.size()) {
    openFirstAvailableController();
}

PadInput::~PadInput() {
    closeController();
}

void PadInput::openFirstAvailableController() {
    if (controller != nullptr) return;
    for (int deviceIndex = 0; deviceIndex < SDL_NumJoysticks(); ++deviceIndex) {
        if (!SDL_IsGameController(deviceIndex)) continue;
        openController(deviceIndex);
        if (controller != nullptr) return;
    }
}

void PadInput::openController(int deviceIndex) {
    if (controller != nullptr || !SDL_IsGameController(deviceIndex)) return;
    controller = SDL_GameControllerOpen(deviceIndex);
    if (controller == nullptr) {
        APS5_LOG_ERR("Pad: could not open game controller %d: %s", deviceIndex, SDL_GetError());
        return;
    }
    const char* name = SDL_GameControllerName(controller);
    APS5_LOG_OUT("Pad: connected game controller: %s", name != nullptr ? name : "unknown");
}

void PadInput::closeController() {
    if (controller == nullptr) return;
    SDL_GameControllerClose(controller);
    controller = nullptr;
}

void PadInput::setMouseMode(bool enabled) {
    if (SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE) != 0) throw std::runtime_error(std::string("Pad: relative mouse mode failed: ") + SDL_GetError());
    int deltaX = 0;
    int deltaY = 0;
    SDL_GetRelativeMouseState(&deltaX, &deltaY);
    mouseEnabled = enabled;
    mouseStick = {128, 128};
    nextMousePoll = std::chrono::steady_clock::now() + std::chrono::milliseconds(Pad::MousePollIntervalMs);
}

void PadInput::HandleEvent(const SDL_Event& event, DisplayWindow& window) {
    if (event.type == SDL_CONTROLLERDEVICEADDED) {
        openController(event.cdevice.which);
        return;
    }
    if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller != nullptr) {
        const auto instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
        if (instanceId == event.cdevice.which) {
            closeController();
            openFirstAvailableController();
            publish();
        }
        return;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP ||
        event.type == SDL_CONTROLLERAXISMOTION || event.type == SDL_CONTROLLERDEVICEREMAPPED) {
        publish();
        return;
    }
    if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST || event.window.event == SDL_WINDOWEVENT_CLOSE)) {
        std::fill(pressed.begin(), pressed.end(), false);
        std::fill(wheelReleaseTimes.begin(), wheelReleaseTimes.end(), std::chrono::steady_clock::time_point{});
        if (mouseEnabled) setMouseMode(false);
        publish();
        return;
    }
    // APS5_NO_PAD_INPUT=1 keeps a measurement run from reacting to keys or mouse buttons that reach
    // its window (a stray press advances the title into another stage).
    static const bool ignoreInput = std::getenv("APS5_NO_PAD_INPUT") != nullptr;
    if (ignoreInput && event.type == SDL_MOUSEWHEEL) return;
    if (event.type == SDL_MOUSEWHEEL) {
        int direction = (event.wheel.y > 0) - (event.wheel.y < 0);
        if (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) direction = -direction;
        if (direction == 0) return;
        const auto releaseTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(Pad::WheelPressDurationMs);
        for (std::size_t index = 0; index < bindings.size(); ++index) {
            const auto& binding = bindings[index];
            if (binding.wheelDirection == 0) continue;
            pressed[index] = binding.wheelDirection == direction;
            wheelReleaseTimes[index] = pressed[index] ? releaseTime : std::chrono::steady_clock::time_point{};
        }
        publish();
        return;
    }
    const bool keyboard = event.type == SDL_KEYDOWN || event.type == SDL_KEYUP;
    const bool mouse = event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP;
    if (!keyboard && !mouse) return;
    if (ignoreInput) return;
    if (keyboard && event.key.repeat != 0) return;
    const bool down = event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN;
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        const auto& binding = bindings[index];
        const bool matches = keyboard
            ? binding.key != SDL_SCANCODE_UNKNOWN && binding.key == event.key.keysym.scancode
            : binding.mouseButton != Pad::MouseButton::None && binding.mouseButton == static_cast<Pad::MouseButton>(event.button.button);

        if (!matches) continue;
        if (binding.control == Pad::InputControl::ToggleFullscreen) {
            if (keyboard && down && !pressed[index] && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) window.ToggleFullscreen();
        }
        if (binding.control == Pad::InputControl::ToggleMouse && down && !pressed[index]) setMouseMode(!mouseEnabled);
        pressed[index] = down;
    }
    publish();
}

void PadInput::Update() {
    const auto now = std::chrono::steady_clock::now();
    if (controller != nullptr) {
        SDL_GameControllerUpdate();
        publish();
    }
    bool released = false;
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        if (bindings[index].wheelDirection == 0 || !pressed[index] || now < wheelReleaseTimes[index]) continue;
        pressed[index] = false;
        wheelReleaseTimes[index] = {};
        released = true;
    }
    if (released) publish();
    if (!mouseEnabled) return;
    if (SDL_GetKeyboardFocus() == nullptr) {
        std::fill(pressed.begin(), pressed.end(), false);
        std::fill(wheelReleaseTimes.begin(), wheelReleaseTimes.end(), std::chrono::steady_clock::time_point{});
        setMouseMode(false);
        publish();
        return;
    }
    if (now < nextMousePoll) return;
    nextMousePoll = now + std::chrono::milliseconds(Pad::MousePollIntervalMs);
    int deltaX = 0;
    int deltaY = 0;
    SDL_GetRelativeMouseState(&deltaX, &deltaY);
    mouseStick = {128, 128};
    if (deltaX != 0 || deltaY != 0) {
        const double distance = std::hypot(deltaX, deltaY);
        const double scale = std::clamp(distance * Pad::MouseSensitivity + 16.0, 64.0, 128.0) / distance;
        const auto mapAxis = [scale](int delta) { return static_cast<std::uint8_t>(std::clamp(128L + std::lround(delta * scale), 0L, 255L)); };
        mouseStick = {mapAxis(deltaX), mapAxis(deltaY)};
    }
    publish();
}

void PadInput::publish() {
    PadInputState state;
    if (controller != nullptr && SDL_GameControllerGetAttached(controller) == SDL_TRUE) {
        const auto readButton = [this](SDL_GameControllerButton button) {
            return SDL_GameControllerGetButton(controller, button) != 0;
        };
        const auto addButton = [&state, &readButton](SDL_GameControllerButton source, Pad::PadButton button) {
            if (readButton(source)) state.buttons |= static_cast<std::uint32_t>(button);
        };
        addButton(SDL_CONTROLLER_BUTTON_A, Pad::PadButton::Cross);
        addButton(SDL_CONTROLLER_BUTTON_B, Pad::PadButton::Circle);
        addButton(SDL_CONTROLLER_BUTTON_X, Pad::PadButton::Square);
        addButton(SDL_CONTROLLER_BUTTON_Y, Pad::PadButton::Triangle);
        addButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Pad::PadButton::L1);
        addButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Pad::PadButton::R1);
        addButton(SDL_CONTROLLER_BUTTON_START, Pad::PadButton::Options);
        addButton(SDL_CONTROLLER_BUTTON_LEFTSTICK, Pad::PadButton::L3);
        addButton(SDL_CONTROLLER_BUTTON_RIGHTSTICK, Pad::PadButton::R3);
        addButton(SDL_CONTROLLER_BUTTON_DPAD_UP, Pad::PadButton::Up);
        addButton(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, Pad::PadButton::Right);
        addButton(SDL_CONTROLLER_BUTTON_DPAD_DOWN, Pad::PadButton::Down);
        addButton(SDL_CONTROLLER_BUTTON_DPAD_LEFT, Pad::PadButton::Left);
        addButton(SDL_CONTROLLER_BUTTON_TOUCHPAD, Pad::PadButton::TouchPad);

        const auto triggerValue = [this](SDL_GameControllerAxis axis) {
            const auto value = std::clamp<int>(SDL_GameControllerGetAxis(controller, axis), 0, 32767);
            return static_cast<std::uint8_t>((value * 255 + 16383) / 32767);
        };
        state.analogButtonsL2 = triggerValue(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        state.analogButtonsR2 = triggerValue(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        if (state.analogButtonsL2 != 0) state.buttons |= static_cast<std::uint32_t>(Pad::PadButton::L2);
        if (state.analogButtonsR2 != 0) state.buttons |= static_cast<std::uint32_t>(Pad::PadButton::R2);

        const auto stickValue = [this](SDL_GameControllerAxis axis) {
            const auto value = static_cast<std::int32_t>(SDL_GameControllerGetAxis(controller, axis)) + 32768;
            return static_cast<std::uint8_t>((value * 255 + 32767) / 65535);
        };
        state.sticks = {
            stickValue(SDL_CONTROLLER_AXIS_LEFTX),
            stickValue(SDL_CONTROLLER_AXIS_LEFTY),
            stickValue(SDL_CONTROLLER_AXIS_RIGHTX),
            stickValue(SDL_CONTROLLER_AXIS_RIGHTY)
        };
    }
    std::array<bool, 4> negative{};
    std::array<bool, 4> positive{};
    for (std::size_t index = 0; index < bindings.size(); ++index) {
        if (!pressed[index]) continue;
        const auto& binding = bindings[index];
        switch (binding.control) {
            case Pad::InputControl::Button:
                state.buttons |= static_cast<std::uint32_t>(binding.button);
                if (binding.button == Pad::PadButton::L2) state.analogButtonsL2 = 255;
                if (binding.button == Pad::PadButton::R2) state.analogButtonsR2 = 255;
                break;
            case Pad::InputControl::LeftStickLeft: negative[0] = true; break;
            case Pad::InputControl::LeftStickRight: positive[0] = true; break;
            case Pad::InputControl::LeftStickUp: negative[1] = true; break;
            case Pad::InputControl::LeftStickDown: positive[1] = true; break;
            case Pad::InputControl::RightStickLeft: negative[2] = true; break;
            case Pad::InputControl::RightStickRight: positive[2] = true; break;
            case Pad::InputControl::RightStickUp: negative[3] = true; break;
            case Pad::InputControl::RightStickDown: positive[3] = true; break;
            case Pad::InputControl::TouchLeft: state.touchLeft = true; break;
            case Pad::InputControl::TouchRight: state.touchRight = true; break;
            case Pad::InputControl::ToggleMouse: break;
            case Pad::InputControl::ToggleFullscreen: break;
        }
    }
    for (std::size_t axis = 0; axis < state.sticks.size(); ++axis) {
        if (negative[axis] || positive[axis]) state.sticks[axis] = negative[axis] == positive[axis] ? 128 : negative[axis] ? 0 : 255;
    }
    if (mouseEnabled) {
        state.sticks[2] = mouseStick[0];
        state.sticks[3] = mouseStick[1];
    }
    PadPublishInput_nid_postfix(state);
}

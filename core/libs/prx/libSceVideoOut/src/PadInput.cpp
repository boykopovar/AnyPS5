#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

#include "SDL.h"
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"

namespace {

std::string appTitleId() {
    const auto titleId = GetAppTitleId_nid_postfix();
    return std::string(titleId.value, std::find(titleId.value, titleId.value + sizeof(titleId.value), '\0'));
}

}

PadInput::PadInput() : PadInput(Pad::LoadInputMapping(appTitleId())) {
}

PadInput::PadInput(Pad::InputConfiguration configuration) : keyboardMouse(std::move(configuration)) {
    openFirstAvailableController();
}

PadInput::~PadInput() {
    closeController();
}

void PadInput::openFirstAvailableController() {
    if (controller != nullptr) return;
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_RUMBLE, "1");
    if ((SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
            APS5_LOG_ERR("Pad: SDL game controller init failed: %s", SDL_GetError());
            return;
        }
    }
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
    APS5_LOG_OUT("Pad: connected game controller: %s (type %d, sensors accel=%d gyro=%d, touchpads=%d, led=%d, trigger rumble=%d)",
        name != nullptr ? name : "unknown", static_cast<int>(SDL_GameControllerGetType(controller)),
        SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) == SDL_TRUE, SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) == SDL_TRUE,
        SDL_GameControllerGetNumTouchpads(controller), SDL_GameControllerHasLED(controller) == SDL_TRUE, SDL_GameControllerHasRumbleTriggers(controller) == SDL_TRUE);
    enableSensors();
    outputPending = true;
    lightBar.Reset();
    triggerEffect.Reset();
}

void PadInput::enableSensors() {
    if (controller == nullptr) return;
    const SDL_bool wanted = outputState.motionEnabled ? SDL_TRUE : SDL_FALSE;
    if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) == SDL_TRUE) SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, wanted);
    if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) == SDL_TRUE) SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, wanted);
}

void PadInput::closeController() {
    if (controller == nullptr) return;
    SDL_GameControllerClose(controller);
    controller = nullptr;
    controllerState = {};
}

void PadInput::applyOutput() {
    PadOutputState fetched;
    if (PadFetchOutput_nid_postfix(&outputSequence, &fetched)) {
        const bool motionChanged = fetched.motionEnabled != outputState.motionEnabled;
        outputState = fetched;
        outputPending = true;
        if (motionChanged) enableSensors();
    }
    if (controller == nullptr) return;
    const auto now = std::chrono::steady_clock::now();
    const bool isPs5 = SDL_GameControllerGetType(controller) == SDL_CONTROLLER_TYPE_PS5;
    const auto large = std::max(outputState.vibrationLarge, outputState.audioVibrationLarge);
    const auto small = std::max(outputState.vibrationSmall, outputState.audioVibrationSmall);
    const bool rumbling = large != 0 || small != 0;
    const bool triggerRumble = outputState.trigger[0].fallback != 0 || outputState.trigger[1].fallback != 0;
    if (SDL_GameControllerHasLED(controller) == SDL_TRUE) {
        const LightBarPacer::Color color = outputState.lightBarValid
            ? LightBarPacer::Color{outputState.lightBar[0], outputState.lightBar[1], outputState.lightBar[2]}
            : LightBarPacer::Color{0, 64, 255};
        if (lightBar.Due(color, now)) lightBar.Result(color, now, SDL_GameControllerSetLED(controller, color[0], color[1], color[2]) == 0);
    }
    if (!outputPending) {
        if ((rumbling || (triggerRumble && !isPs5)) && now >= nextRumbleRefresh) outputPending = true;
        else return;
    }
    outputPending = false;
    nextRumbleRefresh = now + std::chrono::milliseconds(700);
    constexpr Uint32 rumbleMs = 2000;
    SDL_GameControllerRumble(controller, static_cast<Uint16>(large * 257), static_cast<Uint16>(small * 257), rumbling ? rumbleMs : 0);
    if (outputState.triggerTouched) {
        if (isPs5) {
            TriggerEffectCache::Effect effect{};
            effect[0] = 0x04 | 0x08;
            std::memcpy(effect.data() + 10, outputState.trigger[1].effect, 11);
            std::memcpy(effect.data() + 21, outputState.trigger[0].effect, 11);
            if (triggerEffect.NeedsSend(effect)) {
                triggerEffect.Result(effect, SDL_GameControllerSendEffect(controller, effect.data(), static_cast<int>(effect.size())) == 0);
            }
        } else if (SDL_GameControllerHasRumbleTriggers(controller) == SDL_TRUE) {
            SDL_GameControllerRumbleTriggers(controller, static_cast<Uint16>(outputState.trigger[0].fallback * 257), static_cast<Uint16>(outputState.trigger[1].fallback * 257), triggerRumble ? rumbleMs : 0);
        }
    }
}

void PadInput::setMouseMode(bool enabled) {
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");
    if (SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE) != 0) throw std::runtime_error(std::string("Pad: relative mouse mode failed: ") + SDL_GetError());
    keyboardMouse.SetMouseMode(enabled);
}

PadInputState PadInput::sampleController() const {
    PadInputState result;
    if (controller == nullptr) return result;
    const auto readButton = [this](SDL_GameControllerButton button) {
        return SDL_GameControllerGetButton(controller, button) != 0;
    };
    const auto addButton = [&result, &readButton](SDL_GameControllerButton source, Pad::PadButton button) {
        if (readButton(source)) result.buttons |= static_cast<std::uint32_t>(button);
    };
    addButton(SDL_CONTROLLER_BUTTON_A, Pad::PadButton::Cross);
    addButton(SDL_CONTROLLER_BUTTON_B, Pad::PadButton::Circle);
    addButton(SDL_CONTROLLER_BUTTON_X, Pad::PadButton::Square);
    addButton(SDL_CONTROLLER_BUTTON_Y, Pad::PadButton::Triangle);
    addButton(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, Pad::PadButton::L1);
    addButton(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, Pad::PadButton::R1);
    const auto type = SDL_GameControllerGetType(controller);
    const bool viewPressed = type != SDL_CONTROLLER_TYPE_PS4 && type != SDL_CONTROLLER_TYPE_PS5 && readButton(SDL_CONTROLLER_BUTTON_BACK);
    if (viewPressed) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::TouchPad);
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
    result.analogButtonsL2 = triggerValue(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    result.analogButtonsR2 = triggerValue(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    if (result.analogButtonsL2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::L2);
    if (result.analogButtonsR2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::R2);

    const auto stick = [this](SDL_GameControllerAxis xAxis, SDL_GameControllerAxis yAxis, int deadzonePercent) {
        return Pad::StickWithDeadzone(SDL_GameControllerGetAxis(controller, xAxis), SDL_GameControllerGetAxis(controller, yAxis), deadzonePercent);
    };
    const auto left = stick(SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, keyboardMouse.Configuration().settings.leftStickDeadzonePercent);
    const auto right = stick(SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY, keyboardMouse.Configuration().settings.rightStickDeadzonePercent);
    result.sticks = {left[0], left[1], right[0], right[1]};
    switch (SDL_GameControllerGetType(controller)) {
        case SDL_CONTROLLER_TYPE_PS5: result.deviceKind = 1; break;
        case SDL_CONTROLLER_TYPE_PS4: result.deviceKind = 2; break;
        default: result.deviceKind = 3; break;
    }
    if (SDL_GameControllerIsSensorEnabled(controller, SDL_SENSOR_ACCEL) == SDL_TRUE && SDL_GameControllerIsSensorEnabled(controller, SDL_SENSOR_GYRO) == SDL_TRUE) {
        float accel[3];
        float gyro[3];
        if (SDL_GameControllerGetSensorData(controller, SDL_SENSOR_ACCEL, accel, 3) == 0 && SDL_GameControllerGetSensorData(controller, SDL_SENSOR_GYRO, gyro, 3) == 0) {
            result.hasMotion = true;
            for (int i = 0; i < 3; ++i) { result.accel[i] = accel[i]; result.gyro[i] = gyro[i]; }
        }
    }
    if (SDL_GameControllerGetNumTouchpads(controller) > 0) {
        for (int finger = 0; finger < 2; ++finger) {
            Uint8 down = 0;
            float x = 0.0f;
            float y = 0.0f;
            float pressure = 0.0f;
            if (SDL_GameControllerGetTouchpadFinger(controller, 0, finger, &down, &x, &y, &pressure) != 0 || down == 0) continue;
            result.touch[finger].active = true;
            result.touch[finger].x = static_cast<std::uint16_t>(std::clamp(x, 0.0f, 1.0f) * (kTouchPadWidth - 1));
            result.touch[finger].y = static_cast<std::uint16_t>(std::clamp(y, 0.0f, 1.0f) * (kTouchPadHeight - 1));
        }
    }
    if (viewPressed && !result.touch[0].active && !result.touch[1].active) {
        result.touch[0] = {true, kTouchPadWidth / 2, kTouchPadHeight / 2, 0};
    }
    return result;
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
            publish(window.FrameNumber());
        }
        return;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP ||
        event.type == SDL_CONTROLLERAXISMOTION || event.type == SDL_CONTROLLERDEVICEREMAPPED) {
        publish(window.FrameNumber());
        return;
    }
    if (event.type == SDL_WINDOWEVENT) {
        if (keyboardMouse.HandleEvent(event, std::chrono::steady_clock::now()).releaseMouse) setMouseMode(false);
        publish(window.FrameNumber());
        return;
    }
    // APS5_NO_PAD_INPUT=1 keeps a measurement run from reacting to keys or mouse buttons that reach
    // its window (a stray press advances the title into another stage).
    static const bool ignoreInput = std::getenv("APS5_NO_PAD_INPUT") != nullptr;
    const bool input = event.type == SDL_MOUSEWHEEL || event.type == SDL_MOUSEMOTION || event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ||
        event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP;
    if (!input || ignoreInput) return;
    if (event.type == SDL_KEYDOWN && ImeCapturesKeyboard_nid_postfix()) return;
    keyboardMouse.SetTitleReadsMouse(MouseIsOpen_nid_no_patch());
    const auto requests = keyboardMouse.HandleEvent(event, std::chrono::steady_clock::now());
    if (requests.toggleFullscreen && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) window.ToggleFullscreen();
    if (requests.togglePerformanceOverlay && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) AgcDriverTogglePerformanceOverlay_nid_postfix();
    if (requests.screenshot && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) AgcDriverRequestScreenshot_nid_postfix();
    if (requests.toggleMouse) setMouseMode(!keyboardMouse.MouseMode());
    if (event.type != SDL_MOUSEMOTION) publish(window.FrameNumber());
}

void PadInput::Update(const DisplayWindow& window) {
    if (controller != nullptr) SDL_GameControllerUpdate();
    applyOutput();
    if (controller != nullptr) controllerState = sampleController();
    if (keyboardMouse.MouseMode() && SDL_GetKeyboardFocus() == nullptr) {
        keyboardMouse.Release();
        setMouseMode(false);
        publish(window.FrameNumber());
        return;
    }
    const bool changed = keyboardMouse.Advance(std::chrono::steady_clock::now());
    if (controller != nullptr || keyboardMouse.MouseMode() || changed) publish(window.FrameNumber());
}

void PadInput::publish(std::uint64_t frame) {
    const auto state = keyboardMouse.Compose(controllerState);
    PadPublishInput_nid_postfix(state);
    if (auto* telemetry = VideoOutTelemetry::Instance(); telemetry != nullptr) VideoOutTelemetry::EmitPadIfChanged(*telemetry, telemetryFilter, frame, state);
}

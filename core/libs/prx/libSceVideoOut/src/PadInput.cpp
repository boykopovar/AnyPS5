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
#include "prx/libSceVideoOut/include/PadStatusSdl.hpp"
#include "prx/libSceMouse/include/MouseState.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/PadStatusOverlay.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "prx/libScePad/include/PadInputTypes.hpp"
#include "prx/libScePad/include/PadRumbleIntensity.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/ImeInput.hpp"
#include "prx/libSceIme/include/ModalDialog.hpp"
#include "prx/libSceVideoOut/include/HostVolumeInput.hpp"
#include "prx/libScePad/include/HostVolumeSettings.hpp"

PadInput::PadInput() : PadInput(Pad::LoadInputMapping()) {
    HostVolumeConfigureFromEnvironment_nid_no_patch();
}

static_assert(PAD_MAX_CONTROLLERS == AgcDriver::PadStatusSlots, "the pad status panel lists one line per controller slot");

PadInput::PadInput(Pad::InputConfiguration configuration) : keyboardMouse(std::move(configuration)), statusProvider{[this] { return statusSnapshot(); }}, port0Mode(Port0Presence::ConfiguredMode()), port0(port0Mode, false) {
    static_cast<void>(Pad::RumblePercent());
    openAvailableControllers();
    port0 = Port0Presence::Presence(port0Mode, slots[0].controller != nullptr);
    if (port0Mode == Port0Presence::Mode::Controller) applyPort0(port0.Connected());
}

PadInput::~PadInput() {
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i) {
        closeController(i);
    }
}

void PadInput::openAvailableControllers() {
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
    }
}

void PadInput::openController(int deviceIndex) {
    if (!SDL_IsGameController(deviceIndex)) return;
    const auto instanceId = SDL_JoystickGetDeviceInstanceID(deviceIndex);
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i) {
        if (slots[i].controller != nullptr && slots[i].instanceId == instanceId) return;
    }
    int slot = -1;
    for (int i = 0; i < PAD_MAX_CONTROLLERS; ++i) {
        if (slots[i].controller == nullptr) {
            slot = i;
            break;
        }
    }
    if (slot == -1) return;
    SDL_GameController* gc = SDL_GameControllerOpen(deviceIndex);
    if (gc == nullptr) {
        APS5_LOG_ERR("Pad: could not open game controller %d: %s", deviceIndex, SDL_GetError());
        return;
    }
    slots[slot].controller = gc;
    slots[slot].instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
    slots[slot].state = {};
    slots[slot].outputSequence = 0;
    slots[slot].outputState = {};
    slots[slot].outputPending = true;
    slots[slot].nextRumbleRefresh = {};
    slots[slot].lightBar.Reset();
    slots[slot].triggerEffect.Reset();
    const char* name = SDL_GameControllerName(gc);
    slots[slot].kind = PadKindFromSdl(SDL_GameControllerGetType(gc));
    slots[slot].name = name != nullptr ? name : "";
    APS5_LOG_OUT("Pad: connected game controller (slot %d): %s (type %d, sensors accel=%d gyro=%d, touchpads=%d, led=%d, trigger rumble=%d)",
        slot, name != nullptr ? name : "unknown", static_cast<int>(SDL_GameControllerGetType(gc)),
        SDL_GameControllerHasSensor(gc, SDL_SENSOR_ACCEL) == SDL_TRUE, SDL_GameControllerHasSensor(gc, SDL_SENSOR_GYRO) == SDL_TRUE,
        SDL_GameControllerGetNumTouchpads(gc), SDL_GameControllerHasLED(gc) == SDL_TRUE, SDL_GameControllerHasRumbleTriggers(gc) == SDL_TRUE);
    enableSensors(slot);
    if (slot > 0) {
        PadSetPortConnected_nid_postfix(slot, true);
        UserServiceSetUserConnected_nid_postfix(slot, true);
    }
}

void PadInput::enableSensors(int slot) {
    if (slot < 0 || slot >= PAD_MAX_CONTROLLERS) return;
    SDL_GameController* controller = slots[slot].controller;
    if (controller == nullptr) return;
    const SDL_bool wanted = slots[slot].outputState.motionEnabled ? SDL_TRUE : SDL_FALSE;
    if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_ACCEL) == SDL_TRUE) SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_ACCEL, wanted);
    if (SDL_GameControllerHasSensor(controller, SDL_SENSOR_GYRO) == SDL_TRUE) SDL_GameControllerSetSensorEnabled(controller, SDL_SENSOR_GYRO, wanted);
}

void PadInput::closeController(int slot) {
    if (slot < 0 || slot >= PAD_MAX_CONTROLLERS) return;
    if (slots[slot].controller == nullptr) return;
    SDL_GameControllerClose(slots[slot].controller);
    slots[slot].controller = nullptr;
    slots[slot].instanceId = -1;
    slots[slot].kind = AgcDriver::PadKind::Unknown;
    slots[slot].name.clear();
    slots[slot].state = {};
    slots[slot].outputState = {};
    slots[slot].outputPending = false;
    if (slot > 0) {
        PadSetPortConnected_nid_postfix(slot, false);
        UserServiceSetUserConnected_nid_postfix(slot, false);
    }
}

AgcDriver::PadStatusSnapshot PadInput::statusSnapshot() const {
    AgcDriver::PadStatusSnapshot snapshot;
    for (int slot = 0; slot < PAD_MAX_CONTROLLERS; ++slot) {
        SDL_GameController* controller = slots[slot].controller;
        if (controller == nullptr) continue;
        auto& status = snapshot.slots[static_cast<std::size_t>(slot)];
        status.connected = true;
        status.kind = slots[slot].kind;
        status.name = slots[slot].name;
        status.power = PadPowerFromSdl(SDL_JoystickCurrentPowerLevel(SDL_GameControllerGetJoystick(controller)));
    }
    return snapshot;
}

void PadInput::reportStatus() {
    AgcDriverPadStatusPoll_nid_postfix(&statusProvider);
}

void PadInput::applyPort0(bool connected) {
    PadSetPortConnected_nid_postfix(0, connected);
    UserServiceSetUserConnected_nid_postfix(0, connected);
}

void PadInput::updatePort0() {
    if (const auto change = port0.Observe(slots[0].controller != nullptr, std::chrono::steady_clock::now())) applyPort0(*change);
}

void PadInput::applyOutput() {
    for (int slot = 0; slot < PAD_MAX_CONTROLLERS; ++slot) {
        PadOutputState fetched;
        if (PadFetchOutputForPort_nid_postfix(slot, &slots[slot].outputSequence, &fetched)) {
            const bool motionChanged = fetched.motionEnabled != slots[slot].outputState.motionEnabled;
            slots[slot].outputState = fetched;
            slots[slot].outputPending = true;
            if (motionChanged) enableSensors(slot);
        }
        SDL_GameController* controller = slots[slot].controller;
        if (controller == nullptr) continue;
        const auto now = std::chrono::steady_clock::now();
        const bool isPs5 = SDL_GameControllerGetType(controller) == SDL_CONTROLLER_TYPE_PS5;
        const auto large = std::max(slots[slot].outputState.vibrationLarge, slots[slot].outputState.audioVibrationLarge);
        const auto small = std::max(slots[slot].outputState.vibrationSmall, slots[slot].outputState.audioVibrationSmall);
        const unsigned rumblePercent = Pad::RumblePercent();
        const auto motors = Pad::MakeRumbleCommand(large, small, rumblePercent, 2000);
        const auto triggers = Pad::MakeRumbleCommand(slots[slot].outputState.trigger[0].fallback, slots[slot].outputState.trigger[1].fallback, rumblePercent, 2000);
        const bool rumbling = motors.Active();
        const bool triggerRumble = triggers.Active();
        if (SDL_GameControllerHasLED(controller) == SDL_TRUE) {
            struct PlayerLedColor { Uint8 r, g, b; };
            static constexpr PlayerLedColor defaultColors[4] = {
                {0, 64, 255},
                {255, 0, 0},
                {0, 255, 0},
                {255, 0, 128}
            };
            const auto& def = defaultColors[slot % 4];
            const LightBarPacer::Color color = slots[slot].outputState.lightBarValid
                ? LightBarPacer::Color{slots[slot].outputState.lightBar[0], slots[slot].outputState.lightBar[1], slots[slot].outputState.lightBar[2]}
                : LightBarPacer::Color{def.r, def.g, def.b};
            if (slots[slot].lightBar.Due(color, now)) slots[slot].lightBar.Result(color, now, SDL_GameControllerSetLED(controller, color[0], color[1], color[2]) == 0);
        }
        if (!slots[slot].outputPending) {
            if ((rumbling || (triggerRumble && !isPs5)) && now >= slots[slot].nextRumbleRefresh) slots[slot].outputPending = true;
            else continue;
        }
        slots[slot].outputPending = false;
        slots[slot].nextRumbleRefresh = now + std::chrono::milliseconds(700);
        SDL_GameControllerRumble(controller, motors.low, motors.high, motors.durationMs);
        if (slots[slot].outputState.triggerTouched) {
            if (isPs5) {
                TriggerEffectCache::Effect effect{};
                effect[0] = 0x04 | 0x08;
                std::memcpy(effect.data() + 10, slots[slot].outputState.trigger[1].effect, 11);
                std::memcpy(effect.data() + 21, slots[slot].outputState.trigger[0].effect, 11);
                if (slots[slot].triggerEffect.NeedsSend(effect)) {
                    slots[slot].triggerEffect.Result(effect, SDL_GameControllerSendEffect(controller, effect.data(), static_cast<int>(effect.size())) == 0);
                }
            } else if (SDL_GameControllerHasRumbleTriggers(controller) == SDL_TRUE) {
                SDL_GameControllerRumbleTriggers(controller, triggers.low, triggers.high, triggers.durationMs);
            }
        }
    }
}

void PadInput::setMouseMode(bool enabled) {
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_MODE_WARP, "0");
    SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, "0");
    if (SDL_SetRelativeMouseMode(enabled ? SDL_TRUE : SDL_FALSE) != 0) throw std::runtime_error(std::string("Pad: relative mouse mode failed: ") + SDL_GetError());
    keyboardMouse.SetMouseMode(enabled);
}

PadInputState PadInput::sampleController(int slot) const {
    PadInputState result;
    if (slot < 0 || slot >= PAD_MAX_CONTROLLERS) return result;
    SDL_GameController* controller = slots[slot].controller;
    if (controller == nullptr) return result;
    const auto readButton = [controller](SDL_GameControllerButton button) {
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

    const auto triggerValue = [controller](SDL_GameControllerAxis axis) {
        const auto value = std::clamp<int>(SDL_GameControllerGetAxis(controller, axis), 0, 32767);
        return static_cast<std::uint8_t>((value * 255 + 16383) / 32767);
    };
    result.analogButtonsL2 = triggerValue(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
    result.analogButtonsR2 = triggerValue(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
    if (result.analogButtonsL2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::L2);
    if (result.analogButtonsR2 != 0) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::R2);

    const auto stick = [this, controller](SDL_GameControllerAxis xAxis, SDL_GameControllerAxis yAxis, int deadzonePercent) {
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
    if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
        for (int slot = 0; slot < PAD_MAX_CONTROLLERS; ++slot) {
            if (slots[slot].controller != nullptr && slots[slot].instanceId == event.cdevice.which) {
                closeController(slot);
                publish(window.FrameNumber(), slot);
                break;
            }
        }
        return;
    }
    if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP ||
        event.type == SDL_CONTROLLERAXISMOTION || event.type == SDL_CONTROLLERDEVICEREMAPPED) {
        SDL_JoystickID which = -1;
        if (event.type == SDL_CONTROLLERBUTTONDOWN || event.type == SDL_CONTROLLERBUTTONUP) which = event.cbutton.which;
        else if (event.type == SDL_CONTROLLERAXISMOTION) which = event.caxis.which;
        else if (event.type == SDL_CONTROLLERDEVICEREMAPPED) which = event.cdevice.which;
        for (int slot = 0; slot < PAD_MAX_CONTROLLERS; ++slot) {
            if (slots[slot].controller != nullptr && slots[slot].instanceId == which) {
                slots[slot].state = sampleController(slot);
                publish(window.FrameNumber(), slot);
                return;
            }
        }
        return;
    }
    if (event.type == SDL_WINDOWEVENT) {
        if (keyboardMouse.HandleEvent(event, std::chrono::steady_clock::now()).releaseMouse) setMouseMode(false);
        HostVolume volume;
        if (window.Handle() != nullptr && ApplyHostFocusEvent(event, SDL_GetWindowID(window.Handle()), &volume)) APS5_LOG_ERR("%s", HostVolumeMessage(volume).c_str());
        publish(window.FrameNumber(), 0);
        return;
    }
    // APS5_NO_PAD_INPUT=1 keeps a measurement run from reacting to keys or mouse buttons that reach
    // its window (a stray press advances the title into another stage).
    static const bool ignoreInput = std::getenv("APS5_NO_PAD_INPUT") != nullptr;
    const bool input = event.type == SDL_MOUSEWHEEL || event.type == SDL_MOUSEMOTION || event.type == SDL_KEYDOWN || event.type == SDL_KEYUP ||
        event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP;
    if (!input || ignoreInput) return;
    if (event.type == SDL_KEYDOWN && ImeCapturesKeyboard_nid_postfix()) return;
    if (event.type != SDL_KEYUP && event.type != SDL_MOUSEBUTTONUP && ModalDialogCapturesInput_nid_postfix()) return;
    keyboardMouse.SetTitleReadsMouse(MouseIsOpen_nid_no_patch());
    const auto requests = keyboardMouse.HandleEvent(event, std::chrono::steady_clock::now());
    if (requests.toggleFullscreen && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) window.ToggleFullscreen();
    if (requests.togglePerformanceOverlay && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) AgcDriverTogglePerformanceOverlay_nid_postfix();
    if (requests.screenshot && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) AgcDriverRequestScreenshot_nid_postfix();
    if (requests.showPadStatus && window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) {
        AgcDriverPadStatusShow_nid_postfix();
        reportStatus();
    }
    if (window.Handle() != nullptr && event.key.windowID == SDL_GetWindowID(window.Handle())) {
        HostVolume volume;
        if (ApplyHostVolumeRequests(requests, &volume)) APS5_LOG_ERR("%s", HostVolumeMessage(volume).c_str());
    }
    if (requests.toggleMouse) setMouseMode(!keyboardMouse.MouseMode());
    if (event.type != SDL_MOUSEMOTION) publish(window.FrameNumber(), 0);
}

void PadInput::Update(const DisplayWindow& window) {
    if (!hostFocusSynced && window.Handle() != nullptr) {
        hostFocusSynced = true;
        HostVolume volume;
        if (HostVolumeFocus_nid_no_patch(SDL_GetKeyboardFocus() == window.Handle(), &volume)) APS5_LOG_ERR("%s", HostVolumeMessage(volume).c_str());
    }
    bool anyController = false;
    for (int slot = 0; slot < PAD_MAX_CONTROLLERS; ++slot) {
        if (slots[slot].controller != nullptr) {
            anyController = true;
            break;
        }
    }
    if (anyController) SDL_GameControllerUpdate();
    updatePort0();
    reportStatus();
    applyOutput();
    for (int slot = 0; slot < PAD_MAX_CONTROLLERS; ++slot) {
        if (slots[slot].controller != nullptr) {
            slots[slot].state = sampleController(slot);
        }
    }
    if (keyboardMouse.MouseMode() && SDL_GetKeyboardFocus() == nullptr) {
        keyboardMouse.Release();
        setMouseMode(false);
        publish(window.FrameNumber(), 0);
        return;
    }
    const bool changed = keyboardMouse.Advance(std::chrono::steady_clock::now());
    const bool captured = ImeCapturesPad_nid_postfix() || ModalDialogCapturesInput_nid_postfix();
    const bool captureChanged = std::exchange(padCaptured, captured) != captured;
    if (slots[0].controller != nullptr || keyboardMouse.MouseMode() || changed || captured || captureChanged) {
        publish(window.FrameNumber(), 0);
    }
    for (int slot = 1; slot < PAD_MAX_CONTROLLERS; ++slot) {
        if (slots[slot].controller != nullptr) {
            publish(window.FrameNumber(), slot);
        }
    }
}

void PadInput::publish(std::uint64_t frame) {
    for (int slot = 0; slot < PAD_MAX_CONTROLLERS; ++slot) {
        publish(frame, slot);
    }
}

PadInputState PadInput::handOff(int slot, PadInputState state) {
    auto& held = slots[slot].heldThroughCapture;
    const bool imeCaptures = ImeCapturesPad_nid_postfix();
    if (imeCaptures || ModalDialogCapturesInput_nid_postfix()) {
        if (slot == 0) PadApplyInjectedInput_nid_postfix(&state);
        if (slot == 0 && imeCaptures) ImePublishPad_nid_postfix({state.buttons, state.sticks[0], state.sticks[1], state.analogButtonsL2});
        else if (slot == 0) ModalDialogPublishPad_nid_postfix({state.buttons, state.sticks[0], state.sticks[1], state.analogButtonsL2});
        held = state.buttons;
        PadInputState neutral;
        neutral.hasMotion = false;
        neutral.deviceKind = state.deviceKind;
        neutral.intercepted = true;
        return neutral;
    }
    held &= state.buttons;
    state.buttons &= ~held;
    if ((held & static_cast<std::uint32_t>(Pad::PadButton::L2)) != 0) state.analogButtonsL2 = 0;
    if ((held & static_cast<std::uint32_t>(Pad::PadButton::R2)) != 0) state.analogButtonsR2 = 0;
    return state;
}

void PadInput::publish(std::uint64_t frame, int slot) {
    if (slot < 0 || slot >= PAD_MAX_CONTROLLERS) return;
    if (slot == 0) {
        const auto composed = port0.Connected() ? keyboardMouse.Compose(slots[0].state) : PadInputState{};
        const auto state = handOff(0, composed);
        PadPublishInput_nid_postfix(state);
        if (auto* telemetry = VideoOutTelemetry::Instance(); telemetry != nullptr) VideoOutTelemetry::EmitPadIfChanged(*telemetry, telemetryFilter, frame, state);
    } else {
        PadPublishInputForPort_nid_postfix(slot, handOff(slot, slots[slot].state));
    }
}

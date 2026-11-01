#include "prx/libScePad/include/InputMapping.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path root;

void setConfigPath(const std::filesystem::path& path) {
#ifdef _WIN32
    require(_wputenv_s(L"ANYPS5_INPUT_CONFIG", path.wstring().c_str()) == 0, "set ANYPS5_INPUT_CONFIG");
#else
    require(::setenv("ANYPS5_INPUT_CONFIG", path.string().c_str(), 1) == 0, "set ANYPS5_INPUT_CONFIG");
#endif
}

void useConfig(const std::string& text) {
    const auto path = root / "anyps5-input.ini";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
        require(file.good(), "write the input mapping");
    }
    setConfigPath(path);
}

bool rejects(const std::string& text) {
    useConfig(text);
    try {
        Pad::LoadInputMapping();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

std::vector<SDL_Scancode> keysFor(Pad::InputControl control) {
    std::vector<SDL_Scancode> keys;
    for (const auto& binding : Pad::LoadInputMapping().bindings) {
        if (binding.control == control && binding.key != SDL_SCANCODE_UNKNOWN) keys.push_back(binding.key);
    }
    return keys;
}

bool hasKey(const std::vector<SDL_Scancode>& keys, SDL_Scancode key) {
    return std::find(keys.begin(), keys.end(), key) != keys.end();
}

std::uint8_t plainStickByte(std::int16_t axis) {
    return static_cast<std::uint8_t>(((static_cast<std::int32_t>(axis) + 32768) * 255 + 32767) / 65535);
}

void settingsDefault() {
    useConfig("Cross = KEY:F\n");
    const auto configuration = Pad::LoadInputMapping();
    require(configuration.settings.leftStickDeadzonePercent == 0 && configuration.settings.rightStickDeadzonePercent == 0, "deadzones default to 0");
    require(configuration.settings.mouseSensitivity == Pad::DefaultMouseSensitivity, "mouse sensitivity defaults to 1");
}

void settingsParse() {
    useConfig("leftstickdeadzone = 12 ; worn stick\nRightStickDeadzone=5\nMouseSensitivity = 2.5\nCross = KEY:F\n");
    const auto configuration = Pad::LoadInputMapping();
    require(configuration.settings.leftStickDeadzonePercent == 12, "left deadzone is read case-insensitively");
    require(configuration.settings.rightStickDeadzonePercent == 5, "right deadzone is read");
    require(configuration.settings.mouseSensitivity == 2.5, "mouse sensitivity is read");
    bool crossOnF = false;
    for (const auto& binding : configuration.bindings) crossOnF = crossOnF || (binding.control == Pad::InputControl::Button && binding.button == Pad::PadButton::Cross && binding.key == SDL_SCANCODE_F);
    require(crossOnF, "bindings are read alongside settings");
}

void settingsReject() {
    require(rejects("LeftStickDeadzone = 91\n"), "a deadzone above 90 is rejected");
    require(rejects("LeftStickDeadzone = -1\n"), "a negative deadzone is rejected");
    require(rejects("LeftStickDeadzone = 7.5\n"), "a fractional deadzone is rejected");
    require(rejects("RightStickDeadzone =\n"), "an empty deadzone is rejected");
    require(rejects("MouseSensitivity = 0\n"), "a zero mouse sensitivity is rejected");
    require(rejects("MouseSensitivity = 10.5\n"), "a mouse sensitivity above 10 is rejected");
    require(rejects("MouseSensitivity = fast\n"), "a non-numeric mouse sensitivity is rejected");
    require(rejects("MouseSensitivity = 1\nMouseSensitivity = 2\n"), "a setting given twice is rejected");
}

void performanceOverlayBinding() {
    useConfig("Cross = KEY:F\n");
    bool onF10 = false;
    for (const auto& binding : Pad::LoadInputMapping().bindings) onF10 = onF10 || (binding.control == Pad::InputControl::TogglePerformanceOverlay && binding.key == SDL_SCANCODE_F10);
    require(onF10, "F10 toggles the performance overlay by default");
    useConfig("togglePerformanceOverlay = KEY:F9\n");
    bool onF9 = false;
    bool stillOnF10 = false;
    for (const auto& binding : Pad::LoadInputMapping().bindings) {
        if (binding.control != Pad::InputControl::TogglePerformanceOverlay) continue;
        onF9 = onF9 || binding.key == SDL_SCANCODE_F9;
        stillOnF10 = stillOnF10 || binding.key == SDL_SCANCODE_F10;
    }
    require(onF9 && !stillOnF10, "the performance overlay toggle moves to another key");
    require(rejects("TogglePerformanceOverlay = MOUSE:Left\n"), "the performance overlay toggle refuses a mouse button");
}

void screenshotBinding() {
    useConfig("Cross = KEY:F\n");
    bool onF12 = false;
    for (const auto& binding : Pad::LoadInputMapping().bindings) onF12 = onF12 || (binding.control == Pad::InputControl::Screenshot && binding.key == SDL_SCANCODE_F12);
    require(onF12, "F12 takes a screenshot by default");
    useConfig("Screenshot = KEY:F9\n");
    bool onF9 = false;
    bool stillOnF12 = false;
    for (const auto& binding : Pad::LoadInputMapping().bindings) {
        if (binding.control != Pad::InputControl::Screenshot) continue;
        onF9 = onF9 || binding.key == SDL_SCANCODE_F9;
        stillOnF12 = stillOnF12 || binding.key == SDL_SCANCODE_F12;
    }
    require(onF9 && !stillOnF12, "the screenshot key moves to another key");
    require(rejects("Screenshot = MOUSE:Left\n"), "the screenshot key refuses a mouse button");
    require(rejects("Screenshot = WHEEL:Up\n"), "the screenshot key refuses the mouse wheel");
}

std::size_t padStatusBindingsOn(const Pad::InputConfiguration& configuration, SDL_Scancode key) {
    std::size_t count = 0;
    for (const auto& binding : configuration.bindings) count += binding.control == Pad::InputControl::ShowPadStatus && binding.key == key ? 1 : 0;
    return count;
}

void padStatusBinding() {
    useConfig("Cross = KEY:F\n");
    const auto defaults = Pad::LoadInputMapping();
    require(padStatusBindingsOn(defaults, SDL_SCANCODE_F9) == 1, "F9 shows the pad status by default");
    std::size_t padStatus = 0;
    std::size_t onF9 = 0;
    for (const auto& binding : defaults.bindings) {
        padStatus += binding.control == Pad::InputControl::ShowPadStatus ? 1 : 0;
        onF9 += binding.key == SDL_SCANCODE_F9 ? 1 : 0;
    }
    require(padStatus == 1, "the pad status has more than one default input");
    require(onF9 == 1, "another default binding uses F9");
    require(Pad::InputMapping.size() == 43, "the default bindings changed besides the pad status and host volume keys");
    useConfig("ShowPadStatus = KEY:F4\n");
    const auto moved = Pad::LoadInputMapping();
    require(padStatusBindingsOn(moved, SDL_SCANCODE_F4) == 1 && padStatusBindingsOn(moved, SDL_SCANCODE_F9) == 0, "the pad status key moves to another key");
    useConfig("showpadstatus = key:f4\nSHOWPADSTATUS = KEY:F3\nShowPadStatus = KEY:F4\n");
    const auto alternates = Pad::LoadInputMapping();
    require(padStatusBindingsOn(alternates, SDL_SCANCODE_F4) == 1 && padStatusBindingsOn(alternates, SDL_SCANCODE_F3) == 1 && padStatusBindingsOn(alternates, SDL_SCANCODE_F9) == 0, "the pad status keeps alternate keys, once each, whatever the case");
    require(rejects("ShowPadStatus = MOUSE:Left\n"), "the pad status key refuses a mouse button");
    require(rejects("ShowPadStatus = MOUSE:X2\n"), "the pad status key refuses another mouse button");
    require(rejects("ShowPadStatus = WHEEL:Up\n"), "the pad status key refuses the mouse wheel");
    require(rejects("ShowPadStatus = KEY:NoSuchKey\n"), "the pad status key refuses an unknown key");
    require(rejects("ShowPadStatus =\n"), "the pad status key refuses an empty source");
}

void padStatusConflicts() {
    useConfig("ShowPadStatus = KEY:F10\n");
    const auto sharedKey = Pad::LoadInputMapping();
    std::size_t overlayOnF10 = 0;
    for (const auto& binding : sharedKey.bindings) overlayOnF10 += binding.control == Pad::InputControl::TogglePerformanceOverlay && binding.key == SDL_SCANCODE_F10 ? 1 : 0;
    require(padStatusBindingsOn(sharedKey, SDL_SCANCODE_F10) == 1 && overlayOnF10 == 1, "the pad status and the overlay cannot share a key");
    useConfig("Cross = KEY:F9\n");
    const auto taken = Pad::LoadInputMapping();
    bool crossOnF9 = false;
    for (const auto& binding : taken.bindings) crossOnF9 = crossOnF9 || (binding.control == Pad::InputControl::Button && binding.button == Pad::PadButton::Cross && binding.key == SDL_SCANCODE_F9);
    require(crossOnF9 && padStatusBindingsOn(taken, SDL_SCANCODE_F9) == 1, "a pad button on F9 removes the pad status key");
    useConfig("TogglePerformanceOverlay = KEY:F9\n");
    const auto overlayOnPadStatusKey = Pad::LoadInputMapping();
    bool overlayOnF9 = false;
    for (const auto& binding : overlayOnPadStatusKey.bindings) overlayOnF9 = overlayOnF9 || (binding.control == Pad::InputControl::TogglePerformanceOverlay && binding.key == SDL_SCANCODE_F9);
    require(overlayOnF9 && padStatusBindingsOn(overlayOnPadStatusKey, SDL_SCANCODE_F9) == 1, "the overlay on F9 removes the pad status key");
    useConfig("ShowPadStatus = KEY:F4\nTogglePerformanceOverlay = KEY:F9\n");
    const auto swapped = Pad::LoadInputMapping();
    require(padStatusBindingsOn(swapped, SDL_SCANCODE_F4) == 1 && padStatusBindingsOn(swapped, SDL_SCANCODE_F9) == 0, "moving the pad status key off F9 keeps it off");
}

void volumeUpAction() {
    useConfig("volumeup = KEY:F2\nVolumeUp = KEY:F3\n");
    const auto keys = keysFor(Pad::InputControl::VolumeUp);
    require(keys.size() == 2 && hasKey(keys, SDL_SCANCODE_F2) && hasKey(keys, SDL_SCANCODE_F3), "VolumeUp takes a first key and an alternate, case-insensitively");
    useConfig("VolumeUp = KEY:F2\nVolumeUp = KEY:F2\n");
    const auto duplicated = keysFor(Pad::InputControl::VolumeUp);
    require(duplicated.size() == 1 && duplicated[0] == SDL_SCANCODE_F2, "a duplicate VolumeUp line is ignored");
    require(rejects("VolumeUp = MOUSE:Left\n"), "VolumeUp refuses a mouse button");
    require(rejects("VolumeUp = MOUSE:X1\n"), "VolumeUp refuses another mouse button");
    require(rejects("VolumeUp = WHEEL:Up\n"), "VolumeUp refuses the mouse wheel");
    require(rejects("VolumeUp = KEY:NoSuchKey\n"), "VolumeUp refuses an unknown key name");
    require(rejects("VolumeUp =\n"), "VolumeUp refuses an empty source");
    require(rejects("VolumeUp = F2\n"), "VolumeUp refuses a source without a type");
}

void volumeDownAction() {
    useConfig("volumedown = KEY:F2\nVolumeDown = KEY:F3\n");
    const auto keys = keysFor(Pad::InputControl::VolumeDown);
    require(keys.size() == 2 && hasKey(keys, SDL_SCANCODE_F2) && hasKey(keys, SDL_SCANCODE_F3), "VolumeDown takes a first key and an alternate, case-insensitively");
    useConfig("VolumeDown = KEY:F2\nVolumeDown = KEY:F2\n");
    const auto duplicated = keysFor(Pad::InputControl::VolumeDown);
    require(duplicated.size() == 1 && duplicated[0] == SDL_SCANCODE_F2, "a duplicate VolumeDown line is ignored");
    require(rejects("VolumeDown = MOUSE:Left\n"), "VolumeDown refuses a mouse button");
    require(rejects("VolumeDown = MOUSE:X1\n"), "VolumeDown refuses another mouse button");
    require(rejects("VolumeDown = WHEEL:Up\n"), "VolumeDown refuses the mouse wheel");
    require(rejects("VolumeDown = KEY:NoSuchKey\n"), "VolumeDown refuses an unknown key name");
    require(rejects("VolumeDown =\n"), "VolumeDown refuses an empty source");
    require(rejects("VolumeDown = F2\n"), "VolumeDown refuses a source without a type");
}

void toggleMuteAction() {
    useConfig("togglemute = KEY:F2\nToggleMute = KEY:F3\n");
    const auto keys = keysFor(Pad::InputControl::ToggleMute);
    require(keys.size() == 2 && hasKey(keys, SDL_SCANCODE_F2) && hasKey(keys, SDL_SCANCODE_F3), "ToggleMute takes a first key and an alternate, case-insensitively");
    useConfig("ToggleMute = KEY:F2\nToggleMute = KEY:F2\n");
    const auto duplicated = keysFor(Pad::InputControl::ToggleMute);
    require(duplicated.size() == 1 && duplicated[0] == SDL_SCANCODE_F2, "a duplicate ToggleMute line is ignored");
    require(rejects("ToggleMute = MOUSE:Left\n"), "ToggleMute refuses a mouse button");
    require(rejects("ToggleMute = MOUSE:X1\n"), "ToggleMute refuses another mouse button");
    require(rejects("ToggleMute = WHEEL:Up\n"), "ToggleMute refuses the mouse wheel");
    require(rejects("ToggleMute = KEY:NoSuchKey\n"), "ToggleMute refuses an unknown key name");
    require(rejects("ToggleMute =\n"), "ToggleMute refuses an empty source");
    require(rejects("ToggleMute = F2\n"), "ToggleMute refuses a source without a type");
}

void volumeDownDefault() {
    useConfig("Cross = KEY:F\n");
    const auto keys = keysFor(Pad::InputControl::VolumeDown);
    require(keys.size() == 1 && keys[0] == SDL_SCANCODE_F5, "F5 is the only default VolumeDown key");
    useConfig("VolumeDown = KEY:F2\n");
    const auto moved = keysFor(Pad::InputControl::VolumeDown);
    require(moved.size() == 1 && moved[0] == SDL_SCANCODE_F2, "a VolumeDown line replaces the built-in key");
}

void volumeUpDefault() {
    useConfig("Cross = KEY:F\n");
    const auto keys = keysFor(Pad::InputControl::VolumeUp);
    require(keys.size() == 1 && keys[0] == SDL_SCANCODE_F6, "F6 is the only default VolumeUp key");
    useConfig("VolumeUp = KEY:F2\n");
    const auto moved = keysFor(Pad::InputControl::VolumeUp);
    require(moved.size() == 1 && moved[0] == SDL_SCANCODE_F2, "a VolumeUp line replaces the built-in key");
}

void toggleMuteDefault() {
    useConfig("Cross = KEY:F\n");
    const auto keys = keysFor(Pad::InputControl::ToggleMute);
    require(keys.size() == 1 && keys[0] == SDL_SCANCODE_F8, "F8 is the only default ToggleMute key");
    useConfig("ToggleMute = KEY:F2\n");
    const auto moved = keysFor(Pad::InputControl::ToggleMute);
    require(moved.size() == 1 && moved[0] == SDL_SCANCODE_F2, "a ToggleMute line replaces the built-in key");
}

void hostVolumeDefaultsDoNotCollide() {
    useConfig("MouseSensitivity = 1\n");
    std::vector<SDL_Scancode> seen;
    for (const auto& binding : Pad::LoadInputMapping().bindings) {
        if (binding.key == SDL_SCANCODE_UNKNOWN) continue;
        require(!hasKey(seen, binding.key), "no built-in key is bound twice");
        seen.push_back(binding.key);
    }
    for (const auto key : {SDL_SCANCODE_F5, SDL_SCANCODE_F6, SDL_SCANCODE_F8}) require(hasKey(seen, key), "every host volume key is bound by default");
    for (const auto control : {Pad::InputControl::VolumeUp, Pad::InputControl::VolumeDown, Pad::InputControl::ToggleMute}) {
        for (const auto& binding : Pad::LoadInputMapping().bindings) {
            if (binding.control != control) continue;
            require(binding.button == Pad::PadButton::None && binding.mouseButton == Pad::MouseButton::None && binding.wheelDirection == 0, "a host volume binding is a plain keyboard key");
        }
    }
}

void hostVolumeSharedKeysAreKept() {
    useConfig("VolumeUp = KEY:F10\nToggleMute = KEY:Return\n");
    const auto configuration = Pad::LoadInputMapping();
    bool upOnF10 = false;
    bool overlayOnF10 = false;
    bool muteOnReturn = false;
    bool crossOnReturn = false;
    for (const auto& binding : configuration.bindings) {
        upOnF10 = upOnF10 || (binding.control == Pad::InputControl::VolumeUp && binding.key == SDL_SCANCODE_F10);
        overlayOnF10 = overlayOnF10 || (binding.control == Pad::InputControl::TogglePerformanceOverlay && binding.key == SDL_SCANCODE_F10);
        muteOnReturn = muteOnReturn || (binding.control == Pad::InputControl::ToggleMute && binding.key == SDL_SCANCODE_RETURN);
        crossOnReturn = crossOnReturn || (binding.control == Pad::InputControl::Button && binding.button == Pad::PadButton::Cross && binding.key == SDL_SCANCODE_RETURN);
    }
    require(upOnF10 && overlayOnF10, "a key shared with another host action keeps both bindings, as for the existing actions");
    require(muteOnReturn && crossOnReturn, "a key shared with a pad button keeps both bindings, as for the existing actions");
}

void hostVolumeActionNamesAreExact() {
    require(rejects("VolumeMute = KEY:F2\n"), "an unknown volume action name is rejected");
    require(rejects("Volume = KEY:F2\n"), "a bare Volume action is rejected");
    require(rejects("Mute = KEY:F2\n"), "a bare Mute action is rejected");
    require(rejects("VolumeUp2 = KEY:F2\n"), "a suffixed volume action name is rejected");
    useConfig("  VOLUMEUP\t=\tKEY:F2 # louder\nvolumedown=KEY:F3;quieter\n");
    require(!keysFor(Pad::InputControl::VolumeUp).empty() && !keysFor(Pad::InputControl::VolumeDown).empty(), "names ignore case, spacing and trailing comments");
}

void mouseLookDefault() {
    useConfig("Cross = KEY:F\n");
    const auto settings = Pad::LoadInputMapping().settings;
    require(settings.mouseVerticalScale == 1.0 && settings.mouseCurve == 1.0 && settings.mouseMaxSpeed == 1.0, "mouse look scales default to 1");
    require(settings.mouseDeadzoneOffset == 0.2 && settings.mouseSmoothingMs == 10.0 && settings.mouseDecayMs == 30.0, "mouse look timing and offset defaults");
    require(settings.mouseControlsRightStick, "mouse look targets the right stick by default");
}

void mouseLookParse() {
    useConfig("MouseCurve=0.7 ; softer start\nmousedeadzoneoffset = 0.15\nMouseMaxSpeed = 1.5\nMouseSmoothingMs = 0\nMouseDecayMs = 50\nMouseVerticalScale = 0.5\nMouseStickTarget = Left\n");
    const auto settings = Pad::LoadInputMapping().settings;
    require(settings.mouseCurve == 0.7, "mouse curve is read");
    require(settings.mouseDeadzoneOffset == 0.15, "mouse deadzone offset is read case-insensitively");
    require(settings.mouseMaxSpeed == 1.5, "mouse max speed is read");
    require(settings.mouseSmoothingMs == 0.0, "a 0 smoothing is accepted");
    require(settings.mouseDecayMs == 50.0, "mouse decay is read");
    require(settings.mouseVerticalScale == 0.5, "mouse vertical scale is read");
    require(!settings.mouseControlsRightStick, "mouse stick target is read");
    useConfig("mousesticktarget = RIGHT\n");
    require(Pad::LoadInputMapping().settings.mouseControlsRightStick, "mouse stick target is read case-insensitively");
}

void mouseLookReject() {
    require(rejects("MouseCurve = 0.1\n"), "a mouse curve below 0.2 is rejected");
    require(rejects("MouseCurve = steep\n"), "a non-numeric mouse curve is rejected");
    require(rejects("MouseDeadzoneOffset = 0.95\n"), "a deadzone offset above 0.9 is rejected");
    require(rejects("MouseDecayMs =\n"), "an empty decay is rejected");
    require(rejects("MouseDecayMs = 0\n"), "a zero decay is rejected");
    require(rejects("MouseSmoothingMs = 501\n"), "a smoothing above 500 is rejected");
    require(rejects("MouseMaxSpeed = nan\n"), "a non-finite max speed is rejected");
    require(rejects("MouseVerticalScale = 11\n"), "a vertical scale above 10 is rejected");
    require(rejects("MouseDecayMs = 20\nmousedecayms = 30\n"), "a mouse setting given twice is rejected");
    require(rejects("MouseStickTarget = Center\n"), "an invalid stick target is rejected");
    require(rejects("MouseStickTarget =\n"), "an empty mouse stick target is rejected");
    require(rejects("MouseStickTarget = Left\nMouseStickTarget = Right\n"), "a mouse stick target given twice is rejected");
}

void nonAsciiPaths() {
    const std::u8string name = u8"anyps5-input-\u00fc-\u65e5\u672c-" + std::u8string(root.filename().u8string());
    const auto directory = root / std::filesystem::path(name);
    std::filesystem::create_directories(directory);
    const auto file = directory / "anyps5-input.ini";
    {
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream << "MouseSensitivity = 3\n";
        require(stream.good(), "write the input mapping in a non-ASCII directory");
    }
    setConfigPath(file);
    require(Pad::LoadInputMapping().settings.mouseSensitivity == 3.0, "ANYPS5_INPUT_CONFIG may name a path with non-ASCII characters");
    {
        std::ofstream stream(file, std::ios::binary | std::ios::trunc);
        stream << "MouseSensitivity = fast\n";
    }
    std::string message;
    try {
        Pad::LoadInputMapping();
    } catch (const std::runtime_error& error) {
        message = error.what();
    }
    const auto utf8Name = std::string(reinterpret_cast<const char*>(name.data()), name.size());
    require(message.find(utf8Name) != std::string::npos, "an error names the path in UTF-8");
    setConfigPath(directory / "missing.ini");
    message.clear();
    try {
        Pad::LoadInputMapping();
    } catch (const std::runtime_error& error) {
        message = error.what();
    }
    require(message.find(utf8Name) != std::string::npos, "a missing file names the path in UTF-8");
}

std::size_t stickModifierBindings(const Pad::InputConfiguration& configuration) {
    std::size_t count = 0;
    for (const auto& binding : configuration.bindings) count += binding.control == Pad::InputControl::StickModifier ? 1 : 0;
    return count;
}

void stickModifier() {
    useConfig("Cross = KEY:F\n");
    auto configuration = Pad::LoadInputMapping();
    require(stickModifierBindings(configuration) == 0, "no key is a stick modifier by default");
    require(configuration.settings.stickModifierScale == 0.5, "the stick modifier scale defaults to 0.5");
    useConfig("StickModifier = KEY:Z\nstickmodifier = MOUSE:X2\nStickModifierScale = 0.35\n");
    configuration = Pad::LoadInputMapping();
    require(stickModifierBindings(configuration) == 2, "StickModifier takes a key and a mouse button");
    require(configuration.settings.stickModifierScale == 0.35, "the stick modifier scale is read");
    require(rejects("StickModifier = WHEEL:Up\n"), "a wheel direction cannot be a stick modifier");
    require(rejects("StickModifierScale = 0.04\n"), "a stick modifier scale below 0.05 is rejected");
    require(rejects("StickModifierScale = 1.5\n"), "a stick modifier scale above 1 is rejected");
    require(rejects("StickModifierScale = half\n"), "a non-numeric stick modifier scale is rejected");
    require(rejects("StickModifierScale = 0.5\nStickModifierScale = 0.6\n"), "a stick modifier scale given twice is rejected");
}

void keyboardRamp() {
    useConfig("Cross = KEY:F\n");
    require(Pad::LoadInputMapping().settings.keyboardRampMs == 0.0, "the keyboard ramp defaults to 0");
    useConfig("KeyboardRampMs = 120\n");
    require(Pad::LoadInputMapping().settings.keyboardRampMs == 120.0, "the keyboard ramp is read");
    useConfig("keyboardrampms = 0\n");
    require(Pad::LoadInputMapping().settings.keyboardRampMs == 0.0, "a ramp of 0 is accepted");
    require(rejects("KeyboardRampMs = -1\n"), "a negative keyboard ramp is rejected");
    require(rejects("KeyboardRampMs = 2001\n"), "a keyboard ramp above 2000 is rejected");
    require(rejects("KeyboardRampMs = slow\n"), "a non-numeric keyboard ramp is rejected");
    require(rejects("KeyboardRampMs = 50\nKeyboardRampMs = 60\n"), "a keyboard ramp given twice is rejected");
}

void triggerRamp() {
    useConfig("Cross = KEY:F\n");
    require(Pad::LoadInputMapping().settings.triggerRampMs == 0.0, "the trigger ramp defaults to 0");
    useConfig("TriggerRampMs = 80\n");
    require(Pad::LoadInputMapping().settings.triggerRampMs == 80.0, "the trigger ramp is read");
    require(rejects("TriggerRampMs = -5\n"), "a negative trigger ramp is rejected");
    require(rejects("TriggerRampMs = 2500\n"), "a trigger ramp above 2000 is rejected");
    require(rejects("TriggerRampMs = 10\ntriggerrampms = 20\n"), "a trigger ramp given twice is rejected");
}

std::size_t toggleMouseBindings(const Pad::InputConfiguration& configuration, SDL_Scancode key, Pad::MouseButton button) {
    std::size_t count = 0;
    for (const auto& binding : configuration.bindings) {
        if (binding.control == Pad::InputControl::ToggleMouse && binding.key == key && binding.mouseButton == button) ++count;
    }
    return count;
}

void toggleMouseDefaults() {
    useConfig("Cross = KEY:F\n");
    auto configuration = Pad::LoadInputMapping();
    require(toggleMouseBindings(configuration, SDL_SCANCODE_F7, Pad::MouseButton::None) == 1, "F7 toggles the mouse by default");
    require(toggleMouseBindings(configuration, SDL_SCANCODE_UNKNOWN, Pad::MouseButton::Middle) == 1, "the middle button toggles the mouse by default");
    useConfig("ToggleMouse = MOUSE:Right\n");
    configuration = Pad::LoadInputMapping();
    require(toggleMouseBindings(configuration, SDL_SCANCODE_F7, Pad::MouseButton::None) == 0, "a ToggleMouse line replaces F7");
    require(toggleMouseBindings(configuration, SDL_SCANCODE_UNKNOWN, Pad::MouseButton::Right) == 1, "a ToggleMouse line adds its own binding");
}

void deadzoneOffMatchesRawSticks() {
    for (const std::int16_t axis : {std::int16_t{-32768}, std::int16_t{-20000}, std::int16_t{-1}, std::int16_t{0}, std::int16_t{1}, std::int16_t{1234}, std::int16_t{32767}}) {
        const auto stick = Pad::StickWithDeadzone(axis, axis, 0);
        require(stick[0] == plainStickByte(axis) && stick[1] == plainStickByte(axis), "a 0 deadzone keeps the raw mapping");
    }
}

void deadzoneShapesSticks() {
    require(Pad::StickWithDeadzone(3000, -3000, 15) == std::array<std::uint8_t, 2>{128, 128}, "a small deflection inside the deadzone is centered");
    require(Pad::StickWithDeadzone(32767, 0, 15) == std::array<std::uint8_t, 2>{255, 128}, "a full deflection still reaches the edge");
    require(Pad::StickWithDeadzone(-32768, 0, 15) == std::array<std::uint8_t, 2>{0, 128}, "a full negative deflection still reaches the edge");
    require(Pad::StickWithDeadzone(32767, 32767, 15) == std::array<std::uint8_t, 2>{255, 255}, "a full diagonal still reaches the corner");
    const auto justOutside = Pad::StickWithDeadzone(5100, 0, 15);
    require(justOutside[0] >= 128 && justOutside[0] <= 130 && justOutside[1] == 128, "the output starts at the center just outside the deadzone");
    const auto half = Pad::StickWithDeadzone(16384, 0, 20);
    require(half[0] > 128 && half[0] < plainStickByte(16384), "a deflection outside the deadzone is rescaled");
    bool rejected = false;
    try {
        Pad::StickWithDeadzone(0, 0, 91);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "an out-of-range deadzone is rejected");
}

}

int main() try {
    root = std::filesystem::temp_directory_path() / ("anyps5_input_mapping-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    settingsDefault();
    settingsParse();
    settingsReject();
    performanceOverlayBinding();
    screenshotBinding();
    padStatusBinding();
    padStatusConflicts();
    toggleMuteAction();
    toggleMuteDefault();
    hostVolumeDefaultsDoNotCollide();
    hostVolumeSharedKeysAreKept();
    hostVolumeActionNamesAreExact();
    volumeDownAction();
    volumeDownDefault();
    volumeUpAction();
    volumeUpDefault();
    mouseLookDefault();
    mouseLookParse();
    mouseLookReject();
    stickModifier();
    keyboardRamp();
    triggerRamp();
    toggleMouseDefaults();
    nonAsciiPaths();
    deadzoneOffMatchesRawSticks();
    deadzoneShapesSticks();
    std::filesystem::remove_all(root);
    std::cout << "input mapping tests passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}

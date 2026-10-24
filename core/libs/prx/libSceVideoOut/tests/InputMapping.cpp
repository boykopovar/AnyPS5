#include "prx/libScePad/include/InputMapping.hpp"
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

void mouseLookDefault() {
    useConfig("Cross = KEY:F\n");
    const auto settings = Pad::LoadInputMapping().settings;
    require(settings.mouseVerticalScale == 1.0 && settings.mouseCurve == 1.0 && settings.mouseMaxSpeed == 1.0, "mouse look scales default to 1");
    require(settings.mouseDeadzoneOffset == 0.2 && settings.mouseSmoothingMs == 10.0 && settings.mouseDecayMs == 30.0, "mouse look timing and offset defaults");
}

void mouseLookParse() {
    useConfig("MouseCurve=0.7 ; softer start\nmousedeadzoneoffset = 0.15\nMouseMaxSpeed = 1.5\nMouseSmoothingMs = 0\nMouseDecayMs = 50\nMouseVerticalScale = 0.5\n");
    const auto settings = Pad::LoadInputMapping().settings;
    require(settings.mouseCurve == 0.7, "mouse curve is read");
    require(settings.mouseDeadzoneOffset == 0.15, "mouse deadzone offset is read case-insensitively");
    require(settings.mouseMaxSpeed == 1.5, "mouse max speed is read");
    require(settings.mouseSmoothingMs == 0.0, "a 0 smoothing is accepted");
    require(settings.mouseDecayMs == 50.0, "mouse decay is read");
    require(settings.mouseVerticalScale == 0.5, "mouse vertical scale is read");
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

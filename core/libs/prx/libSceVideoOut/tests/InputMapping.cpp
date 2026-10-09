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

void useConfig(const std::string& text) {
    const auto path = root / "anyps5-input.ini";
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << text;
        require(file.good(), "write the input mapping");
    }
#ifdef _WIN32
    require(_putenv_s("ANYPS5_INPUT_CONFIG", path.string().c_str()) == 0, "set ANYPS5_INPUT_CONFIG");
#else
    require(::setenv("ANYPS5_INPUT_CONFIG", path.string().c_str(), 1) == 0, "set ANYPS5_INPUT_CONFIG");
#endif
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
    deadzoneOffMatchesRawSticks();
    deadzoneShapesSticks();
    std::filesystem::remove_all(root);
    std::cout << "input mapping tests passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}

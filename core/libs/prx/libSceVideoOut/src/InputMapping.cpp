#include "prx/libScePad/include/InputMapping.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

#include "SDL_keyboard.h"
#include "SDL_filesystem.h"

namespace {

struct InputAction {
    std::string_view name;
    Pad::InputControl control;
    Pad::PadButton button;
};

constexpr auto actions = std::array{
    InputAction{"Cross", Pad::InputControl::Button, Pad::PadButton::Cross},
    InputAction{"Circle", Pad::InputControl::Button, Pad::PadButton::Circle},
    InputAction{"Triangle", Pad::InputControl::Button, Pad::PadButton::Triangle},
    InputAction{"Square", Pad::InputControl::Button, Pad::PadButton::Square},
    InputAction{"L1", Pad::InputControl::Button, Pad::PadButton::L1},
    InputAction{"R1", Pad::InputControl::Button, Pad::PadButton::R1},
    InputAction{"L2", Pad::InputControl::Button, Pad::PadButton::L2},
    InputAction{"R2", Pad::InputControl::Button, Pad::PadButton::R2},
    InputAction{"L3", Pad::InputControl::Button, Pad::PadButton::L3},
    InputAction{"R3", Pad::InputControl::Button, Pad::PadButton::R3},
    InputAction{"Options", Pad::InputControl::Button, Pad::PadButton::Options},
    InputAction{"Up", Pad::InputControl::Button, Pad::PadButton::Up},
    InputAction{"Right", Pad::InputControl::Button, Pad::PadButton::Right},
    InputAction{"Down", Pad::InputControl::Button, Pad::PadButton::Down},
    InputAction{"Left", Pad::InputControl::Button, Pad::PadButton::Left},
    InputAction{"LeftStickLeft", Pad::InputControl::LeftStickLeft, Pad::PadButton::None},
    InputAction{"LeftStickRight", Pad::InputControl::LeftStickRight, Pad::PadButton::None},
    InputAction{"LeftStickUp", Pad::InputControl::LeftStickUp, Pad::PadButton::None},
    InputAction{"LeftStickDown", Pad::InputControl::LeftStickDown, Pad::PadButton::None},
    InputAction{"RightStickLeft", Pad::InputControl::RightStickLeft, Pad::PadButton::None},
    InputAction{"RightStickRight", Pad::InputControl::RightStickRight, Pad::PadButton::None},
    InputAction{"RightStickUp", Pad::InputControl::RightStickUp, Pad::PadButton::None},
    InputAction{"RightStickDown", Pad::InputControl::RightStickDown, Pad::PadButton::None},
    InputAction{"TouchLeft", Pad::InputControl::TouchLeft, Pad::PadButton::None},
    InputAction{"TouchRight", Pad::InputControl::TouchRight, Pad::PadButton::None},
    InputAction{"ToggleMouse", Pad::InputControl::ToggleMouse, Pad::PadButton::None},
    InputAction{"ToggleFullscreen", Pad::InputControl::ToggleFullscreen, Pad::PadButton::None}
};

std::string upper(std::string_view value) {
    std::string result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character) {
        return static_cast<char>(std::toupper(character));
    });
    return result;
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) value.remove_suffix(1);
    return value;
}

const InputAction* findAction(std::string_view name) {
    const auto normalized = upper(name);
    for (const auto& action : actions) {
        if (upper(action.name) == normalized) return &action;
    }
    return nullptr;
}

bool matchesAction(const Pad::InputBinding& binding, const InputAction& action) {
    if (action.control == Pad::InputControl::Button) {
        return binding.control == Pad::InputControl::Button && binding.button == action.button;
    }
    return binding.control == action.control;
}

Pad::MouseButton parseMouseButton(std::string_view name) {
    const auto normalized = upper(name);
    if (normalized == "LEFT") return Pad::MouseButton::Left;
    if (normalized == "MIDDLE") return Pad::MouseButton::Middle;
    if (normalized == "RIGHT") return Pad::MouseButton::Right;
    if (normalized == "X1") return Pad::MouseButton::X1;
    if (normalized == "X2") return Pad::MouseButton::X2;
    throw std::runtime_error("unknown mouse button '" + std::string(name) + "'");
}

Pad::InputBinding parseBinding(const InputAction& action, std::string_view source) {
    const auto separator = source.find(':');
    if (separator == std::string_view::npos) throw std::runtime_error("source must use TYPE:VALUE format");
    const auto type = upper(trim(source.substr(0, separator)));
    const auto value = trim(source.substr(separator + 1));
    if (value.empty()) throw std::runtime_error("source value is empty");

    if (type == "KEY") {
        const std::string keyName(value);
        const auto key = SDL_GetScancodeFromName(keyName.c_str());
        if (key == SDL_SCANCODE_UNKNOWN) throw std::runtime_error("unknown SDL key name '" + keyName + "'");
        return {key, Pad::MouseButton::None, action.control, action.button, 0};
    }
    if (type == "MOUSE") {
        if (action.control == Pad::InputControl::ToggleFullscreen) {
            throw std::runtime_error("ToggleFullscreen can only use a keyboard key");
        }
        return {SDL_SCANCODE_UNKNOWN, parseMouseButton(value), action.control, action.button, 0};
    }
    if (type == "WHEEL") {
        if (action.control != Pad::InputControl::Button) throw std::runtime_error("mouse wheel can only map to a pad button");
        const auto direction = upper(value);
        if (direction == "UP") return {SDL_SCANCODE_UNKNOWN, Pad::MouseButton::None, action.control, action.button, 1};
        if (direction == "DOWN") return {SDL_SCANCODE_UNKNOWN, Pad::MouseButton::None, action.control, action.button, -1};
        throw std::runtime_error("mouse wheel direction must be UP or DOWN");
    }
    throw std::runtime_error("source type must be KEY, MOUSE or WHEEL");
}

enum class InputSetting {
    LeftStickDeadzone,
    RightStickDeadzone,
    MouseSensitivity
};

struct InputSettingName {
    std::string_view name;
    InputSetting setting;
};

constexpr auto settingNames = std::array{
    InputSettingName{"LeftStickDeadzone", InputSetting::LeftStickDeadzone},
    InputSettingName{"RightStickDeadzone", InputSetting::RightStickDeadzone},
    InputSettingName{"MouseSensitivity", InputSetting::MouseSensitivity}
};

const InputSettingName* findSetting(std::string_view name) {
    const auto normalized = upper(name);
    for (const auto& setting : settingNames) {
        if (upper(setting.name) == normalized) return &setting;
    }
    return nullptr;
}

int parseDeadzonePercent(std::string_view value) {
    int parsed = 0;
    const auto end = value.data() + value.size();
    const auto result = std::from_chars(value.data(), end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || parsed < 0 || parsed > Pad::MaxStickDeadzonePercent) {
        throw std::runtime_error("deadzone must be a whole percentage from 0 to " + std::to_string(Pad::MaxStickDeadzonePercent));
    }
    return parsed;
}

double parseMouseSensitivity(std::string_view value) {
    double parsed = 0.0;
    const auto end = value.data() + value.size();
    const auto result = std::from_chars(value.data(), end, parsed);
    if (result.ec != std::errc{} || result.ptr != end || !(parsed > 0.0) || parsed > Pad::MaxMouseSensitivity) {
        throw std::runtime_error("mouse sensitivity must be a number greater than 0 and at most 10");
    }
    return parsed;
}

void applySetting(Pad::InputSettings& settings, InputSetting setting, std::string_view value) {
    switch (setting) {
        case InputSetting::LeftStickDeadzone: settings.leftStickDeadzonePercent = parseDeadzonePercent(value); break;
        case InputSetting::RightStickDeadzone: settings.rightStickDeadzonePercent = parseDeadzonePercent(value); break;
        case InputSetting::MouseSensitivity: settings.mouseSensitivity = parseMouseSensitivity(value); break;
    }
}

std::uint8_t stickByte(std::int32_t axis) {
    return static_cast<std::uint8_t>(((axis + 32768) * 255 + 32767) / 65535);
}

[[noreturn]] void invalidLine(const std::filesystem::path& path, std::size_t line, const std::string& reason) {
    throw std::runtime_error("Pad: invalid input mapping " + path.string() + ":" + std::to_string(line) + ": " + reason);
}

std::filesystem::path defaultConfigPath() {
    std::unique_ptr<char, decltype(&SDL_free)> basePath(SDL_GetBasePath(), SDL_free);
    if (basePath) return std::filesystem::path(basePath.get()) / "anyps5-input.ini";
    return "anyps5-input.ini";
}

}

std::array<std::uint8_t, 2> Pad::StickWithDeadzone(std::int16_t x, std::int16_t y, int deadzonePercent) {
    if (deadzonePercent < 0 || deadzonePercent > MaxStickDeadzonePercent) throw std::invalid_argument("Pad: stick deadzone out of range");
    if (deadzonePercent == 0) return {stickByte(x), stickByte(y)};
    const double normalizedX = std::max(x / 32767.0, -1.0);
    const double normalizedY = std::max(y / 32767.0, -1.0);
    const double magnitude = std::hypot(normalizedX, normalizedY);
    const double deadzone = deadzonePercent / 100.0;
    if (magnitude <= deadzone) return {stickByte(0), stickByte(0)};
    const double scale = (magnitude - deadzone) / (1.0 - deadzone) / magnitude;
    const auto axis = [scale](double value) {
        return stickByte(static_cast<std::int32_t>(std::lround(std::clamp(value * scale, -1.0, 1.0) * 32767.0)));
    };
    return {axis(normalizedX), axis(normalizedY)};
}

Pad::InputConfiguration Pad::LoadInputMapping() {
    InputConfiguration configuration{{InputMapping.begin(), InputMapping.end()}, {}};
    auto& bindings = configuration.bindings;
    const char* configuredPath = std::getenv("ANYPS5_INPUT_CONFIG");
    const bool explicitPath = configuredPath != nullptr && configuredPath[0] != '\0';
    const std::filesystem::path path = explicitPath ? configuredPath : defaultConfigPath();
    std::ifstream file(path);
    if (!file) {
        if (explicitPath || std::filesystem::exists(path)) {
            throw std::runtime_error("Pad: cannot read input mapping '" + path.string() + "'");
        }
        return configuration;
    }

    std::unordered_set<std::string> overriddenActions;
    std::array<bool, settingNames.size()> configuredSettings{};
    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (lineNumber == 1 && line.starts_with("\xEF\xBB\xBF")) line.erase(0, 3);
        const auto comment = line.find_first_of("#;");
        const std::string_view content = trim(std::string_view(line).substr(0, comment));
        if (content.empty()) continue;
        const auto separator = content.find('=');
        if (separator == std::string_view::npos) invalidLine(path, lineNumber, "expected Action = TYPE:VALUE");

        const auto actionName = trim(content.substr(0, separator));
        const auto source = trim(content.substr(separator + 1));
        if (const auto* setting = findSetting(actionName)) {
            auto& configured = configuredSettings[static_cast<std::size_t>(setting->setting)];
            if (configured) invalidLine(path, lineNumber, std::string(setting->name) + " is set more than once");
            configured = true;
            try {
                applySetting(configuration.settings, setting->setting, source);
            } catch (const std::runtime_error& error) {
                invalidLine(path, lineNumber, error.what());
            }
            continue;
        }
        const auto* action = findAction(actionName);
        if (action == nullptr) invalidLine(path, lineNumber, "unknown action '" + std::string(actionName) + "'");
        if (source.empty()) invalidLine(path, lineNumber, "source is empty");

        try {
            auto binding = parseBinding(*action, source);
            const auto normalizedAction = upper(action->name);
            if (overriddenActions.insert(normalizedAction).second) {
                std::erase_if(bindings, [action](const InputBinding& existing) { return matchesAction(existing, *action); });
            }
            const bool duplicate = std::any_of(bindings.begin(), bindings.end(), [&binding](const InputBinding& existing) {
                return existing.key == binding.key && existing.mouseButton == binding.mouseButton &&
                    existing.control == binding.control && existing.button == binding.button &&
                    existing.wheelDirection == binding.wheelDirection;
            });
            if (!duplicate) bindings.push_back(binding);
        } catch (const std::runtime_error& error) {
            invalidLine(path, lineNumber, error.what());
        }
    }
    if (file.bad()) throw std::runtime_error("Pad: cannot read input mapping '" + path.string() + "'");

    return configuration;
}

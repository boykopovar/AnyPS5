#include "prx/libScePad/include/InputMapping.hpp"

#include <algorithm>
#include <array>
#include <cctype>
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
    InputAction{"ToggleFullscreen", Pad::InputControl::ToggleFullscreen, Pad::PadButton::None},
    InputAction{"Screenshot", Pad::InputControl::Screenshot, Pad::PadButton::None}
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
        if (action.control == Pad::InputControl::ToggleFullscreen || action.control == Pad::InputControl::Screenshot) {
            throw std::runtime_error(std::string(action.name) + " can only use a keyboard key");
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

[[noreturn]] void invalidLine(const std::filesystem::path& path, std::size_t line, const std::string& reason) {
    throw std::runtime_error("Pad: invalid input mapping " + path.string() + ":" + std::to_string(line) + ": " + reason);
}

std::filesystem::path defaultConfigPath() {
    std::unique_ptr<char, decltype(&SDL_free)> basePath(SDL_GetBasePath(), SDL_free);
    if (basePath) return std::filesystem::path(basePath.get()) / "anyps5-input.ini";
    return "anyps5-input.ini";
}

}

std::vector<Pad::InputBinding> Pad::LoadInputMapping() {
    std::vector<InputBinding> bindings(InputMapping.begin(), InputMapping.end());
    const char* configuredPath = std::getenv("ANYPS5_INPUT_CONFIG");
    const bool explicitPath = configuredPath != nullptr && configuredPath[0] != '\0';
    const std::filesystem::path path = explicitPath ? configuredPath : defaultConfigPath();
    std::ifstream file(path);
    if (!file) {
        if (explicitPath || std::filesystem::exists(path)) {
            throw std::runtime_error("Pad: cannot read input mapping '" + path.string() + "'");
        }
        return bindings;
    }

    std::unordered_set<std::string> overriddenActions;
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

    return bindings;
}

#include "prx/libScePad/include/InputMapping.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <istream>
#include <memory>
#include <mutex>
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
    InputAction{"StickModifier", Pad::InputControl::StickModifier, Pad::PadButton::None},
    InputAction{"Shake", Pad::InputControl::Shake, Pad::PadButton::None},
    InputAction{"ToggleMouse", Pad::InputControl::ToggleMouse, Pad::PadButton::None},
    InputAction{"ToggleFullscreen", Pad::InputControl::ToggleFullscreen, Pad::PadButton::None},
    InputAction{"TogglePerformanceOverlay", Pad::InputControl::TogglePerformanceOverlay, Pad::PadButton::None},
    InputAction{"Screenshot", Pad::InputControl::Screenshot, Pad::PadButton::None},
    InputAction{"ShowPadStatus", Pad::InputControl::ShowPadStatus, Pad::PadButton::None},
    InputAction{"VolumeUp", Pad::InputControl::VolumeUp, Pad::PadButton::None},
    InputAction{"VolumeDown", Pad::InputControl::VolumeDown, Pad::PadButton::None},
    InputAction{"ToggleMute", Pad::InputControl::ToggleMute, Pad::PadButton::None},
    InputAction{"ReloadInputMapping", Pad::InputControl::ReloadInputMapping, Pad::PadButton::None}
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

bool isEmulatorAction(Pad::InputControl control) {
    switch (control) {
        case Pad::InputControl::ToggleFullscreen:
        case Pad::InputControl::TogglePerformanceOverlay:
        case Pad::InputControl::Screenshot:
        case Pad::InputControl::ShowPadStatus:
        case Pad::InputControl::VolumeUp:
        case Pad::InputControl::VolumeDown:
        case Pad::InputControl::ToggleMute:
        case Pad::InputControl::ReloadInputMapping: return true;
        default: return false;
    }
}

Pad::PadButton parsePadButton(std::string_view name) {
    const auto normalized = upper(name);
    if (normalized == "CROSS") return Pad::PadButton::Cross;
    if (normalized == "CIRCLE") return Pad::PadButton::Circle;
    if (normalized == "TRIANGLE") return Pad::PadButton::Triangle;
    if (normalized == "SQUARE") return Pad::PadButton::Square;
    if (normalized == "L1") return Pad::PadButton::L1;
    if (normalized == "R1") return Pad::PadButton::R1;
    if (normalized == "L2") return Pad::PadButton::L2;
    if (normalized == "R2") return Pad::PadButton::R2;
    if (normalized == "L3") return Pad::PadButton::L3;
    if (normalized == "R3") return Pad::PadButton::R3;
    if (normalized == "OPTIONS") return Pad::PadButton::Options;
    if (normalized == "UP") return Pad::PadButton::Up;
    if (normalized == "RIGHT") return Pad::PadButton::Right;
    if (normalized == "DOWN") return Pad::PadButton::Down;
    if (normalized == "LEFT") return Pad::PadButton::Left;
    if (normalized == "TOUCHPAD") return Pad::PadButton::TouchPad;
    throw std::runtime_error("unknown pad button '" + std::string(name) + "'");
}

std::uint32_t parsePadChord(std::string_view value) {
    std::uint32_t mask = 0;
    std::size_t start = 0;
    while (true) {
        const auto plus = value.find('+', start);
        const auto part = trim(value.substr(start, plus == std::string_view::npos ? std::string_view::npos : plus - start));
        if (part.empty()) throw std::runtime_error("pad chord has an empty button");
        mask |= static_cast<std::uint32_t>(parsePadButton(part));
        if (plus == std::string_view::npos) break;
        start = plus + 1;
    }
    return mask;
}

Pad::InputBinding parseSource(const InputAction& action, std::string_view source) {
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
        if (action.control == Pad::InputControl::TogglePerformanceOverlay) {
            throw std::runtime_error("TogglePerformanceOverlay can only use a keyboard key");
        }
        if (action.control == Pad::InputControl::Screenshot) {
            throw std::runtime_error("Screenshot can only use a keyboard key");
        }
        if (action.control == Pad::InputControl::ShowPadStatus) {
            throw std::runtime_error("ShowPadStatus can only use a keyboard key");
        }
        if (action.control == Pad::InputControl::VolumeUp) {
            throw std::runtime_error("VolumeUp can only use a keyboard key");
        }
        if (action.control == Pad::InputControl::VolumeDown) {
            throw std::runtime_error("VolumeDown can only use a keyboard key");
        }
        if (action.control == Pad::InputControl::ToggleMute) {
            throw std::runtime_error("ToggleMute can only use a keyboard key");
        }
        if (action.control == Pad::InputControl::ReloadInputMapping) {
            throw std::runtime_error("ReloadInputMapping can only use a keyboard key");
        }
        return {SDL_SCANCODE_UNKNOWN, parseMouseButton(value), action.control, action.button, 0};
    }
    if (type == "PAD") {
        if (!isEmulatorAction(action.control)) throw std::runtime_error("PAD chords can only trigger emulator actions");
        return {SDL_SCANCODE_UNKNOWN, Pad::MouseButton::None, action.control, action.button, 0, 0, parsePadChord(value)};
    }
    if (type == "WHEEL") {
        if (action.control != Pad::InputControl::Button) throw std::runtime_error("mouse wheel can only map to a pad button");
        const auto direction = upper(value);
        if (direction == "UP") return {SDL_SCANCODE_UNKNOWN, Pad::MouseButton::None, action.control, action.button, 1};
        if (direction == "DOWN") return {SDL_SCANCODE_UNKNOWN, Pad::MouseButton::None, action.control, action.button, -1};
        if (direction == "RIGHT") return {SDL_SCANCODE_UNKNOWN, Pad::MouseButton::None, action.control, action.button, 0, 1};
        if (direction == "LEFT") return {SDL_SCANCODE_UNKNOWN, Pad::MouseButton::None, action.control, action.button, 0, -1};
        throw std::runtime_error("mouse wheel direction must be UP, DOWN, LEFT or RIGHT");
    }
    throw std::runtime_error("source type must be KEY, MOUSE, WHEEL or PAD");
}

Pad::InputBinding parseBinding(const InputAction& action, std::string_view source) {
    auto mode = Pad::InputMode::Hold;
    auto rest = trim(source);
    const auto space = rest.find_first_of(" \t");
    if (space != std::string_view::npos && upper(rest.substr(0, space)) == "TOGGLE") {
        mode = Pad::InputMode::Toggle;
        rest = trim(rest.substr(space + 1));
    }
    auto binding = parseSource(action, rest);
    if (mode == Pad::InputMode::Hold) return binding;
    if (action.control != Pad::InputControl::Button) throw std::runtime_error("Toggle can only be used on a pad button");
    if (binding.wheelDirection != 0 || binding.wheelTilt != 0) throw std::runtime_error("Toggle cannot use the mouse wheel");
    binding.mode = mode;
    return binding;
}

enum class InputSetting {
    LeftStickDeadzone,
    RightStickDeadzone,
    MouseSensitivity,
    MouseVerticalScale,
    MouseCurve,
    MouseDeadzoneOffset,
    MouseMaxSpeed,
    MouseSmoothingMs,
    MouseDecayMs,
    StickModifierScale,
    KeyboardRampMs,
    TriggerRampMs,
    MouseControlsRightStick
};

struct InputSettingName {
    std::string_view name;
    InputSetting setting;
};

constexpr auto settingNames = std::array{
    InputSettingName{"LeftStickDeadzone", InputSetting::LeftStickDeadzone},
    InputSettingName{"RightStickDeadzone", InputSetting::RightStickDeadzone},
    InputSettingName{"MouseSensitivity", InputSetting::MouseSensitivity},
    InputSettingName{"MouseVerticalScale", InputSetting::MouseVerticalScale},
    InputSettingName{"MouseCurve", InputSetting::MouseCurve},
    InputSettingName{"MouseDeadzoneOffset", InputSetting::MouseDeadzoneOffset},
    InputSettingName{"MouseMaxSpeed", InputSetting::MouseMaxSpeed},
    InputSettingName{"MouseSmoothingMs", InputSetting::MouseSmoothingMs},
    InputSettingName{"MouseDecayMs", InputSetting::MouseDecayMs},
    InputSettingName{"StickModifierScale", InputSetting::StickModifierScale},
    InputSettingName{"KeyboardRampMs", InputSetting::KeyboardRampMs},
    InputSettingName{"TriggerRampMs", InputSetting::TriggerRampMs},
    InputSettingName{"MouseStickTarget", InputSetting::MouseControlsRightStick}
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

std::string formatNumber(double value) {
    std::array<char, 32> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    return std::string(buffer.data(), result.ptr);
}

double parseNumber(std::string_view value, std::string_view name, double minimum, double maximum) {
    double parsed = 0.0;
    const auto end = value.data() + value.size();
    const auto result = std::from_chars(value.data(), end, parsed);
    if (value.empty() || result.ec != std::errc{} || result.ptr != end || !std::isfinite(parsed) || parsed < minimum || parsed > maximum) {
        throw std::runtime_error(std::string(name) + " must be a number from " + formatNumber(minimum) + " to " + formatNumber(maximum));
    }
    return parsed;
}

int parseStickId(std::string_view value) {
    const auto normalized = upper(value);
    if (normalized == "LEFT") return 0;
    if (normalized == "RIGHT") return 1;
    throw std::runtime_error("mouse target stick must be LEFT or RIGHT");
}

void applySetting(Pad::InputSettings& settings, InputSetting setting, std::string_view value) {
    switch (setting) {
        case InputSetting::LeftStickDeadzone: settings.leftStickDeadzonePercent = parseDeadzonePercent(value); break;
        case InputSetting::RightStickDeadzone: settings.rightStickDeadzonePercent = parseDeadzonePercent(value); break;
        case InputSetting::MouseSensitivity: settings.mouseSensitivity = parseMouseSensitivity(value); break;
        case InputSetting::MouseVerticalScale: settings.mouseVerticalScale = parseNumber(value, "mouse vertical scale", 0.1, 10.0); break;
        case InputSetting::MouseCurve: settings.mouseCurve = parseNumber(value, "mouse curve", 0.2, 5.0); break;
        case InputSetting::MouseDeadzoneOffset: settings.mouseDeadzoneOffset = parseNumber(value, "mouse deadzone offset", 0.0, 0.9); break;
        case InputSetting::MouseMaxSpeed: settings.mouseMaxSpeed = parseNumber(value, "mouse max speed", 0.1, 10.0); break;
        case InputSetting::MouseSmoothingMs: settings.mouseSmoothingMs = parseNumber(value, "mouse smoothing", 0.0, 500.0); break;
        case InputSetting::MouseDecayMs: settings.mouseDecayMs = parseNumber(value, "mouse decay", 1.0, 1000.0); break;
        case InputSetting::StickModifierScale: settings.stickModifierScale = parseNumber(value, "stick modifier scale", 0.05, 1.0); break;
        case InputSetting::KeyboardRampMs: settings.keyboardRampMs = parseNumber(value, "keyboard ramp", 0.0, 2000.0); break;
        case InputSetting::TriggerRampMs: settings.triggerRampMs = parseNumber(value, "trigger ramp", 0.0, 2000.0); break;
        case InputSetting::MouseControlsRightStick: settings.mouseControlsRightStick = (parseStickId(value) == 1); break;
    }
}

std::uint8_t stickByte(std::int32_t axis) {
    return static_cast<std::uint8_t>(((axis + 32768) * 255 + 32767) / 65535);
}

std::filesystem::path pathFromUtf8(std::string_view text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}

std::string pathToUtf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return std::string(reinterpret_cast<const char*>(text.data()), text.size());
}

std::filesystem::path configuredPath() {
#ifdef _WIN32
    const wchar_t* configured = _wgetenv(L"ANYPS5_INPUT_CONFIG");
    return configured != nullptr ? std::filesystem::path(configured) : std::filesystem::path{};
#else
    const char* configured = std::getenv("ANYPS5_INPUT_CONFIG");
    return configured != nullptr ? pathFromUtf8(configured) : std::filesystem::path{};
#endif
}

[[noreturn]] void invalidLine(std::string_view source, std::size_t line, const std::string& reason) {
    throw std::runtime_error("Pad: invalid input mapping " + std::string(source) + ":" + std::to_string(line) + ": " + reason);
}

std::filesystem::path defaultConfigPath() {
    std::unique_ptr<char, decltype(&SDL_free)> basePath(SDL_GetBasePath(), SDL_free);
    if (basePath) return pathFromUtf8(basePath.get()) / "anyps5-input.ini";
    return "anyps5-input.ini";
}

void applyInputStream(Pad::InputConfiguration& configuration, std::istream& file, std::string_view source) {
    auto& bindings = configuration.bindings;
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
        if (separator == std::string_view::npos) invalidLine(source, lineNumber, "expected Action = TYPE:VALUE");

        const auto actionName = trim(content.substr(0, separator));
        const auto value = trim(content.substr(separator + 1));
        if (const auto* setting = findSetting(actionName)) {
            auto& configured = configuredSettings[static_cast<std::size_t>(setting->setting)];
            if (configured) invalidLine(source, lineNumber, std::string(setting->name) + " is set more than once");
            configured = true;
            try {
                applySetting(configuration.settings, setting->setting, value);
            } catch (const std::runtime_error& error) {
                invalidLine(source, lineNumber, error.what());
            }
            continue;
        }
        const auto* action = findAction(actionName);
        if (action == nullptr) invalidLine(source, lineNumber, "unknown action '" + std::string(actionName) + "'");
        if (value.empty()) invalidLine(source, lineNumber, "source is empty");

        try {
            auto binding = parseBinding(*action, value);
            const auto normalizedAction = upper(action->name);
            if (overriddenActions.insert(normalizedAction).second) {
                std::erase_if(bindings, [action](const Pad::InputBinding& existing) { return matchesAction(existing, *action); });
            }
            const bool duplicate = std::any_of(bindings.begin(), bindings.end(), [&binding](const Pad::InputBinding& existing) {
                return existing.key == binding.key && existing.mouseButton == binding.mouseButton &&
                    existing.control == binding.control && existing.button == binding.button &&
                    existing.wheelDirection == binding.wheelDirection && existing.wheelTilt == binding.wheelTilt &&
                    existing.padButtons == binding.padButtons;
            });
            if (!duplicate) bindings.push_back(binding);
        } catch (const std::runtime_error& error) {
            invalidLine(source, lineNumber, error.what());
        }
    }
    if (file.bad()) throw std::runtime_error("Pad: cannot read input mapping '" + std::string(source) + "'");
}

void loadInputFile(Pad::InputConfiguration& configuration, const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Pad: cannot read input mapping '" + pathToUtf8(path) + "'");
    applyInputStream(configuration, file, pathToUtf8(path));
}

}

void Pad::ApplyInputMapping(InputConfiguration& configuration, std::istream& input, std::string_view source) {
    applyInputStream(configuration, input, source);
}

namespace {

Pad::InputFileStamp fileStamp(const std::filesystem::path& path) {
    Pad::InputFileStamp stamp;
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) return stamp;
    stamp.present = true;
    stamp.modified = std::filesystem::last_write_time(path, error);
    stamp.size = std::filesystem::file_size(path, error);
    return stamp;
}

}

Pad::InputMappingStamp Pad::InputMappingFileStamp(std::string_view titleId) {
    const auto configured = configuredPath();
    const std::filesystem::path path = !configured.empty() ? configured : defaultConfigPath();
    InputMappingStamp stamp;
    stamp.shared = fileStamp(path);
    const auto profile = InputProfileName(titleId);
    if (!profile.empty()) stamp.profile = fileStamp(path.parent_path() / profile);
    return stamp;
}

Pad::InputMappingWatch::InputMappingWatch(InputMappingStamp applied) : applied(applied), candidate(applied) {}

bool Pad::InputMappingWatch::Poll(const InputMappingStamp& current) {
    if (current == applied) {
        candidate = current;
        return false;
    }
    if (current != candidate) {
        candidate = current;
        return false;
    }
    applied = current;
    return true;
}

Pad::InputMappingStore::InputMappingStore(InputConfiguration initial) : current(std::make_shared<const InputConfiguration>(std::move(initial))) {}

std::shared_ptr<const Pad::InputConfiguration> Pad::InputMappingStore::Snapshot() const {
    const std::lock_guard lock(mutex);
    return current;
}

std::uint64_t Pad::InputMappingStore::Generation() const {
    const std::lock_guard lock(mutex);
    return generation;
}

void Pad::InputMappingStore::Publish(InputConfiguration configuration) {
    auto next = std::make_shared<const InputConfiguration>(std::move(configuration));
    const std::lock_guard lock(mutex);
    current = std::move(next);
    ++generation;
}

bool Pad::TryLoadInputMapping(std::string_view titleId, InputConfiguration& configuration, std::string& error) {
    try {
        configuration = LoadInputMapping(titleId);
    } catch (const std::runtime_error& failure) {
        error = failure.what();
        return false;
    }
    return true;
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

std::string Pad::InputProfileName(std::string_view titleId) {
    const bool valid = !titleId.empty() && titleId.size() <= MaxTitleIdLength && std::all_of(titleId.begin(), titleId.end(), [](char character) {
        return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '_' || character == '-';
    });
    return valid ? "anyps5-input." + std::string(titleId) + ".ini" : std::string();
}

Pad::InputConfiguration Pad::LoadInputMapping(std::string_view titleId) {
    InputConfiguration configuration{{InputMapping.begin(), InputMapping.end()}, {}};
    const auto configured = configuredPath();
    const bool explicitPath = !configured.empty();
    const std::filesystem::path path = explicitPath ? configured : defaultConfigPath();
    if (explicitPath || std::filesystem::exists(path)) loadInputFile(configuration, path);
    const auto profile = InputProfileName(titleId);
    if (!profile.empty()) {
        const auto profilePath = path.parent_path() / profile;
        if (std::filesystem::exists(profilePath)) loadInputFile(configuration, profilePath);
    }
    return configuration;
}

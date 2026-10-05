#include "ControllerMapping.hpp"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <set>
#include <stdexcept>

namespace {
std::string_view trim(std::string_view text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) text.remove_prefix(1);
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) text.remove_suffix(1);
    return text;
}
void require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}
bool validGuid(std::string_view guid) {
    return guid.size() == 32 && std::all_of(guid.begin(), guid.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}
std::string normalizeGuid(std::string_view guid) {
    require(validGuid(guid), "expected a 32-digit SDL GUID");
    std::string result(guid);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}
std::vector<std::string> lines(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        require(!std::filesystem::exists(path), "Input: cannot read '" + path.string() + "'");
        return {};
    }
    std::vector<std::string> result;
    std::string line;
    while (std::getline(file, line)) result.push_back(line);
    require(!file.bad(), "Input: cannot read '" + path.string() + "'");
    if (!result.empty() && result[0].starts_with("\xEF\xBB\xBF")) result[0].erase(0, 3);
    return result;
}
int number(std::string_view value) {
    int result = -1;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() && result >= 0 && result <= 65535, "invalid physical input index");
    return result;
}
void validateSource(std::string_view source) {
    require(!source.empty(), "empty physical input");
    if (source.front() == '+' || source.front() == '-') {
        source.remove_prefix(1);
        require(source.starts_with("a"), "half input must be an axis");
    }
    if (source.ends_with("~")) {
        source.remove_suffix(1);
        require(source.starts_with("a"), "only axes can be inverted");
    }
    require(!source.empty(), "empty physical input");
    const char kind = source.front();
    source.remove_prefix(1);
    if (kind == 'a' || kind == 'b') { number(source); return; }
    require(kind == 'h', "physical input must be a button, axis or hat");
    const auto dot = source.find('.');
    require(dot != source.npos, "hat requires an index and mask");
    number(source.substr(0, dot));
    const auto mask = number(source.substr(dot + 1));
    require(mask == 1 || mask == 2 || mask == 4 || mask == 8, "hat mask must be a cardinal direction");
}
}

InputConfig::ControllerProfile::ControllerProfile() {
    for (std::size_t index = 0; index < buttons.size(); ++index) buttons[index] = ControllerButtons[index].source;
    for (std::size_t index = 0; index < axes.size(); ++index) axes[index].source = static_cast<SDL_GameControllerAxis>(index);
}

std::string InputConfig::Guid(SDL_Joystick* joystick) {
    char guid[33]{};
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), guid, sizeof(guid));
    return guid;
}

InputConfig::ControllerProfiles InputConfig::LoadControllers(const std::filesystem::path& path) {
    ControllerProfiles result;
    ControllerProfile* profile = nullptr;
    std::set<std::string> seen;
    const auto contents = lines(path);
    for (std::size_t line = 0; line < contents.size(); ++line) {
        try {
            const auto text = trim(std::string_view(contents[line]).substr(0, contents[line].find_first_of("#;")));
            if (text.empty()) continue;
            if (text.front() == '[' && text.back() == ']') {
                const auto guid = normalizeGuid(text.substr(1, text.size() - 2));
                require(!result.contains(guid), "duplicate controller profile");
                profile = &result[guid];
                seen.clear();
                continue;
            }
            require(profile != nullptr, "binding requires a [GUID] section");
            const auto equal = text.find('=');
            require(equal != text.npos, "expected Target = TYPE:VALUE");
            const auto name = trim(text.substr(0, equal));
            const auto value = trim(text.substr(equal + 1));
            require(seen.insert(std::string(name)).second, "duplicate controller target");
            bool found = false;
            for (std::size_t index = 0; index < ControllerButtons.size(); ++index) {
                if (name != ControllerButtons[index].name) continue;
                found = true;
                if (value == "NONE") profile->buttons[index] = SDL_CONTROLLER_BUTTON_INVALID;
                else {
                    require(value.starts_with("BUTTON:"), "button target requires BUTTON:name");
                    const auto button = SDL_GameControllerGetButtonFromString(std::string(value.substr(7)).c_str());
                    require(button != SDL_CONTROLLER_BUTTON_INVALID, "unknown SDL controller button");
                    profile->buttons[index] = button;
                }
            }
            for (std::size_t index = 0; index < ControllerAxisNames.size(); ++index) {
                if (name != ControllerAxisNames[index]) continue;
                found = true;
                if (value == "NONE") profile->axes[index] = {SDL_CONTROLLER_AXIS_INVALID, false};
                else {
                    require(value.starts_with("AXIS:"), "axis target requires AXIS:name or AXIS:name:inverted");
                    auto source = value.substr(5);
                    const bool inverted = source.ends_with(":inverted");
                    if (inverted) source.remove_suffix(9);
                    const auto axis = SDL_GameControllerGetAxisFromString(std::string(source).c_str());
                    require(axis != SDL_CONTROLLER_AXIS_INVALID, "unknown SDL controller axis");
                    profile->axes[index] = {axis, inverted};
                }
            }
            require(found, "unknown PS5 target");
        } catch (const std::exception& error) {
            throw std::runtime_error("Input: " + path.string() + ":" + std::to_string(line + 1) + ": " + error.what());
        }
    }
    return result;
}

std::string InputConfig::SerializeControllers(const ControllerProfiles& profiles) {
    std::string result;
    for (const auto& [guid, profile] : profiles) {
        result += "[" + normalizeGuid(guid) + "]\n";
        for (std::size_t index = 0; index < profile.buttons.size(); ++index) {
            const auto source = profile.buttons[index];
            const char* name = SDL_GameControllerGetStringForButton(source);
            require(source == SDL_CONTROLLER_BUTTON_INVALID || name != nullptr, "invalid controller button");
            result += std::string(ControllerButtons[index].name) + " = " + (source == SDL_CONTROLLER_BUTTON_INVALID ? std::string("NONE") : std::string("BUTTON:") + name) + "\n";
        }
        for (std::size_t index = 0; index < profile.axes.size(); ++index) {
            const auto source = profile.axes[index];
            const char* name = SDL_GameControllerGetStringForAxis(source.source);
            require(source.source == SDL_CONTROLLER_AXIS_INVALID || name != nullptr, "invalid controller axis");
            result += std::string(ControllerAxisNames[index]) + " = " + (source.source == SDL_CONTROLLER_AXIS_INVALID ? std::string("NONE") : std::string("AXIS:") + name + (source.inverted ? ":inverted" : "")) + "\n";
        }
        result += "\n";
    }
    return result;
}

PadInputState InputConfig::SampleController(SDL_GameController* controller, const ControllerProfile& profile) {
    PadInputState result;
    if (controller == nullptr) return result;
    const auto type = SDL_GameControllerGetType(controller);
    const bool playStation = type == SDL_CONTROLLER_TYPE_PS4 || type == SDL_CONTROLLER_TYPE_PS5;
    for (std::size_t index = 0; index < profile.buttons.size(); ++index) {
        if (profile.buttons[index] != SDL_CONTROLLER_BUTTON_INVALID && SDL_GameControllerGetButton(controller, profile.buttons[index])) result.buttons |= static_cast<std::uint32_t>(ControllerButtons[index].output);
        if (!playStation && ControllerButtons[index].output == Pad::PadButton::TouchPad && profile.buttons[index] == SDL_CONTROLLER_BUTTON_TOUCHPAD && SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_BACK)) result.buttons |= static_cast<std::uint32_t>(Pad::PadButton::TouchPad);
    }
    for (std::size_t index = 0; index < profile.axes.size(); ++index) {
        const auto source = profile.axes[index];
        int value = source.source == SDL_CONTROLLER_AXIS_INVALID ? 0 : SDL_GameControllerGetAxis(controller, source.source);
        if (index < 4) {
            if (source.inverted) value = -value - 1;
            result.sticks[index] = source.source == SDL_CONTROLLER_AXIS_INVALID ? 128 : static_cast<std::uint8_t>((static_cast<std::int64_t>(value + 32768) * 255 + 32767) / 65535);
        } else {
            value = std::clamp(value, 0, 32767);
            if (source.inverted && source.source != SDL_CONTROLLER_AXIS_INVALID) value = 32767 - value;
            const auto analog = static_cast<std::uint8_t>((value * 255 + 16383) / 32767);
            if (index == 4) result.analogButtonsL2 = analog;
            else result.analogButtonsR2 = analog;
            if (analog) result.buttons |= static_cast<std::uint32_t>(index == 4 ? Pad::PadButton::L2 : Pad::PadButton::R2);
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
            result.touch[finger].x = static_cast<std::uint16_t>(std::clamp(x, 0.0f, 1.0f) * 1919.0f);
            result.touch[finger].y = static_cast<std::uint16_t>(std::clamp(y, 0.0f, 1.0f) * 942.0f);
        }
    }
    if (!playStation && (result.buttons & static_cast<std::uint32_t>(Pad::PadButton::TouchPad)) != 0 && !result.touch[0].active && !result.touch[1].active) result.touch[0] = {true, 960, 471, 0};
    return result;
}

void InputConfig::ValidateMapping(std::string_view mapping) {
    require(mapping.find_first_of("\r\n") == mapping.npos, "mapping must occupy one line");
    const auto comma = mapping.find(',');
    require(comma != mapping.npos && validGuid(mapping.substr(0, comma)), "mapping requires a GUID");
    mapping.remove_prefix(comma + 1);
    const auto nameEnd = mapping.find(',');
    require(nameEnd != mapping.npos && nameEnd > 0, "mapping requires a controller name");
    mapping.remove_prefix(nameEnd + 1);
    std::set<std::string> seen;
    bool hasInput = false;
    while (!mapping.empty()) {
        const auto end = mapping.find(',');
        const auto field = mapping.substr(0, end);
        if (!field.empty()) {
            const auto colon = field.find(':');
            require(colon != field.npos, "mapping field requires a source");
            const std::string target(field.substr(0, colon));
            const auto source = field.substr(colon + 1);
            require(seen.insert(target).second, "duplicate SDL target");
            if (target == "platform") require(source == "Windows" || source == "Linux" || source == "Mac OS X", "unknown mapping platform");
            else {
                auto output = std::string_view(target);
                if (output.starts_with("+") || output.starts_with("-")) output.remove_prefix(1);
                require(SDL_GameControllerGetButtonFromString(std::string(output).c_str()) != SDL_CONTROLLER_BUTTON_INVALID || SDL_GameControllerGetAxisFromString(std::string(output).c_str()) != SDL_CONTROLLER_AXIS_INVALID, "unknown SDL target");
                validateSource(source);
                hasInput = true;
            }
        }
        if (end == mapping.npos) break;
        mapping.remove_prefix(end + 1);
    }
    require(hasInput, "mapping requires at least one input");
}

std::vector<std::string> InputConfig::LoadDatabase(const std::filesystem::path& path) {
    std::vector<std::string> result;
    const auto contents = lines(path);
    for (std::size_t line = 0; line < contents.size(); ++line) {
        const auto text = trim(contents[line]);
        if (text.empty() || text.front() == '#') continue;
        try { ValidateMapping(text); }
        catch (const std::exception& error) { throw std::runtime_error("Input: " + path.string() + ":" + std::to_string(line + 1) + ": " + error.what()); }
        result.emplace_back(text);
    }
    return result;
}

std::string InputConfig::SerializeDatabase(const std::vector<std::string>& mappings) {
    std::string result;
    for (const auto& mapping : mappings) { ValidateMapping(mapping); result += mapping + "\n"; }
    return result;
}

void InputConfig::ApplyDatabase(const std::vector<std::string>& mappings) {
    for (const auto& mapping : mappings) ValidateMapping(mapping);
    for (const auto& mapping : mappings) {
        const auto platform = mapping.find("platform:");
        if (platform != mapping.npos) {
            const auto end = mapping.find(',', platform);
            if (mapping.substr(platform + 9, end - platform - 9) != SDL_GetPlatform()) continue;
        }
        if (SDL_GameControllerAddMapping(mapping.c_str()) < 0) throw std::runtime_error(std::string("Input: SDL mapping failed: ") + SDL_GetError());
    }
}

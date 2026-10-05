#ifndef ANYPS5_CONTROLLER_MAPPING_HPP
#define ANYPS5_CONTROLLER_MAPPING_HPP

#include "InputConfiguration.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include <array>
#include <map>

namespace InputConfig {

struct ControllerButton {
    std::string_view name;
    Pad::PadButton output;
    SDL_GameControllerButton source;
};
struct ControllerAxis {
    SDL_GameControllerAxis source;
    bool inverted = false;
};
inline constexpr std::array ControllerButtons{
    ControllerButton{"Cross", Pad::PadButton::Cross, SDL_CONTROLLER_BUTTON_A},
    ControllerButton{"Circle", Pad::PadButton::Circle, SDL_CONTROLLER_BUTTON_B},
    ControllerButton{"Square", Pad::PadButton::Square, SDL_CONTROLLER_BUTTON_X},
    ControllerButton{"Triangle", Pad::PadButton::Triangle, SDL_CONTROLLER_BUTTON_Y},
    ControllerButton{"L1", Pad::PadButton::L1, SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
    ControllerButton{"R1", Pad::PadButton::R1, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
    ControllerButton{"Options", Pad::PadButton::Options, SDL_CONTROLLER_BUTTON_START},
    ControllerButton{"L3", Pad::PadButton::L3, SDL_CONTROLLER_BUTTON_LEFTSTICK},
    ControllerButton{"R3", Pad::PadButton::R3, SDL_CONTROLLER_BUTTON_RIGHTSTICK},
    ControllerButton{"Up", Pad::PadButton::Up, SDL_CONTROLLER_BUTTON_DPAD_UP},
    ControllerButton{"Right", Pad::PadButton::Right, SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
    ControllerButton{"Down", Pad::PadButton::Down, SDL_CONTROLLER_BUTTON_DPAD_DOWN},
    ControllerButton{"Left", Pad::PadButton::Left, SDL_CONTROLLER_BUTTON_DPAD_LEFT},
    ControllerButton{"TouchPad", Pad::PadButton::TouchPad, SDL_CONTROLLER_BUTTON_TOUCHPAD}
};
inline constexpr std::array<std::string_view, 6> ControllerAxisNames{"LeftStickX", "LeftStickY", "RightStickX", "RightStickY", "L2", "R2"};
struct ControllerProfile {
    std::array<SDL_GameControllerButton, ControllerButtons.size()> buttons{};
    std::array<ControllerAxis, 6> axes{};
    ControllerProfile();
};
using ControllerProfiles = std::map<std::string, ControllerProfile>;
std::string Guid(SDL_Joystick* joystick);
ControllerProfiles LoadControllers(const std::filesystem::path& path);
std::string SerializeControllers(const ControllerProfiles& profiles);
PadInputState SampleController(SDL_GameController* controller, const ControllerProfile& profile);
std::vector<std::string> LoadDatabase(const std::filesystem::path& path);
void ValidateMapping(std::string_view mapping);
std::string SerializeDatabase(const std::vector<std::string>& mappings);
void ApplyDatabase(const std::vector<std::string>& mappings);

}

#endif

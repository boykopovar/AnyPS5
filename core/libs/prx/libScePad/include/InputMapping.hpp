#ifndef CORE_LIBS_PRX_LIBSCEPAD_INPUTMAPPING_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_INPUTMAPPING_HPP

#include <array>
#include <cstdint>
#include <vector>
#include "SDL_scancode.h"
#include "SDL_mouse.h"

#include "PadInputTypes.hpp"

namespace Pad {

inline constexpr int MousePollIntervalMs = 33;
inline constexpr int WheelPressDurationMs = 80;
inline constexpr double DefaultMouseSensitivity = 1.0;
inline constexpr double MaxMouseSensitivity = 10.0;
inline constexpr int MaxStickDeadzonePercent = 90;

struct InputSettings {
    int leftStickDeadzonePercent = 0;
    int rightStickDeadzonePercent = 0;
    double mouseSensitivity = DefaultMouseSensitivity;
};

struct InputConfiguration {
    std::vector<InputBinding> bindings;
    InputSettings settings;
};

inline constexpr std::array InputMapping{
    InputBinding{SDL_SCANCODE_F11, MouseButton::None, InputControl::ToggleFullscreen},
    InputBinding{SDL_SCANCODE_RETURN, MouseButton::None, InputControl::Button, PadButton::Cross},
    InputBinding{SDL_SCANCODE_SPACE, MouseButton::None, InputControl::Button, PadButton::Cross},
    InputBinding{SDL_SCANCODE_ESCAPE, MouseButton::None, InputControl::Button, PadButton::Options},
    InputBinding{SDL_SCANCODE_I, MouseButton::None, InputControl::Button, PadButton::Triangle},
    InputBinding{SDL_SCANCODE_C, MouseButton::None, InputControl::Button, PadButton::Circle},
    InputBinding{SDL_SCANCODE_Q, MouseButton::None, InputControl::Button, PadButton::L1},
    InputBinding{SDL_SCANCODE_E, MouseButton::None, InputControl::Button, PadButton::R1},
    InputBinding{SDL_SCANCODE_LALT, MouseButton::None, InputControl::Button, PadButton::R1},
    InputBinding{SDL_SCANCODE_RALT, MouseButton::None, InputControl::Button, PadButton::R1},
    InputBinding{SDL_SCANCODE_LSHIFT, MouseButton::None, InputControl::Button, PadButton::L3},
    InputBinding{SDL_SCANCODE_RSHIFT, MouseButton::None, InputControl::Button, PadButton::L3},
    InputBinding{SDL_SCANCODE_LCTRL, MouseButton::None, InputControl::Button, PadButton::R3},
    InputBinding{SDL_SCANCODE_RCTRL, MouseButton::None, InputControl::Button, PadButton::R3},
    InputBinding{SDL_SCANCODE_UP, MouseButton::None, InputControl::Button, PadButton::Up},
    InputBinding{SDL_SCANCODE_RIGHT, MouseButton::None, InputControl::Button, PadButton::Right},
    InputBinding{SDL_SCANCODE_DOWN, MouseButton::None, InputControl::Button, PadButton::Down},
    InputBinding{SDL_SCANCODE_LEFT, MouseButton::None, InputControl::Button, PadButton::Left},
    InputBinding{SDL_SCANCODE_A, MouseButton::None, InputControl::LeftStickLeft},
    InputBinding{SDL_SCANCODE_D, MouseButton::None, InputControl::LeftStickRight},
    InputBinding{SDL_SCANCODE_W, MouseButton::None, InputControl::LeftStickUp},
    InputBinding{SDL_SCANCODE_S, MouseButton::None, InputControl::LeftStickDown},
    InputBinding{SDL_SCANCODE_F, MouseButton::None, InputControl::RightStickLeft},
    InputBinding{SDL_SCANCODE_H, MouseButton::None, InputControl::RightStickRight},
    InputBinding{SDL_SCANCODE_T, MouseButton::None, InputControl::RightStickUp},
    InputBinding{SDL_SCANCODE_G, MouseButton::None, InputControl::RightStickDown},
    InputBinding{SDL_SCANCODE_BACKSPACE, MouseButton::None, InputControl::TouchLeft},
    InputBinding{SDL_SCANCODE_TAB, MouseButton::None, InputControl::TouchRight},
    InputBinding{SDL_SCANCODE_UNKNOWN, MouseButton::Middle, InputControl::ToggleMouse},
    InputBinding{SDL_SCANCODE_UNKNOWN, MouseButton::Left, InputControl::Button, PadButton::Square},
    InputBinding{SDL_SCANCODE_UNKNOWN, MouseButton::Right, InputControl::Button, PadButton::R2},
    InputBinding{SDL_SCANCODE_UNKNOWN, MouseButton::None, InputControl::Button, PadButton::Up, 1},
    InputBinding{SDL_SCANCODE_UNKNOWN, MouseButton::None, InputControl::Button, PadButton::Down, -1}
};

InputConfiguration LoadInputMapping();
std::array<std::uint8_t, 2> StickWithDeadzone(std::int16_t x, std::int16_t y, int deadzonePercent);

}

#endif

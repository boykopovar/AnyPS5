#ifndef CORE_LIBS_PRX_LIBSCEPAD_PADINPUTTYPES_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PADINPUTTYPES_HPP

#include <array>
#include <cstdint>
#include "SDL_scancode.h"
#include "SDL_mouse.h"

namespace Pad {
enum class InputControl {
    Button,
    LeftStickLeft,
    LeftStickRight,
    LeftStickUp,
    LeftStickDown,
    RightStickLeft,
    RightStickRight,
    RightStickUp,
    RightStickDown,
    TouchLeft,
    TouchRight,
    ToggleMouse,
    ToggleFullscreen,
    Screenshot
};

enum class PadButton : std::uint32_t {
    None = 0,
    L3 = 0x0002,
    R3 = 0x0004,
    Options = 0x0008,
    Up = 0x0010,
    Right = 0x0020,
    Down = 0x0040,
    Left = 0x0080,
    L2 = 0x0100,
    R2 = 0x0200,
    L1 = 0x0400,
    R1 = 0x0800,
    Triangle = 0x1000,
    Circle = 0x2000,
    Cross = 0x4000,
    Square = 0x8000,
    TouchPad = 0x100000
};

enum class MouseButton : std::uint8_t {
    None = 0,
    Left = SDL_BUTTON_LEFT,
    Middle = SDL_BUTTON_MIDDLE,
    Right = SDL_BUTTON_RIGHT,
    X1 = SDL_BUTTON_X1,
    X2 = SDL_BUTTON_X2
};

struct InputBinding {
    SDL_Scancode key;
    MouseButton mouseButton;
    InputControl control;
    PadButton button = PadButton::None;
    int wheelDirection = 0;
};

}

#endif

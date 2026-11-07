#include "prx/libSceVideoOut/include/PadStatusSdl.hpp"

AgcDriver::PadKind PadKindFromSdl(SDL_GameControllerType type) {
    switch (type) {
        case SDL_CONTROLLER_TYPE_PS5: return AgcDriver::PadKind::DualSense;
        case SDL_CONTROLLER_TYPE_PS4: return AgcDriver::PadKind::DualShock4;
        case SDL_CONTROLLER_TYPE_PS3: return AgcDriver::PadKind::DualShock3;
        case SDL_CONTROLLER_TYPE_XBOX360:
        case SDL_CONTROLLER_TYPE_XBOXONE: return AgcDriver::PadKind::Xbox;
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
        case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR: return AgcDriver::PadKind::Switch;
        case SDL_CONTROLLER_TYPE_AMAZON_LUNA:
        case SDL_CONTROLLER_TYPE_GOOGLE_STADIA:
        case SDL_CONTROLLER_TYPE_NVIDIA_SHIELD: return AgcDriver::PadKind::Other;
        case SDL_CONTROLLER_TYPE_UNKNOWN:
        case SDL_CONTROLLER_TYPE_VIRTUAL:
        case SDL_CONTROLLER_TYPE_MAX: break;
    }
    return AgcDriver::PadKind::Unknown;
}

AgcDriver::PadPower PadPowerFromSdl(SDL_JoystickPowerLevel level) {
    switch (level) {
        case SDL_JOYSTICK_POWER_EMPTY: return AgcDriver::PadPower::Empty;
        case SDL_JOYSTICK_POWER_LOW: return AgcDriver::PadPower::Low;
        case SDL_JOYSTICK_POWER_MEDIUM: return AgcDriver::PadPower::Medium;
        case SDL_JOYSTICK_POWER_FULL: return AgcDriver::PadPower::Full;
        case SDL_JOYSTICK_POWER_WIRED: return AgcDriver::PadPower::Wired;
        case SDL_JOYSTICK_POWER_UNKNOWN:
        case SDL_JOYSTICK_POWER_MAX: break;
    }
    return AgcDriver::PadPower::Unknown;
}

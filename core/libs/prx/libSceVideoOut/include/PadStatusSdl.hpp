#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADSTATUSSDL_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_PADSTATUSSDL_HPP

#include "SDL_gamecontroller.h"
#include "SDL_joystick.h"
#include "prx/libSceAgcDriver/Execution/include/PadStatus.hpp"

AgcDriver::PadKind PadKindFromSdl(SDL_GameControllerType type);
AgcDriver::PadPower PadPowerFromSdl(SDL_JoystickPowerLevel level);

#endif

#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_HOSTPAUSESETTINGS_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_HOSTPAUSESETTINGS_HPP

#include "SDL_scancode.h"

struct HostPauseSettings {
    SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
    bool onFocusLoss = false;

    bool Enabled() const { return key != SDL_SCANCODE_UNKNOWN || onFocusLoss; }
};

HostPauseSettings ReadHostPauseSettings(const char* key, const char* onFocusLoss);

#endif

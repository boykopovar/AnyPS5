#include "prx/libSceVideoOut/include/KeepAwake.hpp"
#include "SDL.h"
#include <cstring>

bool KeepAwakeEnabledFromValue(const char* value) {
    return value == nullptr || std::strcmp(value, "0") != 0;
}

HostKeepAwake::HostKeepAwake(const KeepAwakeBackend& backend) : backend(backend) {
}

void HostKeepAwake::Acquire() {
    if (held) return;
    backend.disable();
    held = true;
}

void HostKeepAwake::Release() noexcept {
    if (!held) return;
    backend.enable();
    held = false;
}

namespace {

void SdlDisable() {
    SDL_DisableScreenSaver();
}

void SdlEnable() {
    SDL_EnableScreenSaver();
}

bool SdlIsEnabled() {
    return SDL_IsScreenSaverEnabled() == SDL_TRUE;
}

const KeepAwakeBackend sdlBackend{SdlDisable, SdlEnable, SdlIsEnabled};

}

const KeepAwakeBackend& SdlKeepAwakeBackend() {
    return sdlBackend;
}

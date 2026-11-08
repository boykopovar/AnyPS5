#include "prx/libSceVideoOut/include/KeepAwake.hpp"
#include "SDL.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

bool KeepAwakeEnabledFromValue(const char* value) {
    return value == nullptr || std::strcmp(value, "0") != 0;
}

bool KeepAwakeEnabled() {
    return KeepAwakeEnabledFromValue(std::getenv("ANYPS5_KEEP_AWAKE"));
}

HostKeepAwake::HostKeepAwake(const KeepAwakeBackend& backend) : backend(backend) {
}

void HostKeepAwake::Acquire() {
    if (held) return;
    backend.disable();
    held = true;
    if (backend.isEnabled()) std::fprintf(stderr, "AnyPS5: the host did not keep the screen awake; the screen may sleep while the title runs\n");
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

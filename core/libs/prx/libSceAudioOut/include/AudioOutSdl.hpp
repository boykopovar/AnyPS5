#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_AUDIOOUTSDL_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_AUDIOOUTSDL_HPP

#include <cstring>
#include <string>

#include "SDL.h"

int InitializeSdlAudio_nid_no_patch();
bool WasapiDefaultOutputAbsent_nid_no_patch();

inline bool SdlAudioReportsNoOutputEndpoint(const char* driver, int outputDevices) {
    return outputDevices == 0 && driver != nullptr && std::strcmp(driver, "wasapi") == 0 && WasapiDefaultOutputAbsent_nid_no_patch();
}

inline std::string SdlDefaultOutputDeviceName() {
    char* name = nullptr;
    SDL_AudioSpec spec{};
    if (SDL_GetDefaultAudioInfo(&name, &spec, 0) != 0 || name == nullptr) return {};
    std::string result(name);
    SDL_free(name);
    return result;
}

#endif

#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_AUDIOOUTSDL_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_AUDIOOUTSDL_HPP

#include <cstring>

int InitializeSdlAudio_nid_no_patch();
bool WasapiDefaultOutputAbsent_nid_no_patch();

inline bool SdlAudioReportsNoOutputEndpoint(const char* driver, int outputDevices) {
    return outputDevices == 0 && driver != nullptr && std::strcmp(driver, "wasapi") == 0 && WasapiDefaultOutputAbsent_nid_no_patch();
}

#endif

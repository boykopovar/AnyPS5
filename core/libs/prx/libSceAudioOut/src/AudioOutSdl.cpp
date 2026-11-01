#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"

#include "SDL.h"
#include <mutex>
#ifdef _WIN32
#include <mmdeviceapi.h>
#endif

int InitializeSdlAudio_nid_no_patch() {
    static std::mutex initializationMutex;
    std::lock_guard lock(initializationMutex);
    return SDL_InitSubSystem(SDL_INIT_AUDIO);
}

bool WasapiDefaultOutputAbsent_nid_no_patch() {
#ifdef _WIN32
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool absent = false;
    IMMDeviceEnumerator* enumerator = nullptr;
    if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator)))) {
        IMMDevice* device = nullptr;
        absent = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        if (device != nullptr) device->Release();
        enumerator->Release();
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
    return absent;
#else
    return false;
#endif
}

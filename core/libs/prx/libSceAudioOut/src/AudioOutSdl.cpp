#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"

#include "SDL.h"
#include <cerrno>
#include <mutex>
#ifdef _WIN32
#include <mmdeviceapi.h>
#endif

namespace {

struct ErrnoGuard {
    int savedErrno = errno;
    ~ErrnoGuard() {
        errno = savedErrno;
    }
};

}

int InitializeSdlAudio_nid_no_patch() {
    const ErrnoGuard errnoGuard;
    static std::mutex initializationMutex;
    std::lock_guard lock(initializationMutex);
    if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0) {
        return 0;
    }
    return SDL_InitSubSystem(SDL_INIT_AUDIO);
}

bool WasapiDefaultOutputAbsent_nid_no_patch() {
#ifdef _WIN32
    const ErrnoGuard errnoGuard;
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool absent = false;
    if (SUCCEEDED(initialized) || initialized == RPC_E_CHANGED_MODE) {
        IMMDeviceEnumerator* enumerator = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator))) && enumerator != nullptr) {
            IMMDevice* device = nullptr;
            absent = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device) == HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
            if (device != nullptr) device->Release();
            enumerator->Release();
        }
        if (SUCCEEDED(initialized)) CoUninitialize();
    }
    return absent;
#else
    return false;
#endif
}

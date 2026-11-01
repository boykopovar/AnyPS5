#include "prx/libSceAudiodec/include/Api.hpp"
#include "SceTypes.hpp"

#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

void* Find(const char* name) {
#ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(LoadLibraryA("libSceAudiodec.prx"), name));
#else
    return dlsym(RTLD_DEFAULT, name);
#endif
}

}

int main() {
    static int32_t (APS5_VABI* volatile initLibrary)(uint32_t) = &sceAudiodecInitLibrary;
    if (Find("sceAudiodecInitLibrary") == nullptr) {
        std::fprintf(stderr, "libSceAudiodec is not loaded or does not export sceAudiodecInitLibrary\n");
        return EXIT_FAILURE;
    }
    static_cast<void>(initLibrary);
    for (const char* name : {"sceAudiodecGetAjmContext", "sceAudiodecUnregisterCodec"}) {
        if (Find(name) != nullptr) {
            std::fprintf(stderr, "%s is exported by libSceAudiodec, which only the native module exports on the console\n", name);
            return EXIT_FAILURE;
        }
    }
    std::puts("Audiodec standard exports tests passed");
    return 0;
}

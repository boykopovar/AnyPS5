#include <stdexcept>
#include <string>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2CreateHandle(int* handle) {
    if (handle == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    *handle = NP_TROPHY2_HANDLE_DEFAULT;
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2DestroyHandle(int handle) {
    (void)handle;
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2AbortHandle(int handle) {
    (void)handle;
    return SCE_NP_TROPHY2_OK;
}

}

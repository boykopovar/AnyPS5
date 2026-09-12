#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateHandle(int* handle) {
    if (handle == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    *handle = NP_UNIVERSAL_DATA_SYSTEM_HANDLE_DEFAULT;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyHandle(int handle) {
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemAbortHandle(int handle) {
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}

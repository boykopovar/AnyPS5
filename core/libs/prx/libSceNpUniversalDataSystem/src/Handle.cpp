#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include <cerrno>
#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateHandle(int* handle) {
    if (handle == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    const int value = NP_UNIVERSAL_DATA_SYSTEM_HANDLE_DEFAULT;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(handle, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyHandle(int handle) {
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemAbortHandle(int handle) {
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}

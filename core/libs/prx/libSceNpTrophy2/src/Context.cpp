#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "prx/libSceNpTrophy2/include/NpTrophy2.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpTrophy2CreateContext(int* context, int user_id, uint32_t service_label, uint64_t options) {
    if (context == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    (void)user_id;
    (void)service_label;
    (void)options;
    *context = NP_TROPHY2_CONTEXT_DEFAULT;
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2DestroyContext(int context) {
    (void)context;
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2RegisterContext(int context, int handle, uint64_t options) {
    (void)context;
    (void)handle;
    (void)options;
    return SCE_NP_TROPHY2_OK;
}

int APS5_VABI sceNpTrophy2UnregisterUnlockCallback() {
    return SCE_NP_TROPHY2_OK;
}


int APS5_VABI sceNpTrophy2GetRewardIcon(int context, int handle, int trophy_id, void* buffer, size_t* size) {
    (void)context;
    (void)handle;
    (void)trophy_id;
    (void)buffer;
    if (size != nullptr) {
        *size = NP_TROPHY2_ICON_SIZE_NONE;
    }
    throw std::runtime_error(std::string(__func__) + ": icon file not found");
}

int APS5_VABI sceNpTrophy2ShowTrophyList(int context) {
    (void)context;
    return SCE_NP_TROPHY2_OK;
}

}

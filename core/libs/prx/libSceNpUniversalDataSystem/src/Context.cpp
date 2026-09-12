#include <cstdint>
#include <stdexcept>

#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateContext(int* context, int user_id, uint32_t service_label, uint64_t options) {
    if (context == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": invalid argument context");
    }
    *context = NP_UNIVERSAL_DATA_SYSTEM_CONTEXT_DEFAULT;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemDestroyContext(int context) {
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemRegisterContext(int context, int handle, uint64_t options) {
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}

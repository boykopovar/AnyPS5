#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpUniversalDataSystemCreateContext(int* context, int user_id, uint32_t service_label, uint64_t options) {
    (void)context;
    (void)user_id;
    (void)service_label;
    (void)options;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemDestroyContext(int context) {
    (void)context;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUniversalDataSystemRegisterContext(int context, int handle, uint64_t options) {
    (void)context;
    (void)handle;
    (void)options;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

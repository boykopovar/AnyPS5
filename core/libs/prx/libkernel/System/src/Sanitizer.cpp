#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceKernelIsAddressSanitizerEnabled(void) {
    return 0;
}

int APS5_VABI sceKernelIsThreadSanitizerEnabled(void) {
    return 0;
}

MallocReplace* APS5_VABI sceKernelGetSanitizerMallocReplaceExternal(void) {
    return nullptr;
}

NewReplace* APS5_VABI sceKernelGetSanitizerNewReplaceExternal(void) {
    return nullptr;
}

}

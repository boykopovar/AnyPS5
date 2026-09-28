#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceKernelIsAddressSanitizerEnabled(void) {
    return 0;
}

MallocReplace* APS5_VABI sceKernelGetSanitizerMallocReplaceExternal(void) {
    static MallocReplace replacement{};
    return &replacement;
}

NewReplace* APS5_VABI sceKernelGetSanitizerNewReplaceExternal(void) {
    static NewReplace replacement{};
    return &replacement;
}

}

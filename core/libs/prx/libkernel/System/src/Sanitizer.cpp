#include "SceTypes.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {

std::uintptr_t __stack_chk_guard_nid_postfix = 0xDEADBEEFCAFEBABEull;

[[noreturn]] void APS5_VABI __stack_chk_fail_nid_postfix(void) {
    std::abort();
}

int APS5_VABI sceKernelIsAddressSanitizerEnabled(void) {
    return 0;
}

int APS5_VABI sceKernelIsThreadSanitizerEnabled(void) {
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

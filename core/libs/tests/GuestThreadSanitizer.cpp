#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" int APS5_VABI sceKernelIsThreadSanitizerEnabled(void);
extern "C" std::uintptr_t __stack_chk_guard_nid_postfix;

int main() {
    if (sceKernelIsThreadSanitizerEnabled() != 0) std::abort();
    if (__stack_chk_guard_nid_postfix == 0) std::abort();
    return 0;
}

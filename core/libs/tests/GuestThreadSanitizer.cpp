#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceKernelIsThreadSanitizerEnabled(void);
void APS5_VABI __tsan_acquire(void*);
void APS5_VABI __tsan_release(void*);
}

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    Require(sceKernelIsThreadSanitizerEnabled() == 0);
    int dummy = 42;
    __tsan_acquire(nullptr);
    __tsan_acquire(&dummy);
    __tsan_release(nullptr);
    __tsan_release(&dummy);
    Require(dummy == 42);
}

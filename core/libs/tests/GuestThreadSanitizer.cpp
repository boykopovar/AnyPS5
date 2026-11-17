#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

#include "prx/libSceDbgThreadSanitizer/include/Api.hpp"
#include "Testing.hpp"

int main() {
    Require(sceKernelIsThreadSanitizerEnabled() == 0);
    int dummy = 42;
    __tsan_acquire(nullptr);
    __tsan_acquire(&dummy);
    __tsan_release(nullptr);
    __tsan_release(&dummy);
    Require(dummy == 42);
}

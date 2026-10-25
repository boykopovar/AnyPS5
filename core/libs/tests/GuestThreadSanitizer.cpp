#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

int main() {
    if (sceKernelIsThreadSanitizerEnabled() != 0) std::abort();
    return 0;
}

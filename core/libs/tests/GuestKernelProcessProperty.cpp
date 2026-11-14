#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Process property check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

int main() {
    Require(sceKernelSetProcessProperty(nullptr, 1, 2, 3, 4) == SCE_KERNEL_ERROR_EINVAL);

    char name[32] = {};
    std::memcpy(name, "test.property.alpha", 19);
    Require(sceKernelSetProcessProperty(name, 0x1000, 0x2000, 0x3000, 0x4000) == 0);
    Require(sceKernelSetProcessProperty(name, -1, -2, -3, -4) == 0);

    std::memset(name, 0, sizeof(name));
    std::memcpy(name, "test.property.beta", 18);
    Require(sceKernelSetProcessProperty(name, 0, 0, 0, 0) == 0);

    std::memset(name, 0xff, sizeof(name));
    Require(sceKernelSetProcessProperty(name, 1, 2, 3, 4) == 0);
    return 0;
}

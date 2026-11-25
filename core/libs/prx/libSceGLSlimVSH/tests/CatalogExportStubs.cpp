#include "prx/libSceGLSlimVSH/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

using ThrowingStub = int (APS5_VABI *)();

constexpr ThrowingStub kStubs[] = {
    sceSlimglClientLiveTimeout,
    sceSlimglClientMonitorServerEnd,
    sceSlimglClientMonitorServerStart,
    sceSlimglClientPulse,
    sceSlimglSupportServerProcess,
};

constexpr const char* kNames[] = {
    "sceSlimglClientLiveTimeout",
    "sceSlimglClientMonitorServerEnd",
    "sceSlimglClientMonitorServerStart",
    "sceSlimglClientPulse",
    "sceSlimglSupportServerProcess",
};

}

int main() {
    static_assert(sizeof(kStubs) / sizeof(kStubs[0]) == 5, "catalog declaration count changed");
    static_assert(sizeof(kNames) / sizeof(kNames[0]) == sizeof(kStubs) / sizeof(kStubs[0]), "name table size differs from stub table");
    for (std::size_t i = 0; i < sizeof(kStubs) / sizeof(kStubs[0]); ++i) {
        try {
            kStubs[i]();
        } catch (const std::runtime_error&) {
            continue;
        }
        std::fprintf(stderr, "libSceGLSlimVSH catalog stub did not throw: %s\n", kNames[i]);
        std::abort();
    }
    return 0;
}

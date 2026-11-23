#include "prx/libSceNet/include/Net.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

template <typename TCall>
void RequireThrows(TCall call, const char* message) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return;
    }
    Require(false, message);
}

}

int main() {
    RequireThrows([] {
        sceNetGetAppMemInfo(reinterpret_cast<NetAppMemoryInfo*>(0x10), 0);
    }, "sceNetGetAppMemInfo must remain explicitly unsupported");
    RequireThrows([] {
        sceNetGetSystemMemInfo(reinterpret_cast<NetSystemMemoryInfo*>(0x10), 0);
    }, "sceNetGetSystemMemInfo must remain explicitly unsupported");
    RequireThrows([] {
        sceNetGetIfListEx(1, reinterpret_cast<NetInterfaceInfo*>(0x10), 1, 0);
    }, "sceNetGetIfListEx must remain explicitly unsupported");
}
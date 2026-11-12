#include "prx/libSceIpmi/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceIpmi no-op handler: %s\n", message);
    std::abort();
}

using Handler = void (APS5_VABI*)(void*);

}

int main() {
    const Handler handlers[] = {
        _ZN4IPMI7SessionD1Ev,
        _ZN4IPMI7SessionD2Ev,
        _ZN4IPMI6ClientD1Ev,
        _ZN4IPMI6ClientD2Ev,
        _ZN4IPMI6ServerD1Ev,
        _ZN4IPMI6ServerD2Ev,
        _ZN4IPMI6Server12EventHandlerD1Ev,
        _ZN4IPMI6Server12EventHandlerD2Ev,
        _ZN4IPMI6Server12EventHandler15onSessionKilledEPNS_7SessionE,
        _ZN4IPMI6Server12EventHandler20onSyncMethodDispatchEPNS_7SessionEjPvmmS4_m,
        _ZN4IPMI6Server12EventHandler20onSyncMethodDispatchEPNS_7SessionEjPKNS_8DataInfoEjPNS_10BufferInfoEj,
        _ZN4IPMI6Server12EventHandler21onAsyncMethodDispatchEPNS_7SessionEjjPvmS4_m,
        _ZN4IPMI6Server12EventHandler21onAsyncMethodDispatchEPNS_7SessionEjjPKNS_8DataInfoEj,
    };

    for (const Handler handler : handlers) {
        alignas(8) unsigned char object[0x100];
        for (std::size_t index = 0; index < sizeof(object); ++index) object[index] = 0x5a;
        handler(object);
        for (std::size_t index = 0; index < sizeof(object); ++index) {
            Require(object[index] == 0x5a, "the handler leaves the object untouched");
        }
    }

    std::puts("libSceIpmi no-op handler checks passed");
    return 0;
}

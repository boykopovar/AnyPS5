#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdlib>
#include <cstring>

namespace {

std::array<unsigned char, 64> Filled(unsigned char fill) {
    std::array<unsigned char, 64> bytes{};
    std::memset(bytes.data(), fill, bytes.size());
    return bytes;
}

void RequireUnchanged(const std::array<unsigned char, 64>& bytes, unsigned char fill) {
    for (const auto byte : bytes) {
        if (byte != fill) std::abort();
    }
}

}

int main() {
    using Destructor = void (APS5_VABI *)(void*);
    constexpr unsigned char fills[] = {0x00, 0x5A, 0xFF};
    for (const unsigned char fill : fills) {
        for (const Destructor destructor : {
                 _ZN3JSC2VM10ClientDataD1Ev,
                 _ZN7bmalloc8FreeListD2Ev,
                 _ZN9Inspector32DebuggerBackendDispatcherHandlerD2Ev,
                 _ZN9Inspector32TimelineBackendDispatcherHandlerD1Ev,
             }) {
            auto object = Filled(fill);
            destructor(object.data());
            destructor(object.data());
            RequireUnchanged(object, fill);
        }

        auto agent = Filled(fill);
        auto router = Filled(fill);
        auto dispatcher = Filled(fill);
        _ZN9Inspector22InspectorDebuggerAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE(agent.data(), router.data(), dispatcher.data());
        _ZN9Inspector22InspectorDebuggerAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE(agent.data(), nullptr, nullptr);
        RequireUnchanged(agent, fill);
        RequireUnchanged(router, fill);
        RequireUnchanged(dispatcher, fill);
    }
}

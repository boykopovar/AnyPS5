#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libSceJsc/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

std::array<unsigned char, 64> Filled() {
    std::array<unsigned char, 64> bytes{};
    std::memset(bytes.data(), 0x5A, bytes.size());
    return bytes;
}

void RequireUnchanged(const std::array<unsigned char, 64>& bytes) {
    for (const auto byte : bytes) {
        if (byte != 0x5A) std::abort();
    }
}

} // namespace

int main() {
    using InventoryStub = int (APS5_VABI*)();
    for (const InventoryStub stub : {
             libSceJscInventory_TYvNAgirhfM_nid_no_patch,
             libSceJscInventory_4AXLxzY843A_nid_no_patch,
             libSceJscInventory_AnZ5fEIIS40_nid_no_patch,
             libSceJscInventory_GtiYvkQcGr4_nid_no_patch,
             libSceJscInventory_RD9UWM_plus_gJiI_nid_no_patch,
             libSceJscInventory_xu3rtZhKLb0_nid_no_patch,
         }) {
        try {
            stub();
        } catch (const std::runtime_error&) {
            continue;
        }
        std::abort();
    }

    using Destructor = void (APS5_VABI *)(void*);
    for (const Destructor destructor : {
             _ZN3JSC2VM10ClientDataD1Ev,
             _ZN7bmalloc8FreeListD2Ev,
             _ZN9Inspector32DebuggerBackendDispatcherHandlerD2Ev,
             _ZN9Inspector32TimelineBackendDispatcherHandlerD1Ev,
         }) {
        auto object = Filled();
        destructor(object.data());
        RequireUnchanged(object);
    }

    auto agent = Filled();
    auto router = Filled();
    auto dispatcher = Filled();
    _ZN9Inspector22InspectorDebuggerAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE(agent.data(), router.data(), dispatcher.data());
    _ZN9Inspector22InspectorDebuggerAgent27didCreateFrontendAndBackendEPNS_14FrontendRouterEPNS_17BackendDispatcherE(agent.data(), nullptr, nullptr);
    RequireUnchanged(agent);
    RequireUnchanged(router);
    RequireUnchanged(dispatcher);
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/InventoryExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    _ZN7WebCore22EmptyFrameLoaderClient33dispatchWillPerformClientRedirectERKN3WTF3URLEdNS1_8WallTimeENS_19LockBackForwardListE();
    if (NotImplementedCalls_nid_no_patch() != before) {
        std::fprintf(stderr, "dispatchWillPerformClientRedirect recorded an unimplemented call\n");
        std::abort();
    }
    return 0;
}

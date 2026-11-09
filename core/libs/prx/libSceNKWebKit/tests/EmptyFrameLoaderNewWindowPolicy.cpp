#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    _ZN7WebCore22EmptyFrameLoaderClient38dispatchDecidePolicyForNewWindowActionERKNS_16NavigationActionERKNS_15ResourceRequestEPNS_9FormStateERKN3WTF6StringENS_21PolicyCheckIdentifierEONS9_8FunctionIFvNS_12PolicyActionESD_EEE();
    if (NotImplementedCalls_nid_no_patch() != before) {
        std::fprintf(stderr, "dispatchDecidePolicyForNewWindowAction recorded an unimplemented call\n");
        std::abort();
    }
    return 0;
}

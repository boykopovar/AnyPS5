#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    _ZN7WebCore22EmptyFrameLoaderClient39dispatchDecidePolicyForNavigationActionERKNS_16NavigationActionERKNS_15ResourceRequestERKNS_16ResourceResponseEPNS_9FormStateENS_18PolicyDecisionModeENS_21PolicyCheckIdentifierEON3WTF8FunctionIFvNS_12PolicyActionESD_EEE();
    if (NotImplementedCalls_nid_no_patch() != before) {
        std::fprintf(stderr, "dispatchDecidePolicyForNavigationAction recorded an unimplemented call\n");
        std::abort();
    }
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    _ZN7WebCore22EmptyFrameLoaderClient27dispatchWillSendSubmitEventEON3WTF3RefINS_9FormStateENS1_13DumbPtrTraitsIS3_EEEE();
    if (NotImplementedCalls_nid_no_patch() != before) {
        std::fprintf(stderr, "dispatchWillSendSubmitEvent recorded an unimplemented call\n");
        std::abort();
    }
    return 0;
}

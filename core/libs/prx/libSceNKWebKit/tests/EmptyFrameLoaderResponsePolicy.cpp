#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/LegacyExports.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" std::uint64_t APS5_VABI NotImplementedCalls_nid_no_patch();

int main() {
    const auto before = NotImplementedCalls_nid_no_patch();
    _ZN7WebCore22EmptyFrameLoaderClient31dispatchDecidePolicyForResponseERKNS_16ResourceResponseERKNS_15ResourceRequestENS_21PolicyCheckIdentifierERKN3WTF6StringEONS8_8FunctionIFvNS_12PolicyActionES7_EEE();
    if (NotImplementedCalls_nid_no_patch() != before) {
        std::fprintf(stderr, "dispatchDecidePolicyForResponse recorded an unimplemented call\n");
        std::abort();
    }
    return 0;
}

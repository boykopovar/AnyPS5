#include "prx/libSceVrHand_nosubmission/libSceVrHand_nosubmission.hpp"
#include <cstdio>

namespace {

int failures = 0;

template <typename TFn>
void RequireThrows(TFn fn, const char *name) {
    try {
        fn();
    } catch (...) {
        return;
    }
    std::fprintf(stderr, "%s did not throw\n", name);
    ++failures;
}

}

int main() {
    RequireThrows(sceVrHandDebugGetConfig_nid_no_patch, "sceVrHandDebugGetConfig");
    RequireThrows(sceVrHandDebugGetInternalResult_nid_no_patch, "sceVrHandDebugGetInternalResult");
    RequireThrows(sceVrHandDebugSetConfig_nid_no_patch, "sceVrHandDebugSetConfig");
    RequireThrows(sceVrHandSetDebugInterface_nid_no_patch, "sceVrHandSetDebugInterface");
    return failures == 0 ? 0 : 1;
}

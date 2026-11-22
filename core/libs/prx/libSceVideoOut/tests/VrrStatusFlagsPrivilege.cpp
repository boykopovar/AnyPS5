#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

int main() {
    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0,
            "sceVideoOutAddVrrStatusFlagsPrivilege did not return SCE_VIDEO_OUT_OK");
    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0,
            "sceVideoOutAddVrrStatusFlagsPrivilege changed its answer on a second call");
    return 0;
}

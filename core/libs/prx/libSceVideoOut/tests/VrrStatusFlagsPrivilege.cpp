#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Require(bool value, const char* reason) {
    if (!value) {
        std::fprintf(stderr, "%s\n", reason);
        std::abort();
    }
}

static bool ThrowsVrrStatusFlagsPrivilege() {
    try {
        sceVideoOutAddVrrStatusFlagsPrivilege();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    Require(ThrowsVrrStatusFlagsPrivilege(), "sceVideoOutAddVrrStatusFlagsPrivilege did not throw");
    return 0;
}

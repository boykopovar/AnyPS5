#include "prx/libSceHttp/include/Http.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

}

int main() {
    for (int call = 0; call < 3; ++call) {
        Require(sceHttpDbgSetPrintf() == 0, "sceHttpDbgSetPrintf did not answer success");
        Require(sceHttpDbgEnableProfile() == 0, "sceHttpDbgEnableProfile did not answer success");
    }
    return 0;
}

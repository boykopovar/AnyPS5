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

} // namespace

int main() {
    Require(sceHttpDbgSetPrintf() == 0, "sceHttpDbgSetPrintf did not answer success");
    Require(sceHttpDbgSetPrintf() == 0, "sceHttpDbgSetPrintf did not repeat success");
    return 0;
}

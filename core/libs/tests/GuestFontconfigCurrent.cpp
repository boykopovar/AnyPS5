#include "prx/libfontconfig/include/Configuration.hpp"

#include <cstdio>
#include <cstdlib>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    Require(FcConfigGetCurrent() == nullptr, "FcConfigGetCurrent returns no configuration");
    Require(FcConfigGetCurrent() == nullptr, "a second FcConfigGetCurrent still returns no configuration");
    Require(FcInitBringUptoDate() == 1, "FcInitBringUptoDate reports the configuration up to date");
    Require(FcInitBringUptoDate() == 1, "a second FcInitBringUptoDate still reports it up to date");
    Require(FcConfigGetCurrent() == nullptr, "FcInitBringUptoDate creates no current configuration");
    Require(FcInitBringUptoDate() == 1, "a third FcInitBringUptoDate still reports it up to date");
    Require(FcConfigGetCurrent() == nullptr, "configuration pointer remains null after repeated update calls");
}

#include "prx/libSceNetCtl/libSceNetCtl.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "NetCtl Term check failed at line %u\n", location.line());
    std::abort();
}

static void APS5_VABI NoopCallback(int, void*) {
}

int main() {
    Require(sceNetCtlInit() == 0);

    int callbackId = -1;
    Require(sceNetCtlRegisterCallback(NoopCallback, nullptr, &callbackId) == 0 && callbackId >= 0);
    sceNetCtlTerm();
    sceNetCtlTerm();

    int state = -1;
    int stateV6 = -1;
    Require(sceNetCtlGetState(&state) == 0);
    Require(sceNetCtlGetStateV6(&stateV6) == 0);
    Require(sceNetCtlUnregisterCallback(callbackId) == 0);
    return 0;
}

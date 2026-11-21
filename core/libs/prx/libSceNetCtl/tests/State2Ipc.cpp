#include "prx/libSceNetCtl/libSceNetCtl.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

constexpr int NetCtlErrorNotInitialized = static_cast<int>(0x80412101);
constexpr int NetCtlErrorInvalidAddr = static_cast<int>(0x80412107);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest_net_ctl_state2_ipc: %s\n", message);
    std::abort();
}

}

int main() {
    int state = -1;
    int state2 = -1;
    Require(sceNetCtlInit() == 0, "sceNetCtlInit must succeed");
    Require(sceNetCtlGetState2IpcInt(nullptr) == NetCtlErrorInvalidAddr, "null state pointer must return INVALID_ADDR");
    Require(sceNetCtlGetState(&state) == 0, "sceNetCtlGetState must succeed");
    Require(sceNetCtlGetState2IpcInt(&state2) == 0, "sceNetCtlGetState2IpcInt must succeed");
    Require(state2 == state, "sceNetCtlGetState2IpcInt must match sceNetCtlGetState");
    sceNetCtlTerm();
    return 0;
}

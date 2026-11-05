#include "prx/libkernel_sys/include/Api.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>

namespace {

void Require(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

bool Throws(void (*action)()) {
    try {
        action();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

}

int main() {
    constexpr int invalid = static_cast<int>(0x80020016u);
    int mode = -1;
    Require(sceKernelGetCpumodeGame(&mode) == 0 && mode == 1, "the game CPU mode does not start at 1");
    Require(sceKernelSetCpumodeGame(0) == 0, "mode 0 was refused");
    Require(sceKernelGetCpumodeGame(&mode) == 0 && mode == 0, "mode 0 was not reported back");
    for (int refused = 2; refused <= 7; ++refused) {
        Require(sceKernelSetCpumodeGame(refused) == invalid, "a mode from 2 to 7 was not refused with EINVAL");
        Require(sceKernelGetCpumodeGame(&mode) == 0 && mode == 0, "a refused mode changed the game CPU mode");
    }
    Require(sceKernelSetCpumodeGame(1) == 0, "mode 1 was refused");
    Require(sceKernelGetCpumodeGame(&mode) == 0 && mode == 1, "mode 1 was not reported back");
    Require(Throws([] { sceKernelSetCpumodeGame(8); }), "an unmeasured mode did not throw");
    Require(Throws([] { sceKernelSetCpumodeGame(-1); }), "a negative mode did not throw");
    Require(Throws([] { sceKernelGetCpumodeGame(nullptr); }), "a null output did not throw");
    std::puts("game CPU mode tests passed");
    return 0;
}

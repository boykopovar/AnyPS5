#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <stdexcept>

static void Require(bool value) { if (!value) std::abort(); }
static bool Throws(int* mode, int* submode) {
    try { sceKernelGetOperationMode(mode, submode); } catch (const std::invalid_argument&) { return true; }
    return false;
}
int main() {
    int out[4] = {-1, -1, -1, -1};
    sceKernelGetOperationMode(&out[0], &out[2]);

    Require(out[0] == 2 && out[1] == -1);
    Require(out[2] == 0 && out[3] == -1);
    int value = -1;
    Require(Throws(nullptr, &value) && Throws(&value, nullptr) && Throws(nullptr, nullptr));
    Require(value == -1);
    Require(sceKernelIsTrinityMode() == 0);
    Require(sceKernelHasTrinityMode() == 0);
}

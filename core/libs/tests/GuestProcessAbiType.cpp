#include "prx/libkernel/include/Api.hpp"
#include <cstdlib>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    const int self = getpid_nid_postfix();
    Require(sceKernelGetProcessAbiType(self) == 1);
    Require(sceKernelGetProcessAbiType(-1) == 1);
    Require(sceKernelGetProcessAbiType(0) == 0);
    Require(sceKernelGetProcessAbiType(-2) == static_cast<int>(0x80020016));
    Require(sceKernelGetProcessAbiType(-100) == static_cast<int>(0x80020016));
    Require(sceKernelGetProcessAbiType(999999) == static_cast<int>(0x80020003));
    Require(sceKernelGetProcessAbiType(self + 1) == static_cast<int>(0x80020003));
    return 0;
}

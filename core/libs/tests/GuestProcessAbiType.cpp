#include "prx/libkernel/include/Api.hpp"
#include "Testing.hpp"
#include <cstdlib>

int main() {
    const int self = getpid_nid_postfix();
    Require(sceKernelGetProcessAbiType(self) == 1);
    Require(sceKernelGetProcessAbiType(-1) == 1);
    Require(sceKernelGetProcessAbiType(0) == 0);
    return 0;
}

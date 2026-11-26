#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstdlib>

static void Require(bool value) {
    if (!value) std::abort();
}

int main() {
    Require(sceKernelGetSanitizerMallocReplaceExternal() == nullptr);
    Require(sceKernelGetSanitizerNewReplaceExternal() == nullptr);
}

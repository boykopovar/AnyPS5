#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceNpEAAccessInitialize(void);
int APS5_VABI sceNpEAAccessTerminate(void);
}

namespace {
void Require(bool value) { if (!value) std::abort(); }
}

int main() {
    Require(sceNpEAAccessInitialize() == 0);
    Require(sceNpEAAccessTerminate() == 0);
}

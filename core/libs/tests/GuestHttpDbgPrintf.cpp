#include "prx/libSceHttp/include/Http.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cstdio>
#include <cstdlib>

int main() {
    Require(sceHttpDbgSetPrintf() == 0, "sceHttpDbgSetPrintf did not answer success");
    Require(sceHttpDbgSetPrintf() == 0, "sceHttpDbgSetPrintf did not repeat success");
    return 0;
}

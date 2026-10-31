#include "prx/libSceSystemLogger/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    Require(sceSystemLoggerInitialize() == 0);
    Require(sceSystemLoggerSetLogData() == 0);
    Require(sceSystemLoggerTerminate() == 0);

    Require(sceSystemLoggerInitialize() == 0);
    Require(sceSystemLoggerTerminate() == 0);

    return 0;
}

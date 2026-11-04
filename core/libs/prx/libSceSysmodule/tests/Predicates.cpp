#include "prx/libSceSysmodule/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    Require(sceSysmoduleIsCalledFromSysModule() == 0);
    Require(sceSysmoduleIsCameraPreloaded() == 0);
}

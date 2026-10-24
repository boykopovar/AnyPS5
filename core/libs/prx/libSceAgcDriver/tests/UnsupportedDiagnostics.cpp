#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"

int main() {
    if (sceAgcDriverGetCoredumpAddress() != nullptr) return 1;
    if (sceAgcDriverSetTargetRingForDiag() != static_cast<int>(0x8A6D0001)) return 2;
    return 0;
}

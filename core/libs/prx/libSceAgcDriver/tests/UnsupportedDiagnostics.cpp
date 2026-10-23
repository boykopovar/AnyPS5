#include "SceTypes.hpp"

extern "C" void* APS5_VABI sceAgcDriverGetCoredumpAddress();
extern "C" int APS5_VABI sceAgcDriverSetTargetRingForDiag();

int main() {
    if (sceAgcDriverGetCoredumpAddress() != nullptr) return 1;
    if (sceAgcDriverSetTargetRingForDiag() != static_cast<int>(0x8A6D0001)) return 2;
    return 0;
}

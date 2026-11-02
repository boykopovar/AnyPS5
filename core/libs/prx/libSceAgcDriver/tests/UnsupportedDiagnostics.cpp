#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"

int main() {
    if (sceAgcDriverGetCoredumpAddress() != nullptr) return 1;
    if (sceAgcDriverSetTargetRingForDiag() != static_cast<int>(0x8A6D0001)) return 2;
    auto* volatile linked = &sceAgcDriverSetFlip;
    if (linked == nullptr) return 3;
    if (sceAgcDriverAcquireRazorACQ() != static_cast<int>(0x8A6D0001)) return 4;
    if (sceAgcDriverGetPaDebugInterfaceVersion() != static_cast<int>(0x8A6D0001)) return 5;
    if (sceAgcDriverSetHsOffchipParamDirect() != static_cast<int>(0x8A6D0001)) return 6;
    if (sceAgcDriverSubmitMultiCommandBuffersDirect() != static_cast<int>(0x8A6D0001)) return 7;
    if (sceAgcDriverSubmitToRazorACQ() != static_cast<int>(0x8A6D0001)) return 8;
    if (sceAgcDriverSuspendPointSubmitDirect() != static_cast<int>(0x8A6D0001)) return 9;
    if (sceAgcDriverModuleRegistration() != static_cast<int>(0x8A6D0003)) return 10;
    return 0;
}

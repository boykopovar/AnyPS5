#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "SceTypes.hpp"
#include "tests/ExportLookup.hpp"

int main(int argc, char** argv) {
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
    struct Export {
        int (APS5_VABI* function)(void);
        const char* name;
    };
    const Export exports[] = {
        {sceAgcDriverCreateQueue, "sceAgcDriverCreateQueue"},
        {sceAgcDriverCwsrResumeAcq, "sceAgcDriverCwsrResumeAcq"},
        {sceAgcDriverCwsrSuspendAcq, "sceAgcDriverCwsrSuspendAcq"},
        {sceAgcDriverDebugHardwareStatus, "sceAgcDriverDebugHardwareStatus"},
        {sceAgcDriverDestroyQueue, "sceAgcDriverDestroyQueue"},
        {sceAgcDriverGetRegShadowInfo, "sceAgcDriverGetRegShadowInfo"},
        {sceAgcDriverGetRegShadowInfoAgr, "sceAgcDriverGetRegShadowInfoAgr"},
        {sceAgcDriverGetReservedDmemForAgc, "sceAgcDriverGetReservedDmemForAgc"},
        {sceAgcDriverGetWorkloadStreamInfo, "sceAgcDriverGetWorkloadStreamInfo"},
        {sceAgcDriverIDHSSubmit, "sceAgcDriverIDHSSubmit"},
        {sceAgcDriverNotifyDefaultStates, "sceAgcDriverNotifyDefaultStates"},
        {sceAgcDriverPassInfoDownward, "sceAgcDriverPassInfoDownward"},
        {sceAgcDriverPatchClearState, "sceAgcDriverPatchClearState"},
        {sceAgcDriverSetWorkloadComplete, "sceAgcDriverSetWorkloadComplete"},
        {sceAgcDriverSetWorkloadsActive, "sceAgcDriverSetWorkloadsActive"},
        {sceAgcDriverSetupAsyncGraphics, "sceAgcDriverSetupAsyncGraphics"},
        {sceAgcDriverSetupRegisterShadow, "sceAgcDriverSetupRegisterShadow"},
        {sceAgcDriverSubmitToHDRScopesACQ, "sceAgcDriverSubmitToHDRScopesACQ"},
        {sceAgcDriverSuspendPointSubmit, "sceAgcDriverSuspendPointSubmit"},
        {sceAgcDriverSysEnableSubmitDone45Exception, "sceAgcDriverSysEnableSubmitDone45Exception"},
        {sceAgcDriverSysGetClientNumber, "sceAgcDriverSysGetClientNumber"},
        {sceAgcDriverSysIsGameClosed, "sceAgcDriverSysIsGameClosed"},
        {sceAgcDriverTmpInitIdhs, "sceAgcDriverTmpInitIdhs"},
        {sceAgcDriverUserDataWritePacket, "sceAgcDriverUserDataWritePacket"},
        {sceAgcDriverUserDataWritePopMarker, "sceAgcDriverUserDataWritePopMarker"},
        {sceAgcDriverUserDataWritePushMarker, "sceAgcDriverUserDataWritePushMarker"},
        {sceAgcDriverUserDataWriteSetMarker, "sceAgcDriverUserDataWriteSetMarker"},
    };
    if (!ExportsResolveByName(argc > 1 ? argv[1] : nullptr, exports, "sceAgcDriverNotExported")) return 11;
    return 0;
}

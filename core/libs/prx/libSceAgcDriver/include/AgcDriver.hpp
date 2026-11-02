#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceAgcDriverAcquireRazorACQ(void);
int APS5_VABI sceAgcDriverCreateQueue(void);
int APS5_VABI sceAgcDriverCwsrResumeAcq(void);
int APS5_VABI sceAgcDriverCwsrSuspendAcq(void);
int APS5_VABI sceAgcDriverDebugHardwareStatus(void);
int APS5_VABI sceAgcDriverDestroyQueue(void);
int APS5_VABI sceAgcDriverGetPaDebugInterfaceVersion(void);
int APS5_VABI sceAgcDriverGetRegShadowInfo(void);
int APS5_VABI sceAgcDriverGetRegShadowInfoAgr(void);
int APS5_VABI sceAgcDriverGetReservedDmemForAgc(void);
int APS5_VABI sceAgcDriverGetWorkloadStreamInfo(void);
int APS5_VABI sceAgcDriverIDHSSubmit(void);
int APS5_VABI sceAgcDriverModuleRegistration(void);
int APS5_VABI sceAgcDriverNotifyDefaultStates(void);
int APS5_VABI sceAgcDriverPassInfoDownward(void);
int APS5_VABI sceAgcDriverPatchClearState(void);
int APS5_VABI sceAgcDriverSetFlip(void);
int APS5_VABI sceAgcDriverSetHsOffchipParamDirect(void);
int APS5_VABI sceAgcDriverSetWorkloadComplete(void);
int APS5_VABI sceAgcDriverSetWorkloadsActive(void);
int APS5_VABI sceAgcDriverSetupAsyncGraphics(void);
int APS5_VABI sceAgcDriverSetupRegisterShadow(void);
int APS5_VABI sceAgcDriverSubmitMultiCommandBuffersDirect(void);
int APS5_VABI sceAgcDriverSubmitToHDRScopesACQ(void);
int APS5_VABI sceAgcDriverSubmitToRazorACQ(void);
int APS5_VABI sceAgcDriverSuspendPointSubmit(void);
int APS5_VABI sceAgcDriverSuspendPointSubmitDirect(void);
int APS5_VABI sceAgcDriverSysEnableSubmitDone45Exception(void);
int APS5_VABI sceAgcDriverSysGetClientNumber(void);
int APS5_VABI sceAgcDriverSysIsGameClosed(void);
int APS5_VABI sceAgcDriverTmpInitIdhs(void);
int APS5_VABI sceAgcDriverUserDataWritePacket(void);
int APS5_VABI sceAgcDriverUserDataWritePopMarker(void);
int APS5_VABI sceAgcDriverUserDataWritePushMarker(void);
int APS5_VABI sceAgcDriverUserDataWriteSetMarker(void);
int APS5_VABI sceAgcDriverGetShaderDebuggingStatus();
int APS5_VABI sceAgcDriverRegisterDefaultOwner(uint32_t owner);
int APS5_VABI sceAgcDriverSetValidationErrorOutputFrequency(uint32_t frequency);
int APS5_VABI sceAgcDriverSetSubmitValidationMode(uint32_t mode);
int APS5_VABI sceAgcDriverGetSubmitValidationConfig(void* config);
int APS5_VABI sceAgcDriverGetSubmitValidationMode(uint32_t* mode);
int APS5_VABI sceAgcDriverSetSubmitValidationConfig(const void* config);
int APS5_VABI sceAgcDriverGetHsOffchipParam(uint16_t* first, uint16_t* second);
int APS5_VABI sceAgcDriverNopDataWritePacket(void* destination, uint32_t address, const void* data, uint32_t bytes);
int APS5_VABI sceAgcDriverSetValidationDisabled();
int APS5_VABI sceAgcDriverGetCaptureConfig();
int APS5_VABI sceAgcDriverSetCaptureConfig();
int APS5_VABI sceAgcDriverGetGpuPrintfWorkArea();
int APS5_VABI sceAgcDriverSetGpuPrintfEnabled();
int APS5_VABI sceAgcDriverDbgReadRegister();
int APS5_VABI sceAgcDriverDbgWriteRegister();
int APS5_VABI sceAgcDriverGetGpuRefClks();
int APS5_VABI sceAgcDriverIsSuspendPointInFlight();
int APS5_VABI sceAgcDriverIsPaDebug();
int APS5_VABI sceAgcDriverGetExceptionStatus();
int APS5_VABI sceAgcDriverGetTraceInitiator();
int APS5_VABI sceAgcDriverRegisterGpuResetCallbacks();
int APS5_VABI sceAgcDriverUnregisterGpuResetCallbacks();
int APS5_VABI sceAgdResetGpu();
int APS5_VABI sceAgcDriverSysSubmitFlipHandleProxy();
int APS5_VABI sceAgcDriverUserDataImmediateWrite();
int APS5_VABI sceAgcDriverUserDataImmediateWritePacket();
int APS5_VABI sceAgcDriverGetCoredumpMode(uint32_t* mode);
int APS5_VABI sceAgcDriverAllocateToolMemoryForGpuReset(uint64_t, uint64_t, uint64_t, void* range);
int APS5_VABI sceAgcDriverGetAllocatedToolMemoryForGpuReset(uint64_t, void* range);
int APS5_VABI sceAgcDriverAcquireHDRScopesACQ();
void* APS5_VABI sceAgcDriverGetCoredumpAddress();
int APS5_VABI sceAgcDriverHp3dLockEnd();
int APS5_VABI sceAgcDriverHp3dLockStart();
int APS5_VABI sceAgcDriverIsEsvmProcess();
int APS5_VABI sceAgcDriverReleaseHDRScopesACQ();
int APS5_VABI sceAgcDriverSetTargetRingForDiag();
int APS5_VABI sceAgcDriverSetValueToSharedGds();
int APS5_VABI sceAgcDriverSetWorkloadStreamInactive();
int APS5_VABI sceAgcDriverSubmitReprojectionAcb();
int APS5_VABI sceAgcDriverSuspendPointSubmitCdbg();
int APS5_VABI sceAgcDriverSysGetGfxAppGpuResetStatus();

}

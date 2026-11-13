#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceGnmSetPsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs);
int APS5_VABI sceGnmSetVsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier);
int APS5_VABI sceGnmSetCsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs);
int APS5_VABI sceGnmSetEsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier);
int APS5_VABI sceGnmSetGsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs);
int APS5_VABI sceGnmSetHsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs, std::uint32_t lsHsConfig);
int APS5_VABI sceGnmSetLsShader(std::uint32_t* cmd, std::uint32_t size, const std::uint32_t* regs, std::uint32_t modifier);
std::uint32_t APS5_VABI sceGnmDrawInitDefaultHardwareState175(std::uint32_t* cmd, std::uint32_t size);
std::uint32_t APS5_VABI sceGnmDispatchInitDefaultHardwareState(std::uint32_t* cmd, std::uint32_t size);
int APS5_VABI sceGnmInsertPushMarker(std::uint32_t* cmd, std::uint32_t size, const char* marker);
int APS5_VABI sceGnmInsertSetMarker(std::uint32_t* cmd, std::uint32_t size, const char* marker);
int APS5_VABI sceGnmInsertPushColorMarker(std::uint32_t* cmd, std::uint32_t size, const char* marker, std::uint32_t color);
int APS5_VABI sceGnmInsertPopMarker(std::uint32_t* cmd, std::uint32_t size);
int APS5_VABI sceGnmInsertSetColorMarker();
int APS5_VABI sceGnmInsertWaitFlipDone(std::uint32_t* cmd, std::uint32_t size, std::uint32_t handle, std::uint32_t index);
int APS5_VABI sceGnmSubmitCommandBuffers(std::uint32_t count, std::uint32_t* const* dcbs, const std::uint32_t* dcbBytes, std::uint32_t* const* ccbs, const std::uint32_t* ccbBytes);
int APS5_VABI sceGnmSubmitAndFlipCommandBuffers(std::uint32_t count, std::uint32_t* const* dcbs, const std::uint32_t* dcbBytes, std::uint32_t* const* ccbs, const std::uint32_t* ccbBytes,
    std::uint32_t handle, std::uint32_t index, std::uint32_t flipMode, std::int64_t flipArg);
int APS5_VABI sceGnmSubmitDone();
int APS5_VABI sceGnmAreSubmitsAllowed();
void APS5_VABI sceGnmFlushGarlic();
int APS5_VABI sceGnmAddEqEvent(KernelEqueue eq, std::uint64_t id, void* udata);
int APS5_VABI sceGnmDeleteEqEvent(KernelEqueue eq, std::uint64_t id);
int APS5_VABI sceGnmGetEqEventType(const KernelEvent* ev);
int APS5_VABI sceGnmGetOffChipTessellationBufferSize_nid_disambig1(void);
bool APS5_VABI sceGnmDriverTraceInProgress_nid_disambig1(void);
bool APS5_VABI sceGnmIsUserPaEnabled();
bool APS5_VABI sceGnmDriverCaptureInProgress();
int APS5_VABI sceGnmDriverTriggerCapture();
int APS5_VABI sceGnmValidateCommandBuffers();
bool APS5_VABI sceRazorIsLoaded();
int APS5_VABI sceRazorCaptureImmediate();
int APS5_VABI sceRazorCaptureSinceLastFlip();
int APS5_VABI sceRazorCaptureCommandBuffersOnlyImmediate();
int APS5_VABI sceRazorCaptureCommandBuffersOnlySinceLastFlip();
int APS5_VABI sceGnmDebugHardwareStatus();
int APS5_VABI sceGnmDingDong();
int APS5_VABI sceGnmDisableMipStatsReport();
int APS5_VABI sceGnmGetEqTimeStamp();
std::uint64_t APS5_VABI sceGnmGetTheTessellationFactorRingBufferBaseAddress();
int APS5_VABI sceGnmLogicalCuIndexToPhysicalCuIndex();
std::uint32_t APS5_VABI sceGnmLogicalCuMaskToPhysicalCuMask(std::uint32_t, std::uint32_t mask);
int APS5_VABI sceGnmMapComputeQueue();
int APS5_VABI sceGnmRequestFlipAndSubmitDone();
int APS5_VABI sceGnmRequestMipStatsReportAndReset();
int APS5_VABI sceGnmSetGsRingSizes();
int APS5_VABI sceGnmSetupMipStatsReport();
int APS5_VABI sceGnmUnmapComputeQueue();
int APS5_VABI sceGnmSetEmbeddedPsShader_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t, std::uint32_t shaderId, std::uint32_t);
int APS5_VABI sceGnmSetEmbeddedVsShader_nid_disambig1(std::uint32_t* cmdbuf, std::uint32_t, std::uint32_t shaderId, std::uint32_t);
int APS5_VABI sceGnmDestroyWorkloadStream_nid_disambig1(void);
int APS5_VABI sceGnmDingDongForWorkload_nid_disambig1(std::uint32_t vqueueId, std::uint32_t nextOffsetDw, std::uint64_t workloadId);
int APS5_VABI sceGnmDrawIndexMultiInstanced_nid_disambig1(void);
int APS5_VABI sceGnmDrawOpaqueAuto_nid_disambig1(void);
int APS5_VABI sceGnmGetGpuBlockStatus_nid_disambig1(void);
int APS5_VABI sceGnmGetGpuInfoStatus_nid_disambig1(void);
int APS5_VABI sceGnmGetLastWaitedAddress_nid_disambig1(void);
int APS5_VABI sceGnmGetNumTcaUnits_nid_disambig1(void);
int APS5_VABI sceGnmGetProtectionFaultTimeStamp_nid_disambig1(void);
int APS5_VABI sceGnmGetShaderProgramBaseAddress_nid_disambig1(void);
int APS5_VABI sceGnmGetShaderStatus_nid_disambig1(void);
int APS5_VABI sceGnmInsertThreadTraceMarker_nid_disambig1(void);
int APS5_VABI sceGnmMapComputeQueueWithPriority_nid_disambig1(void);
int APS5_VABI sceGnmRequestFlipAndSubmitDoneForWorkload_nid_disambig1(void);
int APS5_VABI sceGnmSetWaveLimitMultipliers_nid_disambig1(void);
int APS5_VABI sceGnmSubmitAndFlipCommandBuffersForWorkload_nid_disambig1(void);
int APS5_VABI sceGnmSubmitCommandBuffersForWorkload_nid_disambig1(void);

}

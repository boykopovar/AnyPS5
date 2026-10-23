#include <cstdint>
#include <algorithm>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceGnmDriverForNeoMode/include/InitSequences.hpp"

namespace {

constexpr std::uint32_t InitPacketWords = 0x100;

template <std::size_t Words>
int WriteHardwareState(std::uint32_t* cmdbuf, std::uint32_t size, const std::array<std::uint32_t, Words>& state) {
    if (size < InitPacketWords) return 0;
    auto* at = std::copy(GnmNeo::ClearState.begin(), GnmNeo::ClearState.end(), cmdbuf);
    at = std::copy(state.begin(), state.end(), at);
    const auto padding = static_cast<std::uint32_t>(cmdbuf + InitPacketWords - at - 1);
    at[0] = 0xc0001000u | ((padding - 1) << 16);
    at[1] = 0;
    return InitPacketWords;
}

template <std::size_t Words>
int WriteContextState(std::uint32_t* cmdbuf, std::uint32_t size, std::uint32_t packetWords, const std::array<std::uint32_t, Words>& state) {
    if (size != packetWords) return 0;
    std::copy(state.begin(), state.end(), cmdbuf);
    return static_cast<int>(packetWords);
}

constexpr int Failure = static_cast<int>(0x8EEE00FFu);
constexpr int NoInterface = static_cast<int>(0x80000000u);
constexpr int ValidationNotEnabled = static_cast<int>(0x80D13FFFu);

} // namespace

extern "C" {

int APS5_VABI sceGnmBeginWorkload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmComputeWaitOnAddress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmComputeWaitSemaphore(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmCreateWorkloadStream(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDebugModuleReset(void) {
    return Failure;
}

int APS5_VABI sceGnmDebugReset(void) {
    return Failure;
}

int APS5_VABI sceGnmDestroyWorkloadStream(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDingDongForWorkload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDispatchDirect(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDispatchIndirect(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDispatchIndirectOnMec(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndexAuto(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndexIndirect(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndexIndirectCountMulti(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndexIndirectMulti(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndexMultiInstanced(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndexOffset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndirect(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndirectCountMulti(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawIndirectMulti(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawInitDefaultHardwareState(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteHardwareState(cmdbuf, size, GnmNeo::HardwareState);
}

int APS5_VABI sceGnmDrawInitDefaultHardwareState200(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteHardwareState(cmdbuf, size, GnmNeo::HardwareState200);
}

int APS5_VABI sceGnmDrawInitDefaultHardwareState350(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteHardwareState(cmdbuf, size, GnmNeo::HardwareState350);
}

int APS5_VABI sceGnmDrawInitToDefaultContextState(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteContextState(cmdbuf, size, 0x20, GnmNeo::ContextState);
}

int APS5_VABI sceGnmDrawInitToDefaultContextState400(std::uint32_t* cmdbuf, std::uint32_t size) {
    return WriteContextState(cmdbuf, size, 0x100, GnmNeo::ContextState400);
}

int APS5_VABI sceGnmDrawInitToDefaultContextStateInternalCommand(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawInitToDefaultContextStateInternalSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDrawOpaqueAuto(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterface(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForGpuDebugger(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForReplay(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForResourceRegistration(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForValidation(void) {
    return NoInterface;
}

int APS5_VABI sceGnmDriverInternalVirtualQuery(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmDriverTraceInProgress(void) {
    return 0;
}

int APS5_VABI sceGnmEndWorkload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmFindResources(void) {
    return Failure;
}

int APS5_VABI sceGnmFindResourcesPublic(void) {
    return Failure;
}

int APS5_VABI sceGnmGetCoredumpAddress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetCoredumpMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetCoredumpProtectionFaultTimestamp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetDbgGcHandle(void) {
    return -1;
}

int APS5_VABI sceGnmGetGpuBlockStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetGpuCoreClockFrequency(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetGpuInfoStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetLastWaitedAddress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetNumTcaUnits(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetOffChipTessellationBufferSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetOwnerName(void) {
    return Failure;
}

int APS5_VABI sceGnmGetPhysicalCounterFromVirtualized(void) {
    return Failure;
}

int APS5_VABI sceGnmGetProtectionFaultTimeStamp(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetResourceBaseAddressAndSizeInBytes(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceName(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceRegistrationBuffers(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceShaderGuid(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceType(void) {
    return Failure;
}

int APS5_VABI sceGnmGetResourceUserData(void) {
    return Failure;
}

int APS5_VABI sceGnmGetShaderProgramBaseAddress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGetShaderStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGpuPaDebugEnter(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmGpuPaDebugLeave(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmInsertDingDongMarker(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmInsertThreadTraceMarker(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmIsCoredumpValid(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmLogicalTcaUnitToPhysical(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmMapComputeQueueWithPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmPaDisableFlipCallbacks(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmPaEnableFlipCallbacks(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmPaHeartbeat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmQueryResourceRegistrationUserMemoryRequirements(void) {
    return Failure;
}

int APS5_VABI sceGnmRaiseUserExceptionEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmRegisterGdsResource(void) {
    return Failure;
}

int APS5_VABI sceGnmRegisterGnmLiveCallbackConfig(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmRegisterOwner(void) {
    return Failure;
}

int APS5_VABI sceGnmRegisterOwnerForSystem(void) {
    return Failure;
}

int APS5_VABI sceGnmRegisterResource(void) {
    return Failure;
}

int APS5_VABI sceGnmRequestFlipAndSubmitDoneForWorkload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmResetVgtControl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSdmaClose(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaConstFill(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaCopyLinear(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaCopyTiled(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaCopyWindow(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaFlush(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaGetMinCmdSize(void) {
    return Failure;
}

int APS5_VABI sceGnmSdmaOpen(void) {
    return Failure;
}

int APS5_VABI sceGnmSetCsShaderWithModifier(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSetEmbeddedPsShader(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSetEmbeddedVsShader(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSetPsShader350(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSetResourceRegistrationUserMemory(void) {
    return Failure;
}

int APS5_VABI sceGnmSetResourceUserData(void) {
    return Failure;
}

int APS5_VABI sceGnmSetSpiEnableSqCounters(void) {
    return Failure;
}

int APS5_VABI sceGnmSetSpiEnableSqCountersForUnitInstance(void) {
    return Failure;
}

int APS5_VABI sceGnmSetVgtControl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSetWaveLimitMultiplier(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSetWaveLimitMultipliers(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSubmitAndFlipCommandBuffersForWorkload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmSubmitCommandBuffersForWorkload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmUnregisterAllResourcesForOwner(void) {
    return Failure;
}

int APS5_VABI sceGnmUnregisterOwnerAndResources(void) {
    return Failure;
}

int APS5_VABI sceGnmUnregisterResource(void) {
    return Failure;
}

int APS5_VABI sceGnmUpdateGsShader(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmUpdateHsShader(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmUpdatePsShader(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmUpdatePsShader350(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmUpdateVsShader(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmValidateDisableDiagnostics(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateDisableDiagnostics2(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateDispatchCommandBuffers(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateDrawCommandBuffers(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateGetDiagnosticInfo(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateGetDiagnostics(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidateGetVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGnmValidateOnSubmitEnabled(void) {
    return 0;
}

int APS5_VABI sceGnmValidateResetState(void) {
    return ValidationNotEnabled;
}

int APS5_VABI sceGnmValidationRegisterMemoryCheckCallback(void) {
    return ValidationNotEnabled;
}

}

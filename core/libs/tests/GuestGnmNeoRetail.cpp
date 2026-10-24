#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <initializer_list>

extern "C" {
int APS5_VABI sceGnmDebugModuleReset(void);
int APS5_VABI sceGnmDebugReset(void);
int APS5_VABI sceGnmDriverInternalRetrieveGnmInterface(void);
int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForGpuDebugger(void);
int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForReplay(void);
int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForResourceRegistration(void);
int APS5_VABI sceGnmDriverInternalRetrieveGnmInterfaceForValidation(void);
int APS5_VABI sceGnmDriverTraceInProgress(void);
int APS5_VABI sceGnmFindResources(void);
int APS5_VABI sceGnmFindResourcesPublic(void);
int APS5_VABI sceGnmGetDbgGcHandle(void);
int APS5_VABI sceGnmGetOwnerName(void);
int APS5_VABI sceGnmGetPhysicalCounterFromVirtualized(void);
int APS5_VABI sceGnmGetResourceBaseAddressAndSizeInBytes(void);
int APS5_VABI sceGnmGetResourceName(void);
int APS5_VABI sceGnmGetResourceRegistrationBuffers(void);
int APS5_VABI sceGnmGetResourceShaderGuid(void);
int APS5_VABI sceGnmGetResourceType(void);
int APS5_VABI sceGnmGetResourceUserData(void);
int APS5_VABI sceGnmQueryResourceRegistrationUserMemoryRequirements(void);
int APS5_VABI sceGnmRegisterGdsResource(void);
int APS5_VABI sceGnmRegisterOwner(void);
int APS5_VABI sceGnmRegisterOwnerForSystem(void);
int APS5_VABI sceGnmRegisterResource(void);
int APS5_VABI sceGnmSdmaClose(void);
int APS5_VABI sceGnmSdmaConstFill(void);
int APS5_VABI sceGnmSdmaCopyLinear(void);
int APS5_VABI sceGnmSdmaCopyTiled(void);
int APS5_VABI sceGnmSdmaCopyWindow(void);
int APS5_VABI sceGnmSdmaFlush(void);
int APS5_VABI sceGnmSdmaGetMinCmdSize(void);
int APS5_VABI sceGnmSdmaOpen(void);
int APS5_VABI sceGnmSetResourceRegistrationUserMemory(void);
int APS5_VABI sceGnmSetResourceUserData(void);
int APS5_VABI sceGnmSetSpiEnableSqCounters(void);
int APS5_VABI sceGnmSetSpiEnableSqCountersForUnitInstance(void);
int APS5_VABI sceGnmUnregisterAllResourcesForOwner(void);
int APS5_VABI sceGnmUnregisterOwnerAndResources(void);
int APS5_VABI sceGnmUnregisterResource(void);
int APS5_VABI sceGnmValidateDisableDiagnostics(void);
int APS5_VABI sceGnmValidateDisableDiagnostics2(void);
int APS5_VABI sceGnmValidateDispatchCommandBuffers(void);
int APS5_VABI sceGnmValidateDrawCommandBuffers(void);
int APS5_VABI sceGnmValidateGetDiagnosticInfo(void);
int APS5_VABI sceGnmValidateGetDiagnostics(void);
int APS5_VABI sceGnmValidateOnSubmitEnabled(void);
int APS5_VABI sceGnmValidateResetState(void);
int APS5_VABI sceGnmValidationRegisterMemoryCheckCallback(void);
}

namespace {

constexpr int Failure = static_cast<int>(0x8EEE00FFu);
constexpr int NoInterface = static_cast<int>(0x80000000u);
constexpr int ValidationNotEnabled = static_cast<int>(0x80D13FFFu);

using Query = int (APS5_VABI *)(void);

void RequireAll(int expected, std::initializer_list<Query> queries) {
    for (const auto query : queries) {
        if (query() != expected) std::abort();
    }
}

} // namespace

int main() {
    RequireAll(Failure, {
        sceGnmDebugModuleReset,
        sceGnmDebugReset,
        sceGnmFindResources,
        sceGnmFindResourcesPublic,
        sceGnmGetOwnerName,
        sceGnmGetResourceBaseAddressAndSizeInBytes,
        sceGnmGetResourceName,
        sceGnmGetResourceRegistrationBuffers,
        sceGnmGetResourceShaderGuid,
        sceGnmGetResourceType,
        sceGnmGetResourceUserData,
        sceGnmQueryResourceRegistrationUserMemoryRequirements,
        sceGnmRegisterGdsResource,
        sceGnmRegisterOwner,
        sceGnmRegisterOwnerForSystem,
        sceGnmRegisterResource,
        sceGnmSetResourceRegistrationUserMemory,
        sceGnmSetResourceUserData,
        sceGnmUnregisterAllResourcesForOwner,
        sceGnmUnregisterOwnerAndResources,
        sceGnmUnregisterResource,
        sceGnmSdmaClose,
        sceGnmSdmaConstFill,
        sceGnmSdmaCopyLinear,
        sceGnmSdmaCopyTiled,
        sceGnmSdmaCopyWindow,
        sceGnmSdmaFlush,
        sceGnmSdmaGetMinCmdSize,
        sceGnmSdmaOpen,
        sceGnmGetPhysicalCounterFromVirtualized,
        sceGnmSetSpiEnableSqCounters,
        sceGnmSetSpiEnableSqCountersForUnitInstance,
    });
    RequireAll(NoInterface, {
        sceGnmDriverInternalRetrieveGnmInterface,
        sceGnmDriverInternalRetrieveGnmInterfaceForGpuDebugger,
        sceGnmDriverInternalRetrieveGnmInterfaceForReplay,
        sceGnmDriverInternalRetrieveGnmInterfaceForResourceRegistration,
        sceGnmDriverInternalRetrieveGnmInterfaceForValidation,
    });
    RequireAll(0, {
        sceGnmDriverTraceInProgress,
        sceGnmValidateOnSubmitEnabled,
    });
    RequireAll(-1, {
        sceGnmGetDbgGcHandle,
    });
    RequireAll(ValidationNotEnabled, {
        sceGnmValidateDisableDiagnostics,
        sceGnmValidateDisableDiagnostics2,
        sceGnmValidateDispatchCommandBuffers,
        sceGnmValidateDrawCommandBuffers,
        sceGnmValidateGetDiagnosticInfo,
        sceGnmValidateGetDiagnostics,
        sceGnmValidateResetState,
        sceGnmValidationRegisterMemoryCheckCallback,
    });
}

#include "prx/libSceGnmDriverForNeoMode/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <initializer_list>

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

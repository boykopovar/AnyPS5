#include "prx/libSceGnmDriverForNeoMode/include/Api.hpp"
#include "prx/libSceGnmDriverForNeoMode/include/InventoryExports.hpp"
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
    if (sceGnmRegisterOwner(nullptr, nullptr) != Failure) std::abort();
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
        sceGnmDriverInternalVirtualQuery,
        sceGnmGetCoredumpAddress,
        sceGnmIsDebugResetAvailable,
        sceGnmPaDisableFlipCallbacks,
        sceGnmPaEnableFlipCallbacks,
        sceGnmPaHeartbeat,
        sceGnmRaiseUserExceptionEvent,
        sceGnmValidateGetVersion,
        libSceGnmDriverForNeoModeUnknown__minus_bNK8kTFf6s_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_2T1zOhnddFQ_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_7LTGukH_plus_M1A_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_MLr_plus_Fyrxf_plus_8_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_QP7vDGU0xDQ_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_QWuQed5Mus4_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_UGePHM7rmgA_nid_no_patch,
    });
    RequireAll(Failure, {
        libSceGnmDriverForNeoModeUnknown__plus_RaJBCVJZVM_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_1TRGZJsC5Y4_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_5R1E24FRI4w_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_6a3_plus_Twrkogk_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_HEOIaxbuVTA_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_epUMKkeaze8_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_gQNwGezNDgE_nid_no_patch,
        libSceGnmDriverForNeoModeUnknown_v7QcBXR48L8_nid_no_patch,
    });
    libSceGnmDriverForNeoModeUnknown_8_plus_itHHi1ea0_nid_no_patch();
    libSceGnmDriverForNeoModeUnknown_KKsYtugjKLw_nid_no_patch();
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

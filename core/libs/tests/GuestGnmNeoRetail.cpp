#include "prx/libSceGnmDriverForNeoMode/include/Api.hpp"
#include "prx/libSceGnmDriverForNeoMode/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
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
        sceGnmGetCoredumpProtectionFaultTimestamp,
        sceGnmGetGpuBlockStatus,
        sceGnmGetGpuInfoStatus,
        sceGnmGetLastWaitedAddress,
        sceGnmGetProtectionFaultTimeStamp,
        sceGnmGetShaderProgramBaseAddress,
        sceGnmGetShaderStatus,
        sceGnmInsertThreadTraceMarker,
        sceGnmIsCoredumpValid,
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
    sceGnmGpuPaDebugEnter();
    sceGnmGpuPaDebugLeave();
    sceGnmRegisterGnmLiveCallbackConfig();
    std::uint32_t mode = 7;
    if (sceGnmGetCoredumpMode(&mode) != 0 || mode != 0) std::abort();
    if (sceGnmGetCoredumpMode(nullptr) != 1) std::abort();
    if (sceGnmDestroyWorkloadStream(1) != 0 || sceGnmDestroyWorkloadStream(15) != 0) std::abort();
    if (sceGnmDestroyWorkloadStream(0) != 1 || sceGnmDestroyWorkloadStream(16) != 1) std::abort();
    if (sceGnmGetNumTcaUnits() != 2) std::abort();
    if (sceGnmLogicalTcaUnitToPhysical(0) != 0 || sceGnmLogicalTcaUnitToPhysical(1) != 1) std::abort();
    const std::uint8_t multipliers[8] = {};
    if (sceGnmSetWaveLimitMultiplier(0, 1) != 1 || sceGnmSetWaveLimitMultipliers(0, multipliers) != 1) std::abort();
    if (libSceGnmDriverForNeoModeUnknown_sKhoi2ectC0_nid_no_patch() != 0) std::abort();
    if (libSceGnmDriverForNeoModeUnknown_R3TYO7Tdv5o_nid_no_patch() != 0) std::abort();
    if (libSceGnmDriverForNeoModeUnknown_sKhoi2ectC0_nid_no_patch() != 1) std::abort();
    if (libSceGnmDriverForNeoModeUnknown_ut57TBmRQN0_nid_no_patch() != 0) std::abort();
    if (libSceGnmDriverForNeoModeUnknown_sKhoi2ectC0_nid_no_patch() != 0) std::abort();
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

    constexpr int SubmitInvalid = static_cast<int>(0x80D11000u);
    constexpr int GpuUnavailable = static_cast<int>(0x80D110FFu);
    void* dcb[2] = {};
    std::uint32_t sizes[2] = {0x100, 0x200};
    std::uint32_t bad[2] = {0x100, 0};
    std::uint32_t big[2] = {0x100, 0x3ffffd};
    if (sceGnmSubmitCommandBuffersForWorkload(1, 2, dcb, sizes, nullptr, nullptr) != GpuUnavailable) std::abort();
    if (sceGnmSubmitCommandBuffersForWorkload(1, 0, nullptr, nullptr, nullptr, nullptr) != GpuUnavailable) std::abort();
    if (sceGnmSubmitCommandBuffersForWorkload(1, 2, nullptr, sizes, nullptr, nullptr) != SubmitInvalid) std::abort();
    if (sceGnmSubmitCommandBuffersForWorkload(1, 2, dcb, nullptr, nullptr, nullptr) != SubmitInvalid) std::abort();
    if (sceGnmSubmitCommandBuffersForWorkload(1, 2, dcb, sizes, nullptr, sizes) != SubmitInvalid) std::abort();
    if (sceGnmSubmitCommandBuffersForWorkload(1, 2, dcb, bad, nullptr, nullptr) != SubmitInvalid) std::abort();
    if (sceGnmSubmitCommandBuffersForWorkload(1, 2, dcb, big, nullptr, nullptr) != SubmitInvalid) std::abort();
    if (sceGnmSubmitCommandBuffersForWorkload(1, 2, dcb, sizes, dcb, big) != SubmitInvalid) std::abort();
    if (sceGnmSubmitAndFlipCommandBuffersForWorkload(1, 2, dcb, sizes, dcb, sizes, 1, 0, 1, 0) != GpuUnavailable) std::abort();
    if (sceGnmSubmitAndFlipCommandBuffersForWorkload(1, 2, dcb, bad, nullptr, nullptr, 1, 0, 1, 0) != SubmitInvalid) std::abort();
    static std::uint32_t flip[0x100] = {};
    if (sceGnmRequestFlipAndSubmitDoneForWorkload(1, flip, 0x100, 1, 0, 1, 0) != GpuUnavailable || flip[0] != 0) std::abort();
    if (sceGnmRequestFlipAndSubmitDoneForWorkload(1, flip, 0xff, 1, 0, 1, 0) != static_cast<int>(0x80D11001u)) std::abort();
    if (sceGnmMapComputeQueueWithPriority(0, 0, 0x1000, 0x400, 0x2000, 1) != static_cast<int>(0x80D170FFu)) std::abort();
    if (sceGnmMapComputeQueueWithPriority(7, 0, 0x1000, 0x400, 0x2000, 0) != static_cast<int>(0x80D17000u)) std::abort();
    if (sceGnmMapComputeQueueWithPriority(0, 8, 0x1000, 0x400, 0x2000, 0) != static_cast<int>(0x80D17001u)) std::abort();
    if (sceGnmMapComputeQueueWithPriority(0, 0, 0x1000, 0x300, 0x2000, 0) != static_cast<int>(0x80D17002u)) std::abort();
    if (sceGnmMapComputeQueueWithPriority(0, 0, 0x1010, 0x400, 0x2000, 0) != static_cast<int>(0x80D17003u)) std::abort();
    if (sceGnmMapComputeQueueWithPriority(0, 0, 0x1000, 0x400, 0x2002, 0) != static_cast<int>(0x80D17004u)) std::abort();
    if (sceGnmMapComputeQueueWithPriority(0, 0, 0x1000, 0x400, 0x2000, 2) != static_cast<int>(0x80D17004u)) std::abort();
    if (sceGnmDingDongForWorkload(1, 0x40, 1) != 0) std::abort();
}

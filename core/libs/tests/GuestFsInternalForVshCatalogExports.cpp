#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "prx/libSceFsInternalForVsh/include/Api.hpp"

namespace {

using ThrowingStub = int (APS5_VABI *)(void);

const ThrowingStub kStubs[] = {
    sceFsBfsCopyFile_nid_no_patch,
    sceFsBfsCreateFakeSpace_nid_no_patch,
    sceFsBfsDeleteFakeSpace_nid_no_patch,
    sceFsBfsGetBlockStatus_nid_no_patch,
    sceFsBfsGetCapacityInfoForTelemetry_nid_no_patch,
    sceFsBfsGetRwDeactivateStatus_nid_no_patch,
    sceFsBfsGetRwDeactivateStatusForShutdown_nid_no_patch,
    sceFsBfsUnmount_nid_no_patch,
    sceFsDevpfsAllocateFile2_nid_no_patch,
    sceFsExfatfsMountForCronos_nid_no_patch,
    sceFsExfatfsUnmount_nid_no_patch,
    sceFsFgcEmuDisable_nid_no_patch,
    sceFsFgcEmuEnable_nid_no_patch,
    sceFsFgcEmuInitializeParams_nid_no_patch,
    sceFsFgcEmuSetParams_nid_no_patch,
    sceFsM2StorageGetDebugInfo_nid_no_patch,
    sceFsM2StorageGetDeviceInfo_nid_no_patch,
    sceFsM2StorageLocalSandboxMount_nid_no_patch,
    sceFsM2StorageLocalSandboxUnmount_nid_no_patch,
    sceFsM2StorageQAC_nid_no_patch,
    sceFsM2StorageThermalShutdownTest_nid_no_patch,
    sceFsNTBAddDevice_nid_no_patch,
    sceFsNTBCreatePartitionTable_nid_no_patch,
    sceFsNTBCreateSavedataSnapshot_nid_no_patch,
    sceFsNTBCronosUserPersonalizedKey_nid_no_patch,
    sceFsNTBGetAttachedDeviceType_nid_no_patch,
    sceFsNTBGetDeviceInfo_nid_no_patch,
    sceFsNTBInvalidateBlockRange_nid_no_patch,
    sceFsNTBRemoveDevice_nid_no_patch,
    sceFsNTBVerifyPartitionTable_nid_no_patch,
    sceFsPprSaveDataConvertSize_nid_no_patch,
    sceFsPprSaveDataConvertToCoW_nid_no_patch,
    sceFsPprSaveDataGetBlkBitmap_nid_no_patch,
    sceFsPs4SaveDataGetBlkBitmap_nid_no_patch,
    sceFsSaveDataDisableWriteThrottling_nid_no_patch,
    sceFsSaveDataGetBlockNumber_nid_no_patch,
    sceFsSaveDataGetImageType_nid_no_patch,
    sceFsSaveDataTrimDeviceFile_nid_no_patch,
    sceFsTransactionPprNoCowSaveDataMountEx_nid_no_patch,
    sceFsTransactionPprSaveDataMountEx_nid_no_patch,
    sceFsTransactionSaveDataReadParamEx_nid_no_patch,
    sceFsTransactionSaveDataReadParamEx2_nid_no_patch,
    sceFsTransactionSaveDataWriteParamEx_nid_no_patch,
    sceFsTransactionSaveDataWriteParamEx2_nid_no_patch,
    sceFsUfsMkfsForCronos_nid_no_patch,
    sceFsUfsMountForCronos_nid_no_patch,
    sceFsUfsUnmountForCronos_nid_no_patch,
    sceFsUpdatePackageMetadataFlexcont_nid_no_patch,
};

const char* kStubNames[] = {
    "sceFsBfsCopyFile",
    "sceFsBfsCreateFakeSpace",
    "sceFsBfsDeleteFakeSpace",
    "sceFsBfsGetBlockStatus",
    "sceFsBfsGetCapacityInfoForTelemetry",
    "sceFsBfsGetRwDeactivateStatus",
    "sceFsBfsGetRwDeactivateStatusForShutdown",
    "sceFsBfsUnmount",
    "sceFsDevpfsAllocateFile2",
    "sceFsExfatfsMountForCronos",
    "sceFsExfatfsUnmount",
    "sceFsFgcEmuDisable",
    "sceFsFgcEmuEnable",
    "sceFsFgcEmuInitializeParams",
    "sceFsFgcEmuSetParams",
    "sceFsM2StorageGetDebugInfo",
    "sceFsM2StorageGetDeviceInfo",
    "sceFsM2StorageLocalSandboxMount",
    "sceFsM2StorageLocalSandboxUnmount",
    "sceFsM2StorageQAC",
    "sceFsM2StorageThermalShutdownTest",
    "sceFsNTBAddDevice",
    "sceFsNTBCreatePartitionTable",
    "sceFsNTBCreateSavedataSnapshot",
    "sceFsNTBCronosUserPersonalizedKey",
    "sceFsNTBGetAttachedDeviceType",
    "sceFsNTBGetDeviceInfo",
    "sceFsNTBInvalidateBlockRange",
    "sceFsNTBRemoveDevice",
    "sceFsNTBVerifyPartitionTable",
    "sceFsPprSaveDataConvertSize",
    "sceFsPprSaveDataConvertToCoW",
    "sceFsPprSaveDataGetBlkBitmap",
    "sceFsPs4SaveDataGetBlkBitmap",
    "sceFsSaveDataDisableWriteThrottling",
    "sceFsSaveDataGetBlockNumber",
    "sceFsSaveDataGetImageType",
    "sceFsSaveDataTrimDeviceFile",
    "sceFsTransactionPprNoCowSaveDataMountEx",
    "sceFsTransactionPprSaveDataMountEx",
    "sceFsTransactionSaveDataReadParamEx",
    "sceFsTransactionSaveDataReadParamEx2",
    "sceFsTransactionSaveDataWriteParamEx",
    "sceFsTransactionSaveDataWriteParamEx2",
    "sceFsUfsMkfsForCronos",
    "sceFsUfsMountForCronos",
    "sceFsUfsUnmountForCronos",
    "sceFsUpdatePackageMetadataFlexcont"
};

void RequireThrowsStub(ThrowingStub stub, const char* name, unsigned line) {
    try {
        stub();
    } catch (const std::runtime_error&) {
        return;
    }
    std::fprintf(stderr, "libSceFsInternalForVsh stub did not throw: %s (line %u)\n", name, line);
    std::abort();
}

}

int main() {
    static_assert(sizeof(kStubs) / sizeof(kStubs[0]) == 48, "catalog declaration count changed");
    for (unsigned i = 0; i < sizeof(kStubs) / sizeof(kStubs[0]); ++i) {
        RequireThrowsStub(kStubs[i], kStubNames[i], __LINE__);
    }
    return 0;
}

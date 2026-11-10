#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

#include "prx/libSceFsInternalForVsh/include/Api.hpp"

namespace {

using ThrowingStub = int (APS5_VABI *)(void);

const ThrowingStub kStubs[] = {
    sceFsBfsCopyFile,
    sceFsBfsCreateFakeSpace,
    sceFsBfsDeleteFakeSpace,
    sceFsBfsGetBlockStatus,
    sceFsBfsGetCapacityInfoForTelemetry,
    sceFsBfsGetRwDeactivateStatus,
    sceFsBfsGetRwDeactivateStatusForShutdown,
    sceFsBfsUnmount,
    sceFsDevpfsAllocateFile2,
    sceFsExfatfsMountForCronos,
    sceFsExfatfsUnmount,
    sceFsFgcEmuDisable,
    sceFsFgcEmuEnable,
    sceFsFgcEmuInitializeParams,
    sceFsFgcEmuSetParams,
    sceFsM2StorageGetDebugInfo,
    sceFsM2StorageGetDeviceInfo,
    sceFsM2StorageLocalSandboxMount,
    sceFsM2StorageLocalSandboxUnmount,
    sceFsM2StorageQAC,
    sceFsM2StorageThermalShutdownTest,
    sceFsNTBAddDevice,
    sceFsNTBCreatePartitionTable,
    sceFsNTBCreateSavedataSnapshot,
    sceFsNTBCronosUserPersonalizedKey,
    sceFsNTBGetAttachedDeviceType,
    sceFsNTBGetDeviceInfo,
    sceFsNTBInvalidateBlockRange,
    sceFsNTBRemoveDevice,
    sceFsNTBVerifyPartitionTable,
    sceFsPprSaveDataConvertSize,
    sceFsPprSaveDataConvertToCoW,
    sceFsPprSaveDataGetBlkBitmap,
    sceFsPs4SaveDataGetBlkBitmap,
    sceFsSaveDataDisableWriteThrottling,
    sceFsSaveDataGetBlockNumber,
    sceFsSaveDataGetImageType,
    sceFsSaveDataTrimDeviceFile,
    sceFsTransactionPprNoCowSaveDataMountEx,
    sceFsTransactionPprSaveDataMountEx,
    sceFsTransactionSaveDataReadParamEx,
    sceFsTransactionSaveDataReadParamEx2,
    sceFsTransactionSaveDataWriteParamEx,
    sceFsTransactionSaveDataWriteParamEx2,
    sceFsUfsMkfsForCronos,
    sceFsUfsMountForCronos,
    sceFsUfsUnmountForCronos,
    sceFsUpdatePackageMetadataFlexcont,
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

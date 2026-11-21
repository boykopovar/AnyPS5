#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceUpdateService/include/InventoryExports.hpp"
#include "prx/libSceUpdateService/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int UpdateServiceNotInitialized = static_cast<int>(0x80182402);

} // namespace

extern "C" {

int APS5_VABI sceUpsrvFlushHttpCookie() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvGetCompletedTaskId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUpsrvGetRebootFlag() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvGetSystemExUpdateTaskState() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUpsrvPostErrorLog() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvResumePausedTask() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvSetFakeVersion() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvStartUpdateSession() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvSwitchSystemExBank() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvSwitchSystemExBankWithoutVerify() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvTerminate() {
    return 0;
}

int APS5_VABI sceUpsrvUpdateCancelBDUpdate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUpsrvUpdateCheckCancel() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCheckCreateHandler() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCheckDestroyHandler() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCheckDoCheckSystem() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemForDevKit() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemForSettings() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemWithPupInfo() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCreateUpdateTask() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateDestroyUpdateTask() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateDoCheckBootUpdate() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateDoUpdateSystemEx() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateDoUpdateWithEntitlement() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateDoUpdateWithPupInfo() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateDoUpdateWithPupPath2() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetBetaAgreementUrl() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetEulaDisplay() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetEulaStr() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetEulaStrLen() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetFeatureListUrl() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetFeatureListUrlForSystemExUpdate() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetFeatureListUrlForUpdate() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetImageWritingProgress() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUpsrvUpdateGetProgress() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetUpdateTaskInfo() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateGetUpdateTaskMgrInfo() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateSetEulaAccept() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateStartUpdateTask() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvVerifySystemExBank() {
    return UpdateServiceNotInitialized;
}

}

extern "C" {

int APS5_VABI sceUpsrvDisableAutoInstall() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvEndUpdateSession() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvNotifyUpdatedEvent() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemWithDownloadTypeForSettings() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCheckDoCheckSystemWithReleaseType() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateCheckIsTestPeriod() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceUpsrvUpdateConfirmUpdate() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateStartUpdateModeWithParam() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI sceUpsrvUpdateStartUpdateMode() {
    return UpdateServiceNotInitialized;
}

int APS5_VABI libSceUpdateServiceUnknown_33Z2z4kXKq0_nid_no_patch() {
    return UpdateServiceNotInitialized;
}
APS5_EXPORT("33Z2z4kXKq0", libSceUpdateServiceUnknown_33Z2z4kXKq0_nid_no_patch);

int APS5_VABI libSceUpdateServiceUnknown_ItlDmxWppec_nid_no_patch() {
    return UpdateServiceNotInitialized;
}
APS5_EXPORT("ItlDmxWppec", libSceUpdateServiceUnknown_ItlDmxWppec_nid_no_patch);

int APS5_VABI libSceUpdateServiceUnknown_SpnzKCdKA_plus_4_nid_no_patch() {
    return 0;
}
APS5_EXPORT("SpnzKCdKA+4", libSceUpdateServiceUnknown_SpnzKCdKA_plus_4_nid_no_patch);

int APS5_VABI libSceUpdateServiceUnknown_sEcvo34_plus_ihg_nid_no_patch() {
    return UpdateServiceNotInitialized;
}
APS5_EXPORT("sEcvo34+ihg", libSceUpdateServiceUnknown_sEcvo34_plus_ihg_nid_no_patch);

int APS5_VABI libSceUpdateServiceUnknown_vrw9HD_minus_1Gs0_nid_no_patch() {
    return UpdateServiceNotInitialized;
}
APS5_EXPORT("vrw9HD-1Gs0", libSceUpdateServiceUnknown_vrw9HD_minus_1Gs0_nid_no_patch);

int APS5_VABI libSceUpdateServiceUnknown_yyy3cEojLOw_nid_no_patch() {
    return UpdateServiceNotInitialized;
}
APS5_EXPORT("yyy3cEojLOw", libSceUpdateServiceUnknown_yyy3cEojLOw_nid_no_patch);

}

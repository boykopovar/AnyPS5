#include "prx/libSceUpdateService/include/Api.hpp"
#include "prx/libSceUpdateService/include/InventoryExports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>

static bool ReportsNotImplemented(const std::string& name, const std::function<int()>& call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        if (error.what() == name + " not implemented") return true;
        std::fprintf(stderr, "%s threw: %s\n", name.c_str(), error.what());
        return false;
    }
    std::fprintf(stderr, "%s returned instead of reporting that it is not implemented\n", name.c_str());
    return false;
}

int main() {
    bool passed = ReportsNotImplemented("sceUpsrvFlushHttpCookie", [] { return sceUpsrvFlushHttpCookie(); });
    passed = ReportsNotImplemented("sceUpsrvGetCompletedTaskId", [] { return sceUpsrvGetCompletedTaskId(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvGetRebootFlag", [] { return sceUpsrvGetRebootFlag(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvGetSystemExUpdateTaskState", [] { return sceUpsrvGetSystemExUpdateTaskState(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvInitialize", [] { return sceUpsrvInitialize(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvPostErrorLog", [] { return sceUpsrvPostErrorLog(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvResumePausedTask", [] { return sceUpsrvResumePausedTask(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvSetFakeVersion", [] { return sceUpsrvSetFakeVersion(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvStartUpdateSession", [] { return sceUpsrvStartUpdateSession(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvSwitchSystemExBank", [] { return sceUpsrvSwitchSystemExBank(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvSwitchSystemExBankWithoutVerify", [] { return sceUpsrvSwitchSystemExBankWithoutVerify(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvTerminate", [] { return sceUpsrvTerminate(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCancelBDUpdate", [] { return sceUpsrvUpdateCancelBDUpdate(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckCancel", [] { return sceUpsrvUpdateCheckCancel(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckCreateHandler", [] { return sceUpsrvUpdateCheckCreateHandler(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckDestroyHandler", [] { return sceUpsrvUpdateCheckDestroyHandler(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckDoCheckSystem", [] { return sceUpsrvUpdateCheckDoCheckSystem(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckDoCheckSystemForDevKit", [] { return sceUpsrvUpdateCheckDoCheckSystemForDevKit(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckDoCheckSystemForSettings", [] { return sceUpsrvUpdateCheckDoCheckSystemForSettings(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckDoCheckSystemWithPupInfo", [] { return sceUpsrvUpdateCheckDoCheckSystemWithPupInfo(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCreateUpdateTask", [] { return sceUpsrvUpdateCreateUpdateTask(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateDestroyUpdateTask", [] { return sceUpsrvUpdateDestroyUpdateTask(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateDoCheckBootUpdate", [] { return sceUpsrvUpdateDoCheckBootUpdate(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateDoUpdateSystemEx", [] { return sceUpsrvUpdateDoUpdateSystemEx(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateDoUpdateWithEntitlement", [] { return sceUpsrvUpdateDoUpdateWithEntitlement(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateDoUpdateWithPupInfo", [] { return sceUpsrvUpdateDoUpdateWithPupInfo(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateDoUpdateWithPupPath2", [] { return sceUpsrvUpdateDoUpdateWithPupPath2(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetBetaAgreementUrl", [] { return sceUpsrvUpdateGetBetaAgreementUrl(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetEulaDisplay", [] { return sceUpsrvUpdateGetEulaDisplay(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetEulaStr", [] { return sceUpsrvUpdateGetEulaStr(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetEulaStrLen", [] { return sceUpsrvUpdateGetEulaStrLen(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetFeatureListUrl", [] { return sceUpsrvUpdateGetFeatureListUrl(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetFeatureListUrlForSystemExUpdate", [] { return sceUpsrvUpdateGetFeatureListUrlForSystemExUpdate(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetFeatureListUrlForUpdate", [] { return sceUpsrvUpdateGetFeatureListUrlForUpdate(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetImageWritingProgress", [] { return sceUpsrvUpdateGetImageWritingProgress(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetProgress", [] { return sceUpsrvUpdateGetProgress(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetUpdateTaskInfo", [] { return sceUpsrvUpdateGetUpdateTaskInfo(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateGetUpdateTaskMgrInfo", [] { return sceUpsrvUpdateGetUpdateTaskMgrInfo(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateSetEulaAccept", [] { return sceUpsrvUpdateSetEulaAccept(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateStartUpdateTask", [] { return sceUpsrvUpdateStartUpdateTask(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvVerifySystemExBank", [] { return sceUpsrvVerifySystemExBank(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvDisableAutoInstall", [] { return sceUpsrvDisableAutoInstall(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvEndUpdateSession", [] { return sceUpsrvEndUpdateSession(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvNotifyUpdatedEvent", [] { return sceUpsrvNotifyUpdatedEvent(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckDoCheckSystemWithDownloadTypeForSettings", [] { return sceUpsrvUpdateCheckDoCheckSystemWithDownloadTypeForSettings(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckDoCheckSystemWithReleaseType", [] { return sceUpsrvUpdateCheckDoCheckSystemWithReleaseType(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateCheckIsTestPeriod", [] { return sceUpsrvUpdateCheckIsTestPeriod(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateConfirmUpdate", [] { return sceUpsrvUpdateConfirmUpdate(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateStartUpdateModeWithParam", [] { return sceUpsrvUpdateStartUpdateModeWithParam(); }) && passed;
    passed = ReportsNotImplemented("sceUpsrvUpdateStartUpdateMode", [] { return sceUpsrvUpdateStartUpdateMode(); }) && passed;
    passed = ReportsNotImplemented("libSceUpdateServiceUnknown_33Z2z4kXKq0_nid_no_patch", [] { return libSceUpdateServiceUnknown_33Z2z4kXKq0_nid_no_patch(); }) && passed;
    passed = ReportsNotImplemented("libSceUpdateServiceUnknown_ItlDmxWppec_nid_no_patch", [] { return libSceUpdateServiceUnknown_ItlDmxWppec_nid_no_patch(); }) && passed;
    passed = ReportsNotImplemented("libSceUpdateServiceUnknown_SpnzKCdKA_plus_4_nid_no_patch", [] { return libSceUpdateServiceUnknown_SpnzKCdKA_plus_4_nid_no_patch(); }) && passed;
    passed = ReportsNotImplemented("libSceUpdateServiceUnknown_sEcvo34_plus_ihg_nid_no_patch", [] { return libSceUpdateServiceUnknown_sEcvo34_plus_ihg_nid_no_patch(); }) && passed;
    passed = ReportsNotImplemented("libSceUpdateServiceUnknown_vrw9HD_minus_1Gs0_nid_no_patch", [] { return libSceUpdateServiceUnknown_vrw9HD_minus_1Gs0_nid_no_patch(); }) && passed;
    passed = ReportsNotImplemented("libSceUpdateServiceUnknown_yyy3cEojLOw_nid_no_patch", [] { return libSceUpdateServiceUnknown_yyy3cEojLOw_nid_no_patch(); }) && passed;
    return passed ? 0 : 1;
}

#include "prx/libSceUpdateService/libSceUpdateService.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {

constexpr int NotInitialized = static_cast<int>(0x80182402);

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

using Export = decltype(&sceUpsrvGetRebootFlag);

const std::array<Export, 49> GatedExports = {
    sceUpsrvFlushHttpCookie,
    sceUpsrvGetRebootFlag,
    sceUpsrvGetSystemExUpdateTaskState,
    sceUpsrvPostErrorLog,
    sceUpsrvResumePausedTask,
    sceUpsrvSetFakeVersion,
    sceUpsrvStartUpdateSession,
    sceUpsrvSwitchSystemExBank,
    sceUpsrvSwitchSystemExBankWithoutVerify,
    sceUpsrvUpdateCheckCancel,
    sceUpsrvUpdateCheckCreateHandler,
    sceUpsrvUpdateCheckDestroyHandler,
    sceUpsrvUpdateCheckDoCheckSystem,
    sceUpsrvUpdateCheckDoCheckSystemForDevKit,
    sceUpsrvUpdateCheckDoCheckSystemForSettings,
    sceUpsrvUpdateCheckDoCheckSystemWithPupInfo,
    sceUpsrvUpdateCreateUpdateTask,
    sceUpsrvUpdateDestroyUpdateTask,
    sceUpsrvUpdateDoCheckBootUpdate,
    sceUpsrvUpdateDoUpdateSystemEx,
    sceUpsrvUpdateDoUpdateWithEntitlement,
    sceUpsrvUpdateDoUpdateWithPupInfo,
    sceUpsrvUpdateDoUpdateWithPupPath2,
    sceUpsrvUpdateGetBetaAgreementUrl,
    sceUpsrvUpdateGetEulaDisplay,
    sceUpsrvUpdateGetEulaStr,
    sceUpsrvUpdateGetEulaStrLen,
    sceUpsrvUpdateGetFeatureListUrl,
    sceUpsrvUpdateGetFeatureListUrlForSystemExUpdate,
    sceUpsrvUpdateGetFeatureListUrlForUpdate,
    sceUpsrvUpdateGetProgress,
    sceUpsrvUpdateGetUpdateTaskInfo,
    sceUpsrvUpdateGetUpdateTaskMgrInfo,
    sceUpsrvUpdateSetEulaAccept,
    sceUpsrvUpdateStartUpdateTask,
    sceUpsrvVerifySystemExBank,
    sceUpsrvDisableAutoInstall,
    sceUpsrvEndUpdateSession,
    sceUpsrvNotifyUpdatedEvent,
    sceUpsrvUpdateCheckDoCheckSystemWithDownloadTypeForSettings,
    sceUpsrvUpdateCheckDoCheckSystemWithReleaseType,
    sceUpsrvUpdateConfirmUpdate,
    sceUpsrvUpdateStartUpdateModeWithParam,
    sceUpsrvUpdateStartUpdateMode,
    libSceUpdateServiceUnknown_33Z2z4kXKq0_nid_no_patch,
    libSceUpdateServiceUnknown_ItlDmxWppec_nid_no_patch,
    libSceUpdateServiceUnknown_sEcvo34_plus_ihg_nid_no_patch,
    libSceUpdateServiceUnknown_vrw9HD_minus_1Gs0_nid_no_patch,
    libSceUpdateServiceUnknown_yyy3cEojLOw_nid_no_patch
};

void CheckBeforeInitialize() {
    for (std::size_t i = 0; i < GatedExports.size(); ++i) {
        if (GatedExports[i]() == NotInitialized) continue;
        std::fprintf(stderr, "export %zu\n", i);
        Require(false, "an export answers 0x80182402 before initialization");
    }
}

void CheckTerminate() {
    Require(sceUpsrvTerminate() == 0, "sceUpsrvTerminate returns 0 with nothing initialized");
    Require(sceUpsrvTerminate() == 0, "a second sceUpsrvTerminate returns 0");
    Require(libSceUpdateServiceUnknown_SpnzKCdKA_plus_4_nid_no_patch() == 0, "SpnzKCdKA+4 returns 0 before initialization");
    Require(sceUpsrvGetRebootFlag() == NotInitialized, "sceUpsrvTerminate leaves the library uninitialized");
}

} // namespace

int main() {
    CheckBeforeInitialize();
    CheckTerminate();
    CheckBeforeInitialize();
    return 0;
}

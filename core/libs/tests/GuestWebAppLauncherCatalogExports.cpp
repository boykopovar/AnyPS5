#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI AdIdDialogOpen_nid_no_patch(void);
int APS5_VABI AuthorizedAppDialogOpen_nid_no_patch(void);
int APS5_VABI ConnectAccountDialogInit_nid_no_patch(void);
int APS5_VABI ConnectAccountDialogOpen_nid_no_patch(void);
int APS5_VABI GetAccessibilitySettings_nid_no_patch(void);
int APS5_VABI GetAppSessionId_nid_no_patch(void);
int APS5_VABI GetAuthorizedAppCode_nid_no_patch(void);
int APS5_VABI GetClosedCaptionAllSettings_nid_no_patch(void);
int APS5_VABI GetFileSize_nid_no_patch(void);
int APS5_VABI GetMessage_nid_no_patch(void);
int APS5_VABI GetNpEnvironment_nid_no_patch(void);
int APS5_VABI GetOpenPsIdForSystem_nid_no_patch(void);
int APS5_VABI GetUniversalEsvmDebugSetting_nid_no_patch(void);
int APS5_VABI IsSupportedAudioOutputMode_nid_no_patch(void);
int APS5_VABI IsSupportedVideoOutputMode_nid_no_patch(void);
int APS5_VABI MusicCoreAbortMessage_nid_no_patch(void);
int APS5_VABI MusicCoreAddSrc_nid_no_patch(void);
int APS5_VABI MusicCoreGetCapability_nid_no_patch(void);
int APS5_VABI MusicCoreGetCurrentTime_nid_no_patch(void);
int APS5_VABI MusicCoreGetDuration_nid_no_patch(void);
int APS5_VABI MusicCoreGetLoop_nid_no_patch(void);
int APS5_VABI MusicCoreGetMusicInfo_nid_no_patch(void);
int APS5_VABI MusicCoreGetMuted_nid_no_patch(void);
int APS5_VABI MusicCoreGetNetworkState_nid_no_patch(void);
int APS5_VABI MusicCoreGetShuffle_nid_no_patch(void);
int APS5_VABI MusicCoreGetVolume_nid_no_patch(void);
int APS5_VABI MusicCoreInitialize_nid_no_patch(void);
int APS5_VABI MusicCoreIsEnded_nid_no_patch(void);
int APS5_VABI MusicCoreIsPaused_nid_no_patch(void);
int APS5_VABI MusicCoreIsPlaying_nid_no_patch(void);
int APS5_VABI MusicCoreIsSeeking_nid_no_patch(void);
int APS5_VABI MusicCoreIsStopped_nid_no_patch(void);
int APS5_VABI MusicCoreLoad_nid_no_patch(void);
int APS5_VABI MusicCorePause_nid_no_patch(void);
int APS5_VABI MusicCorePlay_nid_no_patch(void);
int APS5_VABI MusicCoreRegisterCallback_nid_no_patch(void);
int APS5_VABI MusicCoreRegisterNotify_nid_no_patch(void);
int APS5_VABI MusicCoreRemoveSrc_nid_no_patch(void);
int APS5_VABI MusicCoreSendMessageAsync_nid_no_patch(void);
int APS5_VABI MusicCoreSendMessageSync_nid_no_patch(void);
int APS5_VABI MusicCoreSetCurrentTime_nid_no_patch(void);
int APS5_VABI MusicCoreSetLoop_nid_no_patch(void);
int APS5_VABI MusicCoreSetMuted_nid_no_patch(void);
int APS5_VABI MusicCoreSetShuffle_nid_no_patch(void);
int APS5_VABI MusicCoreSetVolume_nid_no_patch(void);
int APS5_VABI MusicCoreSkipNext_nid_no_patch(void);
int APS5_VABI MusicCoreSkipPrevious_nid_no_patch(void);
int APS5_VABI MusicCoreStop_nid_no_patch(void);
int APS5_VABI MusicCoreTerminate_nid_no_patch(void);
int APS5_VABI MusicCoreUnregisterCallback_nid_no_patch(void);
int APS5_VABI MusicCoreUnregisterNotify_nid_no_patch(void);
int APS5_VABI NpWebGetPushContextId_nid_no_patch(void);
int APS5_VABI NpWebSetAdditionalScope_nid_no_patch(void);
int APS5_VABI OpenApp_nid_no_patch(void);
int APS5_VABI OpenVRApp_nid_no_patch(void);
int APS5_VABI PartnerModuleInitialize_nid_no_patch(void);
int APS5_VABI PartnerModuleTerminate_nid_no_patch(void);
int APS5_VABI PostMessage_nid_no_patch(void);
int APS5_VABI ReadFile_nid_no_patch(void);
int APS5_VABI SaveDataCommit_nid_no_patch(void);
int APS5_VABI SaveDataDelete_nid_no_patch(void);
int APS5_VABI SaveDataInitialize_nid_no_patch(void);
int APS5_VABI SaveDataMount_nid_no_patch(void);
int APS5_VABI SaveDataPrepare_nid_no_patch(void);
int APS5_VABI SaveDataReadFile_nid_no_patch(void);
int APS5_VABI SaveDataReadFileSize_nid_no_patch(void);
int APS5_VABI SaveDataTerminate_nid_no_patch(void);
int APS5_VABI SaveDataUnmount_nid_no_patch(void);
int APS5_VABI SaveDataWriteFile_nid_no_patch(void);
int APS5_VABI SendNativeTelemetryCrashEvent_nid_no_patch(void);
int APS5_VABI SetAudioTagFeature_nid_no_patch(void);
int APS5_VABI SetUniversalEsvm_nid_no_patch(void);
int APS5_VABI SetVideoOutputMode_nid_no_patch(void);
int APS5_VABI ShareFeaturePermit_nid_no_patch(void);
int APS5_VABI ShareFeatureProhibit_nid_no_patch(void);
int APS5_VABI ShareGetCurrentStatus_nid_no_patch(void);
int APS5_VABI ShareInitialize_nid_no_patch(void);
int APS5_VABI ShareTerminate_nid_no_patch(void);
}

namespace {

using ThrowingStub = int (APS5_VABI *)(void);

const ThrowingStub kStubs[] = {
    AdIdDialogOpen_nid_no_patch,
    AuthorizedAppDialogOpen_nid_no_patch,
    ConnectAccountDialogInit_nid_no_patch,
    ConnectAccountDialogOpen_nid_no_patch,
    GetAccessibilitySettings_nid_no_patch,
    GetAppSessionId_nid_no_patch,
    GetAuthorizedAppCode_nid_no_patch,
    GetClosedCaptionAllSettings_nid_no_patch,
    GetFileSize_nid_no_patch,
    GetMessage_nid_no_patch,
    GetNpEnvironment_nid_no_patch,
    GetOpenPsIdForSystem_nid_no_patch,
    GetUniversalEsvmDebugSetting_nid_no_patch,
    IsSupportedAudioOutputMode_nid_no_patch,
    IsSupportedVideoOutputMode_nid_no_patch,
    MusicCoreAbortMessage_nid_no_patch,
    MusicCoreAddSrc_nid_no_patch,
    MusicCoreGetCapability_nid_no_patch,
    MusicCoreGetCurrentTime_nid_no_patch,
    MusicCoreGetDuration_nid_no_patch,
    MusicCoreGetLoop_nid_no_patch,
    MusicCoreGetMusicInfo_nid_no_patch,
    MusicCoreGetMuted_nid_no_patch,
    MusicCoreGetNetworkState_nid_no_patch,
    MusicCoreGetShuffle_nid_no_patch,
    MusicCoreGetVolume_nid_no_patch,
    MusicCoreInitialize_nid_no_patch,
    MusicCoreIsEnded_nid_no_patch,
    MusicCoreIsPaused_nid_no_patch,
    MusicCoreIsPlaying_nid_no_patch,
    MusicCoreIsSeeking_nid_no_patch,
    MusicCoreIsStopped_nid_no_patch,
    MusicCoreLoad_nid_no_patch,
    MusicCorePause_nid_no_patch,
    MusicCorePlay_nid_no_patch,
    MusicCoreRegisterCallback_nid_no_patch,
    MusicCoreRegisterNotify_nid_no_patch,
    MusicCoreRemoveSrc_nid_no_patch,
    MusicCoreSendMessageAsync_nid_no_patch,
    MusicCoreSendMessageSync_nid_no_patch,
    MusicCoreSetCurrentTime_nid_no_patch,
    MusicCoreSetLoop_nid_no_patch,
    MusicCoreSetMuted_nid_no_patch,
    MusicCoreSetShuffle_nid_no_patch,
    MusicCoreSetVolume_nid_no_patch,
    MusicCoreSkipNext_nid_no_patch,
    MusicCoreSkipPrevious_nid_no_patch,
    MusicCoreStop_nid_no_patch,
    MusicCoreTerminate_nid_no_patch,
    MusicCoreUnregisterCallback_nid_no_patch,
    MusicCoreUnregisterNotify_nid_no_patch,
    NpWebGetPushContextId_nid_no_patch,
    NpWebSetAdditionalScope_nid_no_patch,
    OpenApp_nid_no_patch,
    OpenVRApp_nid_no_patch,
    PartnerModuleInitialize_nid_no_patch,
    PartnerModuleTerminate_nid_no_patch,
    PostMessage_nid_no_patch,
    ReadFile_nid_no_patch,
    SaveDataCommit_nid_no_patch,
    SaveDataDelete_nid_no_patch,
    SaveDataInitialize_nid_no_patch,
    SaveDataMount_nid_no_patch,
    SaveDataPrepare_nid_no_patch,
    SaveDataReadFile_nid_no_patch,
    SaveDataReadFileSize_nid_no_patch,
    SaveDataTerminate_nid_no_patch,
    SaveDataUnmount_nid_no_patch,
    SaveDataWriteFile_nid_no_patch,
    SendNativeTelemetryCrashEvent_nid_no_patch,
    SetAudioTagFeature_nid_no_patch,
    SetUniversalEsvm_nid_no_patch,
    SetVideoOutputMode_nid_no_patch,
    ShareFeaturePermit_nid_no_patch,
    ShareFeatureProhibit_nid_no_patch,
    ShareGetCurrentStatus_nid_no_patch,
    ShareInitialize_nid_no_patch,
    ShareTerminate_nid_no_patch,
};

const char* kStubNames[] = {
    "AdIdDialogOpen",
    "AuthorizedAppDialogOpen",
    "ConnectAccountDialogInit",
    "ConnectAccountDialogOpen",
    "GetAccessibilitySettings",
    "GetAppSessionId",
    "GetAuthorizedAppCode",
    "GetClosedCaptionAllSettings",
    "GetFileSize",
    "GetMessage",
    "GetNpEnvironment",
    "GetOpenPsIdForSystem",
    "GetUniversalEsvmDebugSetting",
    "IsSupportedAudioOutputMode",
    "IsSupportedVideoOutputMode",
    "MusicCoreAbortMessage",
    "MusicCoreAddSrc",
    "MusicCoreGetCapability",
    "MusicCoreGetCurrentTime",
    "MusicCoreGetDuration",
    "MusicCoreGetLoop",
    "MusicCoreGetMusicInfo",
    "MusicCoreGetMuted",
    "MusicCoreGetNetworkState",
    "MusicCoreGetShuffle",
    "MusicCoreGetVolume",
    "MusicCoreInitialize",
    "MusicCoreIsEnded",
    "MusicCoreIsPaused",
    "MusicCoreIsPlaying",
    "MusicCoreIsSeeking",
    "MusicCoreIsStopped",
    "MusicCoreLoad",
    "MusicCorePause",
    "MusicCorePlay",
    "MusicCoreRegisterCallback",
    "MusicCoreRegisterNotify",
    "MusicCoreRemoveSrc",
    "MusicCoreSendMessageAsync",
    "MusicCoreSendMessageSync",
    "MusicCoreSetCurrentTime",
    "MusicCoreSetLoop",
    "MusicCoreSetMuted",
    "MusicCoreSetShuffle",
    "MusicCoreSetVolume",
    "MusicCoreSkipNext",
    "MusicCoreSkipPrevious",
    "MusicCoreStop",
    "MusicCoreTerminate",
    "MusicCoreUnregisterCallback",
    "MusicCoreUnregisterNotify",
    "NpWebGetPushContextId",
    "NpWebSetAdditionalScope",
    "OpenApp",
    "OpenVRApp",
    "PartnerModuleInitialize",
    "PartnerModuleTerminate",
    "PostMessage",
    "ReadFile",
    "SaveDataCommit",
    "SaveDataDelete",
    "SaveDataInitialize",
    "SaveDataMount",
    "SaveDataPrepare",
    "SaveDataReadFile",
    "SaveDataReadFileSize",
    "SaveDataTerminate",
    "SaveDataUnmount",
    "SaveDataWriteFile",
    "SendNativeTelemetryCrashEvent",
    "SetAudioTagFeature",
    "SetUniversalEsvm",
    "SetVideoOutputMode",
    "ShareFeaturePermit",
    "ShareFeatureProhibit",
    "ShareGetCurrentStatus",
    "ShareInitialize",
    "ShareTerminate"
};

void RequireThrowsStub(ThrowingStub stub, const char* name, unsigned line) {
    try {
        stub();
    } catch (const std::runtime_error&) {
        return;
    }
    std::fprintf(stderr, "libReactNative.Modules.Vsh.WebAppLauncher stub did not throw: %s (line %u)\n", name, line);
    std::abort();
}

}

int main() {
    static_assert(sizeof(kStubs) / sizeof(kStubs[0]) == 78, "catalog declaration count changed");
    for (unsigned i = 0; i < sizeof(kStubs) / sizeof(kStubs[0]); ++i) {
        RequireThrowsStub(kStubs[i], kStubNames[i], __LINE__);
    }
    return 0;
}

#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI AdIdDialogOpen(void);
int APS5_VABI AuthorizedAppDialogOpen(void);
int APS5_VABI ConnectAccountDialogInit(void);
int APS5_VABI ConnectAccountDialogOpen(void);
int APS5_VABI GetAccessibilitySettings(void);
int APS5_VABI GetAppSessionId(void);
int APS5_VABI GetAuthorizedAppCode(void);
int APS5_VABI GetClosedCaptionAllSettings(void);
int APS5_VABI GetFileSize(void);
int APS5_VABI GetMessage(void);
int APS5_VABI GetNpEnvironment(void);
int APS5_VABI GetOpenPsIdForSystem(void);
int APS5_VABI GetUniversalEsvmDebugSetting(void);
int APS5_VABI IsSupportedAudioOutputMode(void);
int APS5_VABI IsSupportedVideoOutputMode(void);
int APS5_VABI MusicCoreAbortMessage(void);
int APS5_VABI MusicCoreAddSrc(void);
int APS5_VABI MusicCoreGetCapability(void);
int APS5_VABI MusicCoreGetCurrentTime(void);
int APS5_VABI MusicCoreGetDuration(void);
int APS5_VABI MusicCoreGetLoop(void);
int APS5_VABI MusicCoreGetMusicInfo(void);
int APS5_VABI MusicCoreGetMuted(void);
int APS5_VABI MusicCoreGetNetworkState(void);
int APS5_VABI MusicCoreGetShuffle(void);
int APS5_VABI MusicCoreGetVolume(void);
int APS5_VABI MusicCoreInitialize(void);
int APS5_VABI MusicCoreIsEnded(void);
int APS5_VABI MusicCoreIsPaused(void);
int APS5_VABI MusicCoreIsPlaying(void);
int APS5_VABI MusicCoreIsSeeking(void);
int APS5_VABI MusicCoreIsStopped(void);
int APS5_VABI MusicCoreLoad(void);
int APS5_VABI MusicCorePause(void);
int APS5_VABI MusicCorePlay(void);
int APS5_VABI MusicCoreRegisterCallback(void);
int APS5_VABI MusicCoreRegisterNotify(void);
int APS5_VABI MusicCoreRemoveSrc(void);
int APS5_VABI MusicCoreSendMessageAsync(void);
int APS5_VABI MusicCoreSendMessageSync(void);
int APS5_VABI MusicCoreSetCurrentTime(void);
int APS5_VABI MusicCoreSetLoop(void);
int APS5_VABI MusicCoreSetMuted(void);
int APS5_VABI MusicCoreSetShuffle(void);
int APS5_VABI MusicCoreSetVolume(void);
int APS5_VABI MusicCoreSkipNext(void);
int APS5_VABI MusicCoreSkipPrevious(void);
int APS5_VABI MusicCoreStop(void);
int APS5_VABI MusicCoreTerminate(void);
int APS5_VABI MusicCoreUnregisterCallback(void);
int APS5_VABI MusicCoreUnregisterNotify(void);
int APS5_VABI NpWebGetPushContextId(void);
int APS5_VABI NpWebSetAdditionalScope(void);
int APS5_VABI OpenApp(void);
int APS5_VABI OpenVRApp(void);
int APS5_VABI PartnerModuleInitialize(void);
int APS5_VABI PartnerModuleTerminate(void);
int APS5_VABI PostMessage(void);
int APS5_VABI ReadFile(void);
int APS5_VABI SaveDataCommit(void);
int APS5_VABI SaveDataDelete(void);
int APS5_VABI SaveDataInitialize(void);
int APS5_VABI SaveDataMount(void);
int APS5_VABI SaveDataPrepare(void);
int APS5_VABI SaveDataReadFile(void);
int APS5_VABI SaveDataReadFileSize(void);
int APS5_VABI SaveDataTerminate(void);
int APS5_VABI SaveDataUnmount(void);
int APS5_VABI SaveDataWriteFile(void);
int APS5_VABI SendNativeTelemetryCrashEvent(void);
int APS5_VABI SetAudioTagFeature(void);
int APS5_VABI SetUniversalEsvm(void);
int APS5_VABI SetVideoOutputMode(void);
int APS5_VABI ShareFeaturePermit(void);
int APS5_VABI ShareFeatureProhibit(void);
int APS5_VABI ShareGetCurrentStatus(void);
int APS5_VABI ShareInitialize(void);
int APS5_VABI ShareTerminate(void);
}

namespace {

using ThrowingStub = int (APS5_VABI *)(void);

const ThrowingStub kStubs[] = {
    AdIdDialogOpen,
    AuthorizedAppDialogOpen,
    ConnectAccountDialogInit,
    ConnectAccountDialogOpen,
    GetAccessibilitySettings,
    GetAppSessionId,
    GetAuthorizedAppCode,
    GetClosedCaptionAllSettings,
    GetFileSize,
    GetMessage,
    GetNpEnvironment,
    GetOpenPsIdForSystem,
    GetUniversalEsvmDebugSetting,
    IsSupportedAudioOutputMode,
    IsSupportedVideoOutputMode,
    MusicCoreAbortMessage,
    MusicCoreAddSrc,
    MusicCoreGetCapability,
    MusicCoreGetCurrentTime,
    MusicCoreGetDuration,
    MusicCoreGetLoop,
    MusicCoreGetMusicInfo,
    MusicCoreGetMuted,
    MusicCoreGetNetworkState,
    MusicCoreGetShuffle,
    MusicCoreGetVolume,
    MusicCoreInitialize,
    MusicCoreIsEnded,
    MusicCoreIsPaused,
    MusicCoreIsPlaying,
    MusicCoreIsSeeking,
    MusicCoreIsStopped,
    MusicCoreLoad,
    MusicCorePause,
    MusicCorePlay,
    MusicCoreRegisterCallback,
    MusicCoreRegisterNotify,
    MusicCoreRemoveSrc,
    MusicCoreSendMessageAsync,
    MusicCoreSendMessageSync,
    MusicCoreSetCurrentTime,
    MusicCoreSetLoop,
    MusicCoreSetMuted,
    MusicCoreSetShuffle,
    MusicCoreSetVolume,
    MusicCoreSkipNext,
    MusicCoreSkipPrevious,
    MusicCoreStop,
    MusicCoreTerminate,
    MusicCoreUnregisterCallback,
    MusicCoreUnregisterNotify,
    NpWebGetPushContextId,
    NpWebSetAdditionalScope,
    OpenApp,
    OpenVRApp,
    PartnerModuleInitialize,
    PartnerModuleTerminate,
    PostMessage,
    ReadFile,
    SaveDataCommit,
    SaveDataDelete,
    SaveDataInitialize,
    SaveDataMount,
    SaveDataPrepare,
    SaveDataReadFile,
    SaveDataReadFileSize,
    SaveDataTerminate,
    SaveDataUnmount,
    SaveDataWriteFile,
    SendNativeTelemetryCrashEvent,
    SetAudioTagFeature,
    SetUniversalEsvm,
    SetVideoOutputMode,
    ShareFeaturePermit,
    ShareFeatureProhibit,
    ShareGetCurrentStatus,
    ShareInitialize,
    ShareTerminate,
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

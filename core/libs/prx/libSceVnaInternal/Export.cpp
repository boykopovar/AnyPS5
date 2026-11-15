#include "prx/libSceVnaInternal/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_VNA_ERROR_UNKNOWN_HANDLE = static_cast<int>(0x816F8007);

}

extern "C" {

APS5_DUMMY_FUN

int APS5_VABI sceVnaGetMicStatus() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetEnableWupSettings() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetAgentSettings() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetErrorSeverity() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaSetSessionTimeoutSettings() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaStartAnalyzeInternal() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaRequestTts() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaCancelTts() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetEnableAssistantLegalCheckStatus() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaUnregisterCallback() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetVadConfig() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaUtilGetVoiceInputStatus() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaUtilTerminate() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaSetDucking() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetAgentInfo() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetEventData() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetKeyPhraseInfoCount() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetEnableFeedbackSettings() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetWakeUpPhraseTelemetry() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaPlayTts() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetAgentIntentSessionTelemetry() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetAgentAvailabilityInfo() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaCheckCallback() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetVoiceOskOptInStatus() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetLanguageModelDirectoryPath() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetAsrExclusive() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaNotify() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetVoiceDataCollectionSettingsAsync() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaDisableAgentGui() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaStopAnalyze() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaStartAnalyze() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaNotifyStatus() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetLanguageSettings() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaUtilGetSupportedLanguageList() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaGetSupportedLanguageInfo() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetFeedbackStatus() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetVoiceDataCollectionSettingsAsync() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetVoiceOskOptInStatus() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaUtilInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaUnregisterSubscribeTarget() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetTtsSpeedSettings() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaCancelDownloadTts() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetVoiceDataCollectionSettings() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetKeyPhraseCategoryEnable() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaWaitReady() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaRequestDialogTts() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetKeyPhrase() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetSupportedSpeakerType() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaStartKeyPhraseDetection() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaInitializeForParty() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaGetVadState() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaRequestDownloadTts() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaPlaySound() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetCurrentAudioInTelemetryInfo() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSendAgentEvent() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaRegisterSubscribeTarget() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetLocation() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetSessionInfo() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetTtsVolumeSettings() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetTtsDuckingLevel() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaRegisterCallback() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetEnableAssistantSettings() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaUtilUpdateVoiceInputStatus() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaSetFeedbackTrigger() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaRequestDownloadCahcedTts() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaUtilIsLanguageSupported() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaSetVadVolumeThreshold() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetupKeyPhraseDetection() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetUmmStatus() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaRequestPlayCachedTts() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaDevideAnalyze() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGenerateTelemetrySessionId() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetTtsVolume() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaCloseTtsStream() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaDisableWupRecording() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetTrimEndAudioFrame() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaGetKeyPhraseInfo() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaReadTtsStream() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaTerminate() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaSetClientStatus() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaUtilRemoveVoiceInputStatus() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceVnaStopKeyPhraseDetection() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaOpenTtsStream() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

}

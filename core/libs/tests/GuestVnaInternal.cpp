#include "prx/libSceVnaInternal/libSceVnaInternal.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int unknownHandle = static_cast<int>(0x816F8007u);
    for (int call = 0; call < 2; ++call) {
        Require(sceVnaStartAnalyze() == unknownHandle);
        Require(sceVnaStartAnalyzeInternal() == unknownHandle);
        Require(sceVnaStopAnalyze() == unknownHandle);
        Require(sceVnaDevideAnalyze() == unknownHandle);
        Require(sceVnaRequestDownloadTts() == unknownHandle);
        Require(sceVnaCancelDownloadTts() == unknownHandle);
        Require(sceVnaOpenTtsStream() == unknownHandle);
        Require(sceVnaReadTtsStream() == unknownHandle);
        Require(sceVnaCloseTtsStream() == unknownHandle);
        Require(sceVnaUnregisterSubscribeTarget() == unknownHandle);
        Require(sceVnaGetEventData() == unknownHandle);
        Require(sceVnaGetSupportedLanguageInfo() == unknownHandle);
        Require(sceVnaGetSupportedSpeakerType() == unknownHandle);
        Require(sceVnaSetAgentIntentSessionTelemetry() == unknownHandle);
        Require(sceVnaSetWakeUpPhraseTelemetry() == unknownHandle);
        Require(sceVnaGenerateTelemetrySessionId() == unknownHandle);
        Require(sceVnaPlaySound() == unknownHandle);
        Require(sceVnaSetAsrExclusive() == unknownHandle);
        Require(sceVnaSetSessionInfo() == unknownHandle);
        Require(sceVnaSetupKeyPhraseDetection() == unknownHandle);
        Require(sceVnaStartKeyPhraseDetection() == unknownHandle);
        Require(sceVnaStopKeyPhraseDetection() == unknownHandle);
        Require(sceVnaRequestTts() == unknownHandle);
        Require(sceVnaRequestDialogTts() == unknownHandle);
        Require(sceVnaCancelTts() == unknownHandle);
        Require(sceVnaRequestPlayCachedTts() == unknownHandle);
        Require(sceVnaRequestDownloadCahcedTts() == unknownHandle);
        Require(sceVnaSetDucking() == unknownHandle);
        Require(sceVnaSetTtsDuckingLevel() == unknownHandle);
        Require(sceVnaSetVadVolumeThreshold() == unknownHandle);
        Require(sceVnaRegisterCallback() == unknownHandle);
        Require(sceVnaUnregisterCallback() == unknownHandle);
        Require(sceVnaCheckCallback() == unknownHandle);
        Require(sceVnaGetMicStatus() == unknownHandle);
        Require(sceVnaGetFeedbackStatus() == unknownHandle);
        Require(sceVnaGetVadState() == unknownHandle);
        Require(sceVnaGetTtsVolume() == unknownHandle);
        Require(sceVnaNotifyStatus() == unknownHandle);
        Require(sceVnaNotify() == unknownHandle);
        Require(sceVnaSetClientStatus() == unknownHandle);
        Require(sceVnaGetLanguageModelDirectoryPath() == unknownHandle);
        Require(sceVnaGetKeyPhraseInfoCount() == unknownHandle);
        Require(sceVnaGetKeyPhraseInfo() == unknownHandle);
        Require(sceVnaSetKeyPhrase() == unknownHandle);
        Require(sceVnaSetKeyPhraseCategoryEnable() == unknownHandle);
        Require(sceVnaGetAgentSettings() == unknownHandle);
        Require(sceVnaSetEnableAssistantSettings() == unknownHandle);
        Require(sceVnaSetTtsSpeedSettings() == unknownHandle);
        Require(sceVnaSetTtsVolumeSettings() == unknownHandle);
        Require(sceVnaSetSessionTimeoutSettings() == unknownHandle);
        Require(sceVnaSetLanguageSettings() == unknownHandle);
        Require(sceVnaSetEnableWupSettings() == unknownHandle);
        Require(sceVnaSetEnableAssistantLegalCheckStatus() == unknownHandle);
        Require(sceVnaSetEnableFeedbackSettings() == unknownHandle);
        Require(sceVnaDisableAgentGui() == unknownHandle);
        Require(sceVnaPlayTts() == unknownHandle);
        Require(sceVnaGetVoiceDataCollectionSettings() == unknownHandle);
        Require(sceVnaGetVoiceDataCollectionSettingsAsync() == unknownHandle);
        Require(sceVnaSetVoiceDataCollectionSettingsAsync() == unknownHandle);
        Require(sceVnaGetAgentAvailabilityInfo() == unknownHandle);
        Require(sceVnaSendAgentEvent() == unknownHandle);
        Require(sceVnaGetVoiceOskOptInStatus() == unknownHandle);
        Require(sceVnaSetVoiceOskOptInStatus() == unknownHandle);
        Require(sceVnaSetLocation() == unknownHandle);
        Require(sceVnaGetCurrentAudioInTelemetryInfo() == unknownHandle);
        Require(sceVnaSetUmmStatus() == unknownHandle);
        Require(sceVnaSetVadConfig() == unknownHandle);
        Require(sceVnaSetAgentInfo() == unknownHandle);
        Require(sceVnaDisableWupRecording() == unknownHandle);
        Require(sceVnaSetFeedbackTrigger() == unknownHandle);
        Require(sceVnaSetTrimEndAudioFrame() == unknownHandle);
        Require(sceVnaRegisterSubscribeTarget() == unknownHandle);
        Require(sceVnaTerminate() == unknownHandle);
    }
    return 0;
}

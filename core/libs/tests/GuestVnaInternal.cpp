#include "prx/libSceVnaInternal/libSceVnaInternal.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

void CheckSeverity(std::uint32_t code, std::string_view expected) {
    std::array<unsigned char, 16> out{};
    out.fill(0xA5);
    errno = 71;
    Require(sceVnaGetErrorSeverity(static_cast<std::int32_t>(code), reinterpret_cast<char*>(out.data())) == 0);
    Require(errno == 71);
    Require(std::memcmp(out.data(), expected.data(), expected.size()) == 0);
    Require(out[expected.size()] == 0);
    for (std::size_t i = expected.size() + 1; i < out.size(); ++i) {
        Require(out[i] == 0xA5);
    }
}

} // namespace

int main() {
    constexpr int invalidArg = static_cast<int>(0x816F8004u);
    constexpr int notFound = static_cast<int>(0x816F8005u);
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
        Require(sceVnaUtilTerminate() == unknownHandle);
        Require(sceVnaUtilGetSupportedLanguageList() == unknownHandle);
        Require(sceVnaUtilIsLanguageSupported() == unknownHandle);
        Require(sceVnaUtilGetVoiceInputStatus() == unknownHandle);
    }

    Require(sceVnaGetErrorSeverity(0, nullptr) == invalidArg);
    errno = 71;
    Require(sceVnaGetErrorSeverity(0, reinterpret_cast<char*>(static_cast<std::uintptr_t>(0x10))) == invalidArg);
    Require(errno == 71);

    CheckSeverity(0x00000000u, "");
    CheckSeverity(0x81761E0Au, "critical");
    CheckSeverity(0x81761E0Bu, "critical");
    CheckSeverity(0x816F0000u, "major");
    CheckSeverity(0x816F0004u, "major");
    CheckSeverity(0x816F801Au, "major");
    CheckSeverity(0x816F804Au, "major");
    CheckSeverity(0x816F804Bu, "major");
    CheckSeverity(0x81761E04u, "major");
    CheckSeverity(0x81761E09u, "major");
    CheckSeverity(0x816F6FFEu, "major");
    CheckSeverity(0x816F0010u, "normal");
    CheckSeverity(0x816F002Fu, "normal");
    CheckSeverity(0x816F003Fu, "normal");
    CheckSeverity(0x816F0003u, "info");
    CheckSeverity(0x816F004Du, "info");
    CheckSeverity(0x816F6FFDu, "info");
    CheckSeverity(0x816F801Fu, "info");
    CheckSeverity(0x816F8051u, "info");
    CheckSeverity(0x81761E01u, "info");
    CheckSeverity(0x81761E0Cu, "info");
    CheckSeverity(0x816F0009u, "minor");
    CheckSeverity(0x816F6FFFu, "minor");
    CheckSeverity(0x816F8004u, "minor");
    CheckSeverity(0x816F804Eu, "minor");
    CheckSeverity(0x81761E00u, "minor");
    CheckSeverity(0x81761E0Du, "minor");
    CheckSeverity(0x12345678u, "minor");

    Require(sceVnaUtilUpdateVoiceInputStatus(nullptr) == invalidArg);
    errno = 71;
    Require(sceVnaUtilUpdateVoiceInputStatus(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(0x10))) == invalidArg);
    Require(errno == 71);
    Require(sceVnaUtilRemoveVoiceInputStatus(42) == notFound);

    const std::uint64_t initial = (std::uint64_t{0x00000003u} << 32) | 42ull;
    const std::uint64_t updated = (std::uint64_t{0x00000007u} << 32) | 42ull;
    Require(sceVnaUtilUpdateVoiceInputStatus(&initial) == 0);
    Require(errno == 71);
    Require(sceVnaUtilUpdateVoiceInputStatus(&updated) == 0);
    Require(sceVnaUtilRemoveVoiceInputStatus(42) == 0);
    Require(sceVnaUtilRemoveVoiceInputStatus(42) == notFound);
    return 0;
}

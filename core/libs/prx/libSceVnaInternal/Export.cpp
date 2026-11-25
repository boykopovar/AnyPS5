#include "prx/libSceVnaInternal/include/InventoryExports.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_VNA_ERROR_INVALID_ARGUMENT = static_cast<int>(0x816F8004);
constexpr int SCE_VNA_ERROR_NOT_FOUND = static_cast<int>(0x816F8005);
constexpr int SCE_VNA_ERROR_UNKNOWN_HANDLE = static_cast<int>(0x816F8007);

constexpr std::array<std::uint32_t, 40> MAJOR_TABLE_CODES = {
    0x816F0000u, 0x816F0004u, 0x816F0020u, 0x816F0008u, 0x816F0022u, 0x816F0023u, 0x816F0024u, 0x816F0025u,
    0x816F0026u, 0x816F0027u, 0x816F0028u, 0x816F0029u, 0x816F002Au, 0x816F002Bu, 0x816F002Cu, 0x816F002Du,
    0x816F0031u, 0x816F0033u, 0x816F0034u, 0x816F0035u, 0x816F0036u, 0x816F0037u, 0x816F0038u, 0x816F0039u,
    0x816F003Au, 0x816F003Eu, 0x816F0040u, 0x816F0041u, 0x816F0042u, 0x816F0043u, 0x816F0044u, 0x816F0045u,
    0x816F0046u, 0x816F0047u, 0x816F0048u, 0x816F0049u, 0x816F004Au, 0x816F804Au, 0x816F801Au, 0x816F804Bu,
};

bool ContainsCode(const std::uint32_t* values, std::size_t count, std::uint32_t code) {
    for (std::size_t i = 0; i < count; ++i) {
        if (values[i] == code) return true;
    }
    return false;
}

const char* ResolveSeverityText(std::int32_t errorCode) {
    const auto code = static_cast<std::uint32_t>(errorCode);
    if (code == 0x81761E0Au || code == 0x81761E0Bu) return "critical";
    if (code == 0u) return "";
    if (ContainsCode(MAJOR_TABLE_CODES.data(), MAJOR_TABLE_CODES.size(), code)) return "major";
    if (errorCode > static_cast<std::int32_t>(0x816F6FFEu)) {
        const std::uint32_t d1 = code - 0x81761E00u;
        if (d1 <= 0xdu) {
            if (((0x370u >> d1) & 1u) != 0u) return "major";
            if (((0x2089u >> d1) & 1u) != 0u) return "minor";
            if (((0x1006u >> d1) & 1u) != 0u) return "info";
        }
        const std::uint32_t d2 = code - 0x816F801Fu;
        if (d2 <= 0x32u && ((0x7620800000009ull >> d2) & 1ull) != 0ull) return "info";
        return "minor";
    }
    const std::uint32_t d3 = code - 0x816F0003u;
    if (d3 <= 0x4au) {
        constexpr std::array<std::uint32_t, 10> infoJumpCodes = {
            0x816F0003u, 0x816F0005u, 0x816F0006u, 0x816F0007u, 0x816F0021u,
            0x816F002Eu, 0x816F003Bu, 0x816F004Bu, 0x816F004Cu, 0x816F004Du,
        };
        constexpr std::array<std::uint32_t, 8> normalJumpCodes = {
            0x816F0010u, 0x816F0011u, 0x816F002Fu, 0x816F0030u,
            0x816F0032u, 0x816F003Cu, 0x816F003Du, 0x816F003Fu,
        };
        if (ContainsCode(infoJumpCodes.data(), infoJumpCodes.size(), code)) return "info";
        if (ContainsCode(normalJumpCodes.data(), normalJumpCodes.size(), code)) return "normal";
        return "minor";
    }
    if (code == 0x816F6FFDu) return "info";
    if (code == 0x816F6FFEu) return "major";
    return "minor";
}

struct UtilVoiceInputState {
    std::mutex mutex;
    std::map<std::int32_t, std::uint64_t> statuses;
};

UtilVoiceInputState& VoiceInputState() {
    static UtilVoiceInputState state;
    return state;
}

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

int APS5_VABI sceVnaGetErrorSeverity(std::int32_t errorCode, char* severity) {
    if (severity == nullptr) return SCE_VNA_ERROR_INVALID_ARGUMENT;
    const char* text = ResolveSeverityText(errorCode);
    const std::size_t len = std::strlen(text) + 1;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(severity, text, len);
    errno = savedErrno;
    if (!copied) return SCE_VNA_ERROR_INVALID_ARGUMENT;
    return 0;
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
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaUtilTerminate() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
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
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
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

int APS5_VABI sceVnaUtilUpdateVoiceInputStatus(const void* status) {
    if (status == nullptr) return SCE_VNA_ERROR_INVALID_ARGUMENT;
    std::uint64_t record = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&record, status, sizeof(record));
    errno = savedErrno;
    if (!copied) return SCE_VNA_ERROR_INVALID_ARGUMENT;
    const auto userId = static_cast<std::int32_t>(record & 0xffffffffu);
    auto& state = VoiceInputState();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.statuses[userId] = record;
    return 0;
}

int APS5_VABI sceVnaSetFeedbackTrigger() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaRequestDownloadCahcedTts() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaUtilIsLanguageSupported() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
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

int APS5_VABI sceVnaUtilRemoveVoiceInputStatus(std::int32_t userId) {
    auto& state = VoiceInputState();
    std::lock_guard<std::mutex> lock(state.mutex);
    return state.statuses.erase(userId) == 1 ? 0 : SCE_VNA_ERROR_NOT_FOUND;
}

int APS5_VABI sceVnaStopKeyPhraseDetection() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

int APS5_VABI sceVnaOpenTtsStream() {
    return SCE_VNA_ERROR_UNKNOWN_HANDLE;
}

}

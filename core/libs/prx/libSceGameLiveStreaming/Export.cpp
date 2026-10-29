#include "prx/libSceGameLiveStreaming/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <string>

static std::mutex g_mutex;
static bool g_initialized = false;
static constexpr int ErrorInvalidArgument = static_cast<int>(0x80A00002);
static constexpr int ErrorAlreadyInitialized = static_cast<int>(0x80A00003);
static constexpr int ErrorNotInitialized = static_cast<int>(0x80A00004);
static constexpr std::size_t ProgramInfoSize = 0x134C;
static constexpr std::size_t MaxMetadataLength = 240;
static constexpr std::size_t MaxPresetDescriptionLength = 720;
static constexpr std::size_t MaxInvitationSessionIdLength = 45;
static constexpr std::size_t MaxSocialMessageLength = 360;
static constexpr std::size_t MaxSpoilerTags = 16;
static constexpr std::size_t MaxPresetCommands = 5;

static bool ValidFeedbackType(int type) {
    return type >= 1 && type <= 3;
}

static bool ValidString(const char* text, std::size_t maxLength) {
    return text != nullptr && std::memchr(text, 0, maxLength + 1) != nullptr;
}

static bool AllZero(const std::uint8_t* bytes, std::size_t size) {
    for (std::size_t i = 0; i < size; i++) {
        if (bytes[i] != 0) return false;
    }
    return true;
}

static_assert(sizeof(GameLiveStreamingStatus2) == 72);

extern "C" {

int APS5_VABI sceGameLiveStreamingInitialize(size_t heapSize) {
    if (heapSize != 0x4000) return ErrorInvalidArgument;
    std::lock_guard lock(g_mutex);
    if (g_initialized) return ErrorAlreadyInitialized;
    g_initialized = true;
    return 0;
}

int APS5_VABI sceGameLiveStreamingTerminate(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    g_initialized = false;
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetCurrentStatus2(GameLiveStreamingStatus2* status) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (status == nullptr) return ErrorInvalidArgument;
    std::memset(status, 0, sizeof(*status));
    status->userId = USER_SERVICE_USER_ID_INVALID;
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetProgramInfo(GameLiveStreamingProgramInfo* info) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (info == nullptr) return ErrorInvalidArgument;
    std::memset(info, 0, ProgramInfoSize);
    return 0;
}

int APS5_VABI sceGameLiveStreamingScreenInitialize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingScreenInitializeSeparateModeParameter(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingScreenTerminate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingScreenCloseSeparateMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingScreenConfigureSeparateMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingScreenOpenSeparateMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingScreenSetMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingPermitLiveStreaming(bool permit) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetMaxBitrate(std::uint32_t bitrate) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetCurrentStatus(GameLiveStreamingStatus* status) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingEnableSocialFeedback(bool enable) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetSocialFeedbackMessagesCount(int type, std::uint32_t* count) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (count == nullptr || !ValidFeedbackType(type)) return ErrorInvalidArgument;
    *count = 0;
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetSocialFeedbackMessages(int type, size_t capacity, GameLiveStreamingFeedbackMessage* messages, size_t* acquired) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (messages == nullptr || acquired == nullptr || !ValidFeedbackType(type)) return ErrorInvalidArgument;
    *acquired = 0;
    return 0;
}

int APS5_VABI sceGameLiveStreamingPermitServerSideRecording(bool permit) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetMetadata(const char* metadata) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (!ValidString(metadata, MaxMetadataLength)) return ErrorInvalidArgument;
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetSpoilerTag(size_t count, const GameLiveStreamingSpoilerTag* tags) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (tags == nullptr || count > MaxSpoilerTags) return ErrorInvalidArgument;
    for (size_t i = 0; i < count; i++) {
        if (!AllZero(tags[i].reserved, sizeof(tags[i].reserved))) return ErrorInvalidArgument;
    }
    return 0;
}

int APS5_VABI sceGameLiveStreamingClearSpoilerTag(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetPresetSocialFeedbackCommands(size_t count, const GameLiveStreamingPresetCommand* commands) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (commands == nullptr || count > MaxPresetCommands) return ErrorInvalidArgument;
    for (size_t i = 0; i < count; i++) {
        if (!AllZero(commands[i].reserved, sizeof(commands[i].reserved))) return ErrorInvalidArgument;
    }
    return 0;
}

int APS5_VABI sceGameLiveStreamingClearPresetSocialFeedbackCommands(void) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription(const char* description) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (!ValidString(description, MaxPresetDescriptionLength)) return ErrorInvalidArgument;
    return 0;
}

int APS5_VABI sceGameLiveStreamingClearSocialFeedbackMessages(int type) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (!ValidFeedbackType(type)) return ErrorInvalidArgument;
    return 0;
}

int APS5_VABI sceGameLiveStreamingPostSocialMessage(const char* message) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (!ValidString(message, MaxSocialMessageLength)) return ErrorInvalidArgument;
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetCameraFrameSetting(const GameLiveStreamingCameraFrameSetting* setting) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (setting == nullptr || !AllZero(setting->reserved, sizeof(setting->reserved))) return ErrorInvalidArgument;
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetServiceProviderPermission(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetDefaultServiceProviderPermission(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetInvitationSessionId(const char* sessionId) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return ErrorNotInitialized;
    if (!ValidString(sessionId, MaxInvitationSessionIdLength)) return ErrorInvalidArgument;
    return 0;
}

int APS5_VABI sceGameLiveStreamingStartGenerateStandbyScreenResource(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingStopGenerateStandbyScreenResource(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingRegisterCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingUnregisterCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingCheckCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetStandbyScreenResource(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetCurrentBroadcastScreenLayout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingLaunchLiveViewer(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingLaunchLiveViewerA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetGuardAreas(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingStartSocialFeedbackMessageFiltering(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingStopSocialFeedbackMessageFiltering(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingApplySocialFeedbackMessageFilter(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetLinkCommentPreset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingStartDebugBroadcast(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingStopDebugBroadcast(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingEnableLiveStreaming(bool isEnableLiveStreaming) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) throw std::logic_error(std::string(__func__) + ": not initialized");
    (void)isEnableLiveStreaming;
    return 0;
}

}

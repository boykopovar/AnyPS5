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

struct GameLiveStreamingProgramInfo;

struct GameLiveStreamingStatus2 {
    std::int32_t userId;
    bool isOnAir;
    std::uint8_t align[3];
    std::uint32_t spectatorCounts;
    std::uint32_t textMessageCounts;
    std::uint32_t commandMessageCounts;
    std::uint32_t broadcastVideoResolution;
    std::uint8_t reserved[48];
};
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

int APS5_VABI sceGameLiveStreamingPermitLiveStreaming(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetMaxBitrate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetCurrentStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingEnableSocialFeedback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetSocialFeedbackMessagesCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingGetSocialFeedbackMessages(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingPermitServerSideRecording(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetMetadata(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetSpoilerTag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingClearSpoilerTag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetPresetSocialFeedbackCommands(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingClearPresetSocialFeedbackCommands(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingClearSocialFeedbackMessages(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingPostSocialMessage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameLiveStreamingSetCameraFrameSetting(void) {
    NotImplemented_nid_no_patch(__func__);
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

int APS5_VABI sceGameLiveStreamingSetInvitationSessionId(void) {
    NotImplemented_nid_no_patch(__func__);
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

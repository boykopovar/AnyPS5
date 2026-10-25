#pragma once

#include "SceTypes.hpp"
#include <cstddef>
#include <cstdint>

struct GameLiveStreamingProgramInfo;

struct GameLiveStreamingStatus {
    std::uint8_t opaque00[12];
    std::uint8_t reserved[60];
};

static_assert(offsetof(GameLiveStreamingStatus, reserved) == 0x0C);
static_assert(sizeof(GameLiveStreamingStatus) == 0x48);

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

extern "C" {

int APS5_VABI sceGameLiveStreamingInitialize(size_t heapSize);
int APS5_VABI sceGameLiveStreamingTerminate(void);
int APS5_VABI sceGameLiveStreamingGetCurrentStatus2(GameLiveStreamingStatus2* status);
int APS5_VABI sceGameLiveStreamingGetProgramInfo(GameLiveStreamingProgramInfo* info);
int APS5_VABI sceGameLiveStreamingScreenInitialize(void);
int APS5_VABI sceGameLiveStreamingScreenInitializeSeparateModeParameter(void);
int APS5_VABI sceGameLiveStreamingScreenTerminate(void);
int APS5_VABI sceGameLiveStreamingScreenCloseSeparateMode(void);
int APS5_VABI sceGameLiveStreamingScreenConfigureSeparateMode(void);
int APS5_VABI sceGameLiveStreamingScreenOpenSeparateMode(void);
int APS5_VABI sceGameLiveStreamingScreenSetMode(void);
int APS5_VABI sceGameLiveStreamingPermitLiveStreaming(void);
int APS5_VABI sceGameLiveStreamingSetMaxBitrate(void);
int APS5_VABI sceGameLiveStreamingGetCurrentStatus(GameLiveStreamingStatus* status);
int APS5_VABI sceGameLiveStreamingEnableSocialFeedback(void);
int APS5_VABI sceGameLiveStreamingGetSocialFeedbackMessagesCount(void);
int APS5_VABI sceGameLiveStreamingGetSocialFeedbackMessages(void);
int APS5_VABI sceGameLiveStreamingPermitServerSideRecording(void);
int APS5_VABI sceGameLiveStreamingSetMetadata(void);
int APS5_VABI sceGameLiveStreamingSetSpoilerTag(void);
int APS5_VABI sceGameLiveStreamingClearSpoilerTag(void);
int APS5_VABI sceGameLiveStreamingSetPresetSocialFeedbackCommands(void);
int APS5_VABI sceGameLiveStreamingClearPresetSocialFeedbackCommands(void);
int APS5_VABI sceGameLiveStreamingSetPresetSocialFeedbackCommandsDescription(void);
int APS5_VABI sceGameLiveStreamingClearSocialFeedbackMessages(void);
int APS5_VABI sceGameLiveStreamingPostSocialMessage(void);
int APS5_VABI sceGameLiveStreamingSetCameraFrameSetting(void);
int APS5_VABI sceGameLiveStreamingSetServiceProviderPermission(void);
int APS5_VABI sceGameLiveStreamingSetDefaultServiceProviderPermission(void);
int APS5_VABI sceGameLiveStreamingSetInvitationSessionId(void);
int APS5_VABI sceGameLiveStreamingStartGenerateStandbyScreenResource(void);
int APS5_VABI sceGameLiveStreamingStopGenerateStandbyScreenResource(void);
int APS5_VABI sceGameLiveStreamingRegisterCallback(void);
int APS5_VABI sceGameLiveStreamingUnregisterCallback(void);
int APS5_VABI sceGameLiveStreamingCheckCallback(void);
int APS5_VABI sceGameLiveStreamingSetStandbyScreenResource(void);
int APS5_VABI sceGameLiveStreamingGetCurrentBroadcastScreenLayout(void);
int APS5_VABI sceGameLiveStreamingLaunchLiveViewer(void);
int APS5_VABI sceGameLiveStreamingLaunchLiveViewerA(void);
int APS5_VABI sceGameLiveStreamingSetGuardAreas(void);
int APS5_VABI sceGameLiveStreamingStartSocialFeedbackMessageFiltering(void);
int APS5_VABI sceGameLiveStreamingStopSocialFeedbackMessageFiltering(void);
int APS5_VABI sceGameLiveStreamingApplySocialFeedbackMessageFilter(void);
int APS5_VABI sceGameLiveStreamingSetLinkCommentPreset(void);
int APS5_VABI sceGameLiveStreamingStartDebugBroadcast(void);
int APS5_VABI sceGameLiveStreamingStopDebugBroadcast(void);
int APS5_VABI sceGameLiveStreamingEnableLiveStreaming(bool isEnableLiveStreaming);

}

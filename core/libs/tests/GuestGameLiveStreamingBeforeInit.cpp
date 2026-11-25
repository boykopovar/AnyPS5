#include "prx/libSceGameLiveStreaming/include/Api.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int notInitialized = static_cast<int>(0x80A00004u);

void CheckNotInitializedCalls() {
    Require(sceGameLiveStreamingScreenInitialize() == notInitialized);
    Require(sceGameLiveStreamingScreenTerminate() == notInitialized);
    Require(sceGameLiveStreamingScreenCloseSeparateMode() == notInitialized);
    Require(sceGameLiveStreamingScreenConfigureSeparateMode(nullptr) == notInitialized);
    Require(sceGameLiveStreamingScreenOpenSeparateMode() == notInitialized);
    Require(sceGameLiveStreamingScreenSetMode(1) == notInitialized);
    Require(sceGameLiveStreamingSetServiceProviderPermission(1, true) == notInitialized);
    Require(sceGameLiveStreamingSetDefaultServiceProviderPermission(true) == notInitialized);
    Require(sceGameLiveStreamingStartGenerateStandbyScreenResource(nullptr) == notInitialized);
    Require(sceGameLiveStreamingStopGenerateStandbyScreenResource() == notInitialized);
    Require(sceGameLiveStreamingCheckCallback() == notInitialized);
    Require(sceGameLiveStreamingSetStandbyScreenResource() == notInitialized);
    Require(sceGameLiveStreamingGetCurrentBroadcastScreenLayout(nullptr) == notInitialized);
    Require(sceGameLiveStreamingSetGuardAreas(0, nullptr) == notInitialized);
    Require(sceGameLiveStreamingStartSocialFeedbackMessageFiltering() == notInitialized);
    Require(sceGameLiveStreamingStopSocialFeedbackMessageFiltering() == notInitialized);
    Require(sceGameLiveStreamingApplySocialFeedbackMessageFilter() == notInitialized);
    Require(sceGameLiveStreamingSetLinkCommentPreset(nullptr) == notInitialized);
    Require(sceGameLiveStreamingStartDebugBroadcast(nullptr) == notInitialized);
    Require(sceGameLiveStreamingStopDebugBroadcast() == notInitialized);
}

}

int main() {
    CheckNotInitializedCalls();
    Require(sceGameLiveStreamingInitialize(0x4000) == 0);
    Require(sceGameLiveStreamingTerminate() == 0);
    CheckNotInitializedCalls();
    return 0;
}

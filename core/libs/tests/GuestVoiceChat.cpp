#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {
int APS5_VABI sceVoiceChatInitialize(void* param);
int APS5_VABI sceVoiceChatTerminate(void);
int APS5_VABI sceVoiceChatRegisterHandlers(void* a);
int APS5_VABI sceVoiceChatRegisterMicEventHandler(void* a, void* b);
int APS5_VABI sceVoiceChatRegisterChannelEventHandler(void* a, void* b);
int APS5_VABI sceVoiceChatUnregisterMicEventHandler(void* a);
int APS5_VABI sceVoiceChatProcessEvent(void);
int APS5_VABI sceVoiceChatStartChannelEvent(void* a);
int APS5_VABI sceVoiceChatStopChannelEvent(void* a);
int APS5_VABI sceVoiceChatGetMicState(int32_t* state);
int APS5_VABI sceVoiceChatGetUserVoiceVolumeGain(void* a, int32_t* gain);
int APS5_VABI sceVoiceChatSetUserVoiceVolumeGain(void* a, int32_t gain);
int APS5_VABI sceVoiceChatGetActiveChannel(void* channel);
int APS5_VABI sceVoiceChatCreateRequest(void* a, void* b);
int APS5_VABI sceVoiceChatRequestAcsTts(int requestId, void* a, void* b);
int APS5_VABI sceVoiceChatAbortRequest(int requestId);
}

static void Require(bool value, const char* what) {
    if (value) return;
    std::printf("VoiceChat: %s\n", what);
    std::abort();
}

int main() {
    constexpr int kSignedOut = static_cast<int>(0x80550006u);

    Require(sceVoiceChatInitialize(nullptr) == 0, "initialization failed");
    Require(sceVoiceChatProcessEvent() == 0, "event processing failed");
    Require(sceVoiceChatRegisterHandlers(nullptr) == 0, "register handlers failed");
    Require(sceVoiceChatRegisterMicEventHandler(nullptr, nullptr) == 0, "register mic event handler failed");
    Require(sceVoiceChatRegisterChannelEventHandler(nullptr, nullptr) == 0, "register channel event handler failed");
    Require(sceVoiceChatUnregisterMicEventHandler(nullptr) == 0, "unregister mic event handler failed");
    Require(sceVoiceChatStartChannelEvent(nullptr) == 0, "start channel event failed");
    Require(sceVoiceChatStopChannelEvent(nullptr) == 0, "stop channel event failed");

    Require(sceVoiceChatGetMicState(nullptr) == kSignedOut, "get mic state did not return signed out");
    Require(sceVoiceChatGetUserVoiceVolumeGain(nullptr, nullptr) == kSignedOut, "get volume gain did not return signed out");
    Require(sceVoiceChatSetUserVoiceVolumeGain(nullptr, 50) == kSignedOut, "set volume gain did not return signed out");

    Require(sceVoiceChatGetActiveChannel(nullptr) == kSignedOut, "get active channel did not return signed out");
    Require(sceVoiceChatCreateRequest(nullptr, nullptr) == kSignedOut, "create request did not return signed out");
    Require(sceVoiceChatRequestAcsTts(1, nullptr, nullptr) == kSignedOut, "tts request did not return signed out");
    Require(sceVoiceChatAbortRequest(1) == 0, "abort request failed");
    Require(sceVoiceChatTerminate() == 0, "termination failed");

    std::puts("VoiceChat tests passed");
    return 0;
}

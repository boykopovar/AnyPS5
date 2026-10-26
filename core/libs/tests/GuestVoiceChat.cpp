#include "prx/libSceVoiceChat/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

extern "C" {

int APS5_VABI sceVoiceChatRequestAcsTts(int requestId, void* a, void* b);

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
    Require(sceVoiceChatRegisterHandlers(nullptr, nullptr) == 0, "register handlers failed");
    Require(sceVoiceChatRegisterMicEventHandler(0, nullptr, nullptr) == 0, "register mic event handler failed");
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

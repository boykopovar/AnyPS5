#include "prx/libSceVoiceChat/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

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
    Require(sceVoiceChatRegisterDaemonEventHandler(nullptr, nullptr) == 0, "register daemon event handler failed");
    Require(sceVoiceChatRegisterMemberEventHandler(nullptr, nullptr) == 0, "register member event handler failed");
    Require(sceVoiceChatRegisterPartyUserStateChangedHandler(nullptr, nullptr) == 0, "register party user state changed handler failed");
    Require(sceVoiceChatRegisterVoiceEventHandler(nullptr, nullptr) == 0, "register voice event handler failed");
    Require(sceVoiceChatUnregisterMicEventHandler(nullptr) == 0, "unregister mic event handler failed");
    Require(sceVoiceChatUnregisterChannelEventHandler(nullptr) == 0, "unregister channel event handler failed");
    Require(sceVoiceChatUnregisterDaemonEventHandler(nullptr) == 0, "unregister daemon event handler failed");
    Require(sceVoiceChatUnregisterMemberEventHandler(nullptr) == 0, "unregister member event handler failed");
    Require(sceVoiceChatUnregisterPartyUserStateChangedHandler(nullptr) == 0, "unregister party user state changed handler failed");
    Require(sceVoiceChatUnregisterVoiceEventHandler(nullptr) == 0, "unregister voice event handler failed");
    Require(sceVoiceChatStartChannelEvent(nullptr) == 0, "start channel event failed");
    Require(sceVoiceChatStopChannelEvent(nullptr) == 0, "stop channel event failed");
    Require(sceVoiceChatStartDaemonEvent(nullptr) == 0, "start daemon event failed");
    Require(sceVoiceChatStopDaemonEvent(nullptr) == 0, "stop daemon event failed");
    Require(sceVoiceChatStartMemberEvent(nullptr) == 0, "start member event failed");
    Require(sceVoiceChatStopMemberEvent(nullptr) == 0, "stop member event failed");
    Require(sceVoiceChatStartVoiceEvent(nullptr) == 0, "start voice event failed");
    Require(sceVoiceChatStopVoiceEvent(nullptr) == 0, "stop voice event failed");

    Require(sceVoiceChatGetMicState(nullptr) == kSignedOut, "get mic state did not return signed out");
    Require(sceVoiceChatGetUserVoiceVolumeGain(nullptr, nullptr) == kSignedOut, "get volume gain did not return signed out");
    Require(sceVoiceChatSetUserVoiceVolumeGain(nullptr, 50) == kSignedOut, "set volume gain did not return signed out");
    Require(sceVoiceChatGetChannelMemberActiveChannelMatchingState(nullptr, nullptr) == kSignedOut, "matching state did not return signed out");
    Require(sceVoiceChatGetChannelMemberVoiceConnectionState(nullptr, nullptr) == kSignedOut, "voice connection state did not return signed out");
    Require(sceVoiceChatGetChatChannelMemberInfoList(nullptr, nullptr) == kSignedOut, "channel member list did not return signed out");
    Require(sceVoiceChatGetChatGroupMemberInfoList(nullptr, nullptr) == kSignedOut, "group member list did not return signed out");

    Require(sceVoiceChatGetActiveChannel(nullptr) == kSignedOut, "get active channel did not return signed out");
    Require(sceVoiceChatCreateRequest(nullptr, nullptr) == kSignedOut, "create request did not return signed out");
    Require(sceVoiceChatRequestRegisterSession(1, nullptr, nullptr) == kSignedOut, "register session did not return signed out");
    Require(sceVoiceChatRequestUnregisterSession(1, nullptr, nullptr) == kSignedOut, "unregister session did not return signed out");
    Require(sceVoiceChatRequestCreateGameSessionVoiceChatChannel(1, nullptr, nullptr) == kSignedOut, "create game session did not return signed out");
    Require(sceVoiceChatRequestDeleteGameSessionVoiceChatChannel(1, nullptr, nullptr) == kSignedOut, "delete game session did not return signed out");
    Require(sceVoiceChatRequestJoinGameSessionVoiceChatChannel(1, nullptr, nullptr) == kSignedOut, "join game session did not return signed out");
    Require(sceVoiceChatRequestLeaveGameSessionVoiceChatChannel(1, nullptr, nullptr) == kSignedOut, "leave game session did not return signed out");
    Require(sceVoiceChatRequestCreatePlayerSessionVoiceChatChannel(1, nullptr, nullptr) == kSignedOut, "create player session did not return signed out");
    Require(sceVoiceChatRequestDeletePlayerSessionVoiceChatChannel(1, nullptr, nullptr) == kSignedOut, "delete player session did not return signed out");
    Require(sceVoiceChatRequestCreateVoiceChatGroup(1, nullptr, nullptr) == kSignedOut, "create voice chat group did not return signed out");
    Require(sceVoiceChatRequestJoinVoiceChatGroup(1, nullptr, nullptr) == kSignedOut, "join voice chat group did not return signed out");
    Require(sceVoiceChatRequestLeaveVoiceChatGroup(1, nullptr, nullptr) == kSignedOut, "leave voice chat group did not return signed out");
    Require(sceVoiceChatRequestDeleteVoiceChatGroup(1, nullptr, nullptr) == kSignedOut, "delete voice chat group did not return signed out");
    Require(sceVoiceChatRequestAcsTts(1, nullptr, nullptr) == kSignedOut, "tts request did not return signed out");
    Require(sceVoiceChatAbortRequest(1) == 0, "abort request failed");
    Require(sceVoiceChatDeleteRequest(1) == 0, "delete request failed");
    Require(sceVoiceChatTerminate() == 0, "termination failed");

    std::puts("VoiceChat tests passed");
    return 0;
}

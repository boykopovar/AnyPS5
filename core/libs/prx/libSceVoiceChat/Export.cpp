#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr int VOICECHAT_ERROR_SIGNED_OUT = static_cast<int>(0x80550006u);
}  // namespace

namespace {
void logSoft(const char* func, const char* what) {
    static std::mutex mtx;
    static std::map<std::string, int> hits;
    std::lock_guard<std::mutex> lk(mtx);
    if (++hits[func] > 3) {
        return;
    }
    std::fprintf(stderr, "[SOFT-VOICECHAT] %s -> %s\n", func, what);
    std::fflush(stderr);
}
}  // namespace

extern "C" {

int APS5_VABI sceVoiceChatInitialize(void* param) { (void)param; return 0; }
int APS5_VABI sceVoiceChatTerminate(void) { return 0; }
int APS5_VABI sceVoiceChatRegisterHandlers(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatRegisterMicEventHandler(void* a, void* b) { (void)a; (void)b; return 0; }
int APS5_VABI sceVoiceChatProcessEvent(void) { return 0; }
int APS5_VABI sceVoiceChatDeleteRequest(int requestId) { (void)requestId; return 0; }

int APS5_VABI sceVoiceChatCreateRequest(void* a, void* b) {
    (void)a;
    (void)b;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

#define VOICECHAT_OFFLINE_REQUEST(NAME) \
    int APS5_VABI NAME(int requestId, void* a, void* b) { \
        (void)requestId; \
        (void)a; \
        (void)b; \
        logSoft(#NAME, "SIGNED_OUT (offline)"); \
        return VOICECHAT_ERROR_SIGNED_OUT; \
    }

VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestRegisterSession)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestUnregisterSession)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestCreateGameSessionVoiceChatChannel)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestDeleteGameSessionVoiceChatChannel)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestJoinGameSessionVoiceChatChannel)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestLeaveGameSessionVoiceChatChannel)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestCreatePlayerSessionVoiceChatChannel)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestDeletePlayerSessionVoiceChatChannel)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestJoinPlayerSessionVoiceChatChannel)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestLeavePlayerSessionVoiceChatChannel)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestCreateVoiceChatGroup)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestJoinVoiceChatGroup)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestLeaveVoiceChatGroup)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestDeleteVoiceChatGroup)

int APS5_VABI sceVoiceChatAbortRequest(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatGetActiveChannel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatGetChannelMemberActiveChannelMatchingState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatGetChannelMemberVoiceConnectionState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatGetChatChannelMemberInfoList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatGetChatGroupMemberInfoList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatGetMicState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatGetUserVoiceVolumeGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRegisterChannelEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRegisterDaemonEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRegisterMemberEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRegisterPartyUserStateChangedHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRegisterVoiceEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRequest0(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRequest1(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRequestAcsTts(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRequestAction(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRequestSetMemberVolume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRequestUpdateGameSessionVoiceChatChannelName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatRequestUpdatePlayerSessionVoiceChatChannelName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatSetUserVoiceVolumeGain(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStartAccessibilityEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStartChannelEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStartDaemonEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStartMemberEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStartRangedMemberEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStartVoiceEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStopAccessibilityEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStopChannelEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStopDaemonEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStopMemberEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStopRangedMemberEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatStopVoiceEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatUnregisterAccessibilityEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatUnregisterChannelEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatUnregisterDaemonEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatUnregisterMemberEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatUnregisterMicEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatUnregisterPartyUserStateChangedHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatUnregisterRangedMemberEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceChatUnregisterVoiceEventHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

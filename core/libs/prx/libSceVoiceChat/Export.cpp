#include "prx/libSceVoiceChat/include/Api.hpp"
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

int offlineRequest(const char* func) {
    logSoft(func, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}
}  // namespace

extern "C" {

int APS5_VABI sceVoiceChatInitialize(void* param) { (void)param; return 0; }
int APS5_VABI sceVoiceChatTerminate(void) { return 0; }
int APS5_VABI sceVoiceChatRegisterHandlers(void* handlers, void* userArg) { (void)handlers; (void)userArg; return 0; }
int APS5_VABI sceVoiceChatRegisterMicEventHandler(std::int32_t userId, void* handler, void* userArg) { (void)userId; (void)handler; (void)userArg; return 0; }
int APS5_VABI sceVoiceChatProcessEvent(void) { return 0; }
int APS5_VABI sceVoiceChatDeleteRequest(int requestId) { (void)requestId; return 0; }

int APS5_VABI sceVoiceChatCreateRequest(void* a, void* b) {
    (void)a;
    (void)b;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

int APS5_VABI sceVoiceChatRequestRegisterSession(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestUnregisterSession(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestCreateGameSessionVoiceChatChannel(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestDeleteGameSessionVoiceChatChannel(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestJoinGameSessionVoiceChatChannel(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestLeaveGameSessionVoiceChatChannel(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestCreatePlayerSessionVoiceChatChannel(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestDeletePlayerSessionVoiceChatChannel(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestJoinPlayerSessionVoiceChatChannel(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestLeavePlayerSessionVoiceChatChannel(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestCreateVoiceChatGroup(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestJoinVoiceChatGroup(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestLeaveVoiceChatGroup(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestDeleteVoiceChatGroup(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }

int APS5_VABI sceVoiceChatRequest0(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequest1(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestAcsTts(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestAction(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestSetMemberVolume(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestUpdateGameSessionVoiceChatChannelName(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }
int APS5_VABI sceVoiceChatRequestUpdatePlayerSessionVoiceChatChannelName(int requestId, void* a, void* b) { (void)requestId; (void)a; (void)b; return offlineRequest(__func__); }

int APS5_VABI sceVoiceChatAbortRequest(int requestId) {
    (void)requestId;
    return 0;
}

int APS5_VABI sceVoiceChatGetActiveChannel(void* channel) {
    (void)channel;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

int APS5_VABI sceVoiceChatGetChannelMemberActiveChannelMatchingState(void* a, void* b) {
    (void)a;
    (void)b;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

int APS5_VABI sceVoiceChatGetChannelMemberVoiceConnectionState(void* a, void* b) {
    (void)a;
    (void)b;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

int APS5_VABI sceVoiceChatGetChatChannelMemberInfoList(void* a, void* b) {
    (void)a;
    (void)b;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

int APS5_VABI sceVoiceChatGetChatGroupMemberInfoList(void* a, void* b) {
    (void)a;
    (void)b;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

int APS5_VABI sceVoiceChatGetMicState(void* state) {
    (void)state;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

int APS5_VABI sceVoiceChatGetUserVoiceVolumeGain(void* a, void* gain) {
    (void)a;
    (void)gain;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

int APS5_VABI sceVoiceChatSetUserVoiceVolumeGain(void* a, int32_t gain) {
    (void)a;
    (void)gain;
    logSoft(__func__, "SIGNED_OUT (offline)");
    return VOICECHAT_ERROR_SIGNED_OUT;
}

int APS5_VABI sceVoiceChatRegisterChannelEventHandler(void* a, void* b) { (void)a; (void)b; return 0; }
int APS5_VABI sceVoiceChatRegisterDaemonEventHandler(void* a, void* b) { (void)a; (void)b; return 0; }
int APS5_VABI sceVoiceChatRegisterMemberEventHandler(void* a, void* b) { (void)a; (void)b; return 0; }
int APS5_VABI sceVoiceChatRegisterPartyUserStateChangedHandler(void* a, void* b) { (void)a; (void)b; return 0; }
int APS5_VABI sceVoiceChatRegisterVoiceEventHandler(void* a, void* b) { (void)a; (void)b; return 0; }

int APS5_VABI sceVoiceChatStartAccessibilityEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStartChannelEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStartDaemonEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStartMemberEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStartRangedMemberEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStartVoiceEvent(void* a) { (void)a; return 0; }

int APS5_VABI sceVoiceChatStopAccessibilityEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStopChannelEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStopDaemonEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStopMemberEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStopRangedMemberEvent(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatStopVoiceEvent(void* a) { (void)a; return 0; }

int APS5_VABI sceVoiceChatUnregisterAccessibilityEventHandler(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatUnregisterChannelEventHandler(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatUnregisterDaemonEventHandler(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatUnregisterMemberEventHandler(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatUnregisterMicEventHandler(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatUnregisterPartyUserStateChangedHandler(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatUnregisterRangedMemberEventHandler(void* a) { (void)a; return 0; }
int APS5_VABI sceVoiceChatUnregisterVoiceEventHandler(void* a) { (void)a; return 0; }

}

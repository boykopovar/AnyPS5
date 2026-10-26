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

VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequest0)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequest1)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestAcsTts)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestAction)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestSetMemberVolume)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestUpdateGameSessionVoiceChatChannelName)
VOICECHAT_OFFLINE_REQUEST(sceVoiceChatRequestUpdatePlayerSessionVoiceChatChannelName)

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

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
}

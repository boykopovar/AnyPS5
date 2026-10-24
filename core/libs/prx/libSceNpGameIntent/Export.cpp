#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int NpGameIntentErrorInvalidArgument = -2141898748;
constexpr int NpGameIntentErrorIntentNotFound = -2141898746;
constexpr int NpGameIntentErrorValueNotFound = -2141898745;
constexpr int NpGameIntentErrorNotInitialized = -2141898750;
constexpr int NpGameIntentUserIdInvalid = -1;

std::atomic_bool& AppLauncherInitialized() {
    static std::atomic_bool initialized{false};
    return initialized;
}


}

extern "C" {

int APS5_VABI sceNpGameIntentGetPropertyValueString(const NpGameIntentData* intentData, const char* key, char* valueBuf, size_t bufSize) {
 if (intentData == nullptr || key == nullptr || valueBuf == nullptr || bufSize == 0) {
  APS5_INVALID_ARG_EX;
 }

 valueBuf[0] = '\0';
 return NpGameIntentErrorValueNotFound;
}

int APS5_VABI sceNpGameIntentInitialize(const void* initParam) {
 (void)initParam;
 return 0;
}

int APS5_VABI sceNpGameIntentReceiveIntent(NpGameIntentInfo* intentInfo) {
 if (intentInfo == nullptr) {
  APS5_INVALID_ARG_EX;
 }

 intentInfo->user_id = NpGameIntentUserIdInvalid;
 std::memset(intentInfo->intent_type, 0, sizeof(intentInfo->intent_type));
 std::memset(&intentInfo->intent_data, 0, sizeof(intentInfo->intent_data));

 return NpGameIntentErrorIntentNotFound;
}

int APS5_VABI sceNpGameIntentTerminate(void) {
 return 0;
}

int APS5_VABI sceNpGameIntentInitParamInit(NpGameIntentInitParam* param) {
    if (param == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    std::memset(param, 0, sizeof(NpGameIntentInitParam));
    param->size = sizeof(NpGameIntentInitParam);
    return 0;
}

int APS5_VABI sceNpGameIntentInfoInit(NpGameIntentInfo* info) {
    if (info == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    std::memset(info, 0, sizeof(NpGameIntentInfo));
    info->size = sizeof(NpGameIntentInfo);
    info->user_id = NpGameIntentUserIdInvalid;
    return 0;
}

int APS5_VABI sceNpGameIntentVshInitialize(void) {
    return 0;
}

int APS5_VABI sceNpGameIntentVshTerminate(void) {
    return 0;
}

int APS5_VABI sceNpAppLauncherInitialize(const void* initParam) {
    (void)initParam;
    AppLauncherInitialized() = true;
    return 0;
}

int APS5_VABI sceNpAppLauncherLaunchApp(std::int32_t arg0, const char* arg2, const char* arg3) {
    (void)arg0;
    if (!AppLauncherInitialized()) return NpGameIntentErrorNotInitialized;
    if (arg2 == nullptr || *arg2 == '\0' || (arg3 != nullptr && std::memchr(arg3, '\0', 0x2001) == nullptr)) return NpGameIntentErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpAppLauncherTerminate() {
    AppLauncherInitialized() = false;
    return 0;
}

int APS5_VABI sceNpGameIntentGetUdsNpComId() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpGameIntentLaunchApp() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpGameIntentLaunchApp2() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpGameIntentNotifyAppLaunched() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionServiceGetJoinedPartySession() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionServiceGetRunningGamePlayerSession() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionServiceSendPlayerSessionInvitations() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

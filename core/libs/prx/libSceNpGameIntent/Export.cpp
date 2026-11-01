#include "prx/libSceNpGameIntent/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int NpGameIntentErrorNotInitialized = -2141898750;
constexpr int NpGameIntentErrorInvalidArgument = -2141898748;
constexpr int NpGameIntentErrorIntentNotFound = -2141898746;
constexpr int NpGameIntentErrorValueNotFound = -2141898745;
constexpr int NpGameIntentUserIdInvalid = -1;
constexpr int NpAppLauncherErrorNotInitialized = -2141898238;
constexpr int NpAppLauncherErrorInvalidArgument = -2141898236;

std::atomic_bool& NpGameIntentInitialized() {
    static std::atomic_bool initialized{false};
    return initialized;
}

std::atomic_bool& AppLauncherInitialized() {
    static std::atomic_bool initialized{false};
    return initialized;
}

}

extern "C" {

int APS5_VABI sceNpGameIntentGetPropertyValueString(const NpGameIntentData* intentData, const char* key, char* valueBuf, size_t bufSize) {
	if (!NpGameIntentInitialized()) {
		return NpGameIntentErrorNotInitialized;
	}
	if (intentData == nullptr || key == nullptr || key[0] == '\0' || valueBuf == nullptr || bufSize == 0) {
		return NpGameIntentErrorInvalidArgument;
	}

	valueBuf[0] = '\0';
	return NpGameIntentErrorValueNotFound;
}

int APS5_VABI sceNpGameIntentInitialize(const void* initParam) {
	(void)initParam;
	NpGameIntentInitialized() = true;
	return 0;
}

int APS5_VABI sceNpGameIntentReceiveIntent(NpGameIntentInfo* intentInfo) {
	if (!NpGameIntentInitialized()) {
		return NpGameIntentErrorNotInitialized;
	}
	if (intentInfo == nullptr || intentInfo->size != 0x4140) {
		return NpGameIntentErrorInvalidArgument;
	}

	intentInfo->user_id = NpGameIntentUserIdInvalid;
	std::memset(intentInfo->intent_type, 0, sizeof(intentInfo->intent_type));
	std::memset(&intentInfo->intent_data, 0, sizeof(intentInfo->intent_data));

	return NpGameIntentErrorIntentNotFound;
}

int APS5_VABI sceNpGameIntentTerminate(void) {
	NpGameIntentInitialized() = false;
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
    if (!AppLauncherInitialized()) return NpAppLauncherErrorNotInitialized;
    if (arg2 == nullptr || *arg2 == '\0' || (arg3 != nullptr && std::memchr(arg3, '\0', 0x2001) == nullptr)) return NpAppLauncherErrorInvalidArgument;
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

int APS5_VABI sceNpSessionServiceInitialize() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpSessionServiceGetRunningGamePlayerSession() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionServiceSendPlayerSessionInvitations() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpSessionServiceSendPlayerSessionRequestToJoin() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpSessionServiceTerminate() {
    NotImplemented_nid_no_patch(__func__);
}

}

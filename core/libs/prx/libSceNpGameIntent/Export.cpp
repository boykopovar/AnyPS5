#include "prx/libSceNpGameIntent/include/InventoryExports.hpp"
#include "prx/libSceNpGameIntent/include/Api.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
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

bool ReadGuestBytePreservingErrno_nid_no_patch(const char* source, char& value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(value));
    errno = savedErrno;
    return copied;
}

}

extern "C" {

int APS5_VABI sceNpGameIntentGetPermittedIntentTypes() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpGameIntentLaunchAppForPinvoke() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpGameIntentLaunchAppRequestToGkp() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpGameIntentRemoveLaunchCallback() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNpGameIntentRequest() {
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI sceNpGameIntentGetPropertyValueString(const NpGameIntentData* intentData, const char* key, char* valueBuf, size_t bufSize) {
	if (!NpGameIntentInitialized()) {
		return NpGameIntentErrorNotInitialized;
	}
	if (intentData == nullptr || key == nullptr || valueBuf == nullptr || bufSize == 0) {
		return NpGameIntentErrorInvalidArgument;
	}

	const int savedErrno = errno;
	char firstKeyByte{};
	const bool keyReadable = GuestCopyIn_nid_no_patch(&firstKeyByte, key, 1);
	errno = savedErrno;
	if (!keyReadable || firstKeyByte == '\0') {
		return NpGameIntentErrorInvalidArgument;
	}

	const char terminator = '\0';
	const bool outputWritable = GuestCopyOut_nid_no_patch(valueBuf, &terminator, 1);
	errno = savedErrno;
	return outputWritable ? NpGameIntentErrorValueNotFound : NpGameIntentErrorInvalidArgument;
}

int APS5_VABI sceNpGameIntentInitialize(const void* initParam) {
	if (initParam == nullptr) {
		return NpGameIntentErrorInvalidArgument;
	}
	NpGameIntentInitialized() = true;
	return 0;
}

int APS5_VABI sceNpGameIntentReceiveIntent(NpGameIntentInfo* intentInfo) {
	if (!NpGameIntentInitialized()) {
		return NpGameIntentErrorNotInitialized;
	}
	if (intentInfo == nullptr) {
		return NpGameIntentErrorInvalidArgument;
	}
	NpGameIntentInfo localInfo{};
	const int savedErrno = errno;
	const bool copiedIn = GuestCopyIn_nid_no_patch(&localInfo, intentInfo, sizeof(localInfo));
	errno = savedErrno;
	if (!copiedIn || localInfo.size != 0x4140) {
		return NpGameIntentErrorInvalidArgument;
	}
	localInfo.user_id = NpGameIntentUserIdInvalid;
	std::memset(localInfo.intent_type, 0, sizeof(localInfo.intent_type));
	std::memset(&localInfo.intent_data, 0, sizeof(localInfo.intent_data));
	const int savedErrnoOut = errno;
	const bool copiedOut = GuestCopyOut_nid_no_patch(intentInfo, &localInfo, sizeof(localInfo));
	errno = savedErrnoOut;
	if (!copiedOut) {
		return NpGameIntentErrorInvalidArgument;
	}
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
    NpGameIntentInitParam localParam{};
    localParam.size = sizeof(NpGameIntentInitParam);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(param, &localParam, sizeof(localParam));
    errno = savedErrno;
    if (!copied) {
        APS5_INVALID_ARG_EX;
    }
    return 0;
}

int APS5_VABI sceNpGameIntentInfoInit(NpGameIntentInfo* info) {
    if (info == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    NpGameIntentInfo localInfo{};
    localInfo.size = sizeof(NpGameIntentInfo);
    localInfo.user_id = NpGameIntentUserIdInvalid;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &localInfo, sizeof(localInfo));
    errno = savedErrno;
    if (!copied) {
        APS5_INVALID_ARG_EX;
    }
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
    char first = 0;
    if (arg2 == nullptr || !ReadGuestBytePreservingErrno_nid_no_patch(arg2, first) || first == '\0') return NpAppLauncherErrorInvalidArgument;
    if (arg3 != nullptr) {
        std::string optionalArgument;
        const int savedErrno = errno;
        const int copyResult = CopyGuestString_nid_no_patch(arg3, 0x2001, optionalArgument);
        errno = savedErrno;
        if (copyResult != 0) return NpAppLauncherErrorInvalidArgument;
    }
    return NpAppLauncherErrorNotInitialized;
}

int APS5_VABI sceNpAppLauncherTerminate() {
    AppLauncherInitialized() = false;
    return 0;
}

int APS5_VABI sceNpGameIntentGetUdsNpComId() {
    if (!NpGameIntentInitialized()) {
        return NpGameIntentErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpGameIntentLaunchApp() {
    if (!NpGameIntentInitialized()) {
        return NpGameIntentErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpGameIntentLaunchApp2() {
    if (!NpGameIntentInitialized()) {
        return NpGameIntentErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpGameIntentNotifyAppLaunched() {
    if (!NpGameIntentInitialized()) {
        return NpGameIntentErrorNotInitialized;
    }
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

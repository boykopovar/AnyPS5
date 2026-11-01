#ifndef CORE_LIBS_PRX_LIBSCEUSERSERVICE_USERSERVICE_HPP
#define CORE_LIBS_PRX_LIBSCEUSERSERVICE_USERSERVICE_HPP

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"

struct UserServiceInitParam {
 std::int32_t priority;
};

static_assert(sizeof(UserServiceInitParam) == 4);

constexpr int USER_SERVICE_OK = 0;
constexpr int USER_SERVICE_ERROR_NOT_INITIALIZED = static_cast<int>(0x80960002u);
constexpr int USER_SERVICE_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80960003u);
constexpr int USER_SERVICE_ERROR_INVALID_ARGUMENT = -2137653243;
constexpr int USER_SERVICE_ERROR_NO_EVENT = -2137653241;
constexpr int USER_SERVICE_ERROR_NOT_LOGGED_IN = -2137653239;
constexpr int USER_SERVICE_USER_ID_INVALID = -1;
constexpr int USER_SERVICE_INITIAL_USER_ID = 0x10000000;
constexpr int USER_SERVICE_THREAD_PRIORITY_MIN = 0x100;
constexpr int USER_SERVICE_THREAD_PRIORITY_MAX = 0x2FF;
constexpr int USER_SERVICE_ERROR_BUFFER_TOO_SHORT = -2137653238;
constexpr int USER_SERVICE_ERROR_UNKNOWN_USER = static_cast<int>(0x80960105u);
constexpr std::size_t USER_SERVICE_MAX_USER_NAME_LENGTH = 16;
constexpr const char* USER_SERVICE_INITIAL_USER_NAME = "Player";
constexpr int USER_SERVICE_MAX_USERS = 4;
constexpr const char* USER_SERVICE_USER_NAMES[USER_SERVICE_MAX_USERS] = {
    "Player", "Player2", "Player3", "Player4"
};

extern "C" void UserServiceSetUserConnected_nid_postfix(int userIndex, bool connected);

extern "C" {

int APS5_VABI sceUserServiceGetAccessibilityChatTranscription(int userId, int32_t* chatTranscription);
int APS5_VABI sceUserServiceGetAccessibilityKeyremapEnable(std::int32_t userId, std::int32_t* enabled);
int APS5_VABI sceUserServiceGetAccessibilityPressAndHoldDelay(int userId, int32_t* pressAndHoldDelay);
int APS5_VABI sceUserServiceGetAccessibilityTriggerEffect(int userId, int32_t* triggerEffect);
int APS5_VABI sceUserServiceGetAccessibilityVibration(int userId, int32_t* vibration);
int APS5_VABI sceUserServiceGetAccessibilityZoom(std::int32_t userId, std::int32_t* zoom);
int APS5_VABI sceUserServiceGetAccessibilityZoomEnabled(int userId, int32_t* zoomEnabled);
int APS5_VABI sceUserServiceGetAccessibilityZoomFollowFocus(int userId, int32_t* zoomFollowFocus);
int APS5_VABI sceUserServiceGetAgeLevel(int userId, uint32_t* ageLevel);
int APS5_VABI sceUserServiceGetEvent(SceUserServiceEvent* event);
int APS5_VABI sceUserServiceGetGamePresets(int userId, UserServiceGamePresets* presets);
int APS5_VABI sceUserServiceGetInitialUser(int* userId);
int APS5_VABI sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* userIdList);
int APS5_VABI sceUserServiceGetUserName(int userId, char* name, size_t size);
int APS5_VABI sceUserServiceGetUserNumber(int userId, int32_t* number);
int APS5_VABI sceUserServiceInitialize(const UserServiceInitParam* params);
int APS5_VABI sceUserServiceInitialize2(std::int32_t priority, KernelCpumask affinityMask);
int APS5_VABI sceUserServiceTerminate(void);
int APS5_VABI sceUserServiceGetPlatformPrivacyWs1(int32_t userId, int32_t* value);
int APS5_VABI sceUserServiceGetForegroundUser(int* userId);
int APS5_VABI sceUserServiceGetRegisteredUserIdList(UserServiceRegisteredUserIdList* userIdList);
int APS5_VABI sceUserServiceGetUserColor(int userId, int* color);
int APS5_VABI sceUserServiceGetKeyboardType(int userId, int32_t* type);
int APS5_VABI sceUserServiceGetJapaneseInputType(int userId, int32_t* type);
int APS5_VABI sceUserServiceGetTraditionalChineseInputType(int userId, int32_t* type);
int APS5_VABI sceUserServiceGetNpAccountId(int userId, uint64_t* accountId);

}

#endif

#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceUserServiceGetForegroundUser(int* user_id);
int APS5_VABI sceUserServiceGetRegisteredUserIdList(UserServiceRegisteredUserIdList* user_id_list);
int APS5_VABI sceUserServiceGetUserColor(int user_id, int* color);
int APS5_VABI sceUserServiceGetNpAccountId(int user_id, std::uint64_t* account_id);
int APS5_VABI sceUserServiceGetTriggerEffectEnabled(int user_id, int* enabled);
int APS5_VABI sceUserServiceGetTriggerEffectStrength(int user_id, int* strength);
int APS5_VABI sceUserServiceGetVibrationStrength(int user_id, int* strength);
int APS5_VABI sceUserServiceGetMonoOutput(int user_id, int* mono_output);
int APS5_VABI sceUserServiceGetPlatformPrivacyWs1Internal(int32_t user_id, int32_t* value);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    int foreground = -1;
    Require(sceUserServiceGetForegroundUser(&foreground) == USER_SERVICE_OK);
    Require(foreground == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceGetRegisteredUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    UserServiceRegisteredUserIdList registered{};
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    int color = -1;
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserColor(123, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, &color) == USER_SERVICE_OK);
    Require(color == 0);

    std::uint64_t account_id = 0xdeadbeefu;
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(123, &account_id) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, &account_id) == USER_SERVICE_OK);
    Require(account_id == 0);

    int triggerEnabled = -1;
    Require(sceUserServiceGetTriggerEffectEnabled(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetTriggerEffectEnabled(123, &triggerEnabled) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetTriggerEffectEnabled(USER_SERVICE_INITIAL_USER_ID, &triggerEnabled) == USER_SERVICE_OK);
    Require(triggerEnabled == 1);

    int triggerStrength = -1;
    Require(sceUserServiceGetTriggerEffectStrength(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetTriggerEffectStrength(123, &triggerStrength) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetTriggerEffectStrength(USER_SERVICE_INITIAL_USER_ID, &triggerStrength) == USER_SERVICE_OK);
    Require(triggerStrength == 3);

    int vibrationStrength = -1;
    Require(sceUserServiceGetVibrationStrength(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetVibrationStrength(123, &vibrationStrength) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetVibrationStrength(USER_SERVICE_INITIAL_USER_ID, &vibrationStrength) == USER_SERVICE_OK);
    Require(vibrationStrength == 3);

    int monoOutput = -1;
    Require(sceUserServiceGetMonoOutput(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetMonoOutput(123, &monoOutput) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetMonoOutput(USER_SERVICE_INITIAL_USER_ID, &monoOutput) == USER_SERVICE_OK);
    Require(monoOutput == 0);

    int32_t privacyValue = -1;
    Require(sceUserServiceGetPlatformPrivacyWs1Internal(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetPlatformPrivacyWs1Internal(123, &privacyValue) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetPlatformPrivacyWs1Internal(USER_SERVICE_INITIAL_USER_ID, &privacyValue) == USER_SERVICE_OK);
    Require(privacyValue == 0);
}


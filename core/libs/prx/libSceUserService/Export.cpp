#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceUserService/UserService.hpp"

extern "C" {

int sceUserServiceGetAccessibilityChatTranscription(int user_id, int32_t* chat_transcription) {
 (void)user_id;
 (void)chat_transcription;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceGetAccessibilityPressAndHoldDelay(int user_id, int32_t* press_and_hold_delay) {
 (void)user_id;
 (void)press_and_hold_delay;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceGetAccessibilityTriggerEffect(int user_id, int32_t* trigger_effect) {
 (void)user_id;
 (void)trigger_effect;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceGetAccessibilityVibration(int user_id, int32_t* vibration) {
 (void)user_id;
 (void)vibration;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceGetAccessibilityZoomEnabled(int user_id, int32_t* zoom_enabled) {
 (void)user_id;
 (void)zoom_enabled;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceGetAgeLevel(int user_id, uint32_t* age_level) {
 (void)user_id;
 (void)age_level;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceGetEvent(SceUserServiceEvent* event) {
 (void)event;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceGetGamePresets(int user_id, UserServiceGamePresets* presets) {
 (void)user_id;
 (void)presets;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceGetInitialUser(int* user_id) {
 if (user_id == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *user_id = USER_SERVICE_INITIAL_USER_ID;
 return USER_SERVICE_OK;
}

int sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* user_id_list) {
 if (user_id_list == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 user_id_list->user_id[0] = USER_SERVICE_INITIAL_USER_ID;
 user_id_list->user_id[1] = USER_SERVICE_USER_ID_INVALID;
 user_id_list->user_id[2] = USER_SERVICE_USER_ID_INVALID;
 user_id_list->user_id[3] = USER_SERVICE_USER_ID_INVALID;
 return USER_SERVICE_OK;
}

int sceUserServiceGetUserName(int user_id, char* name, size_t size) {
 (void)user_id;
 (void)name;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceGetUserNumber(int user_id, int32_t* number) {
 (void)user_id;
 (void)number;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int sceUserServiceInitialize(const void* params) {
 (void)params;
 return USER_SERVICE_OK;
}

int sceUserServiceInitialize2(void) {
 return USER_SERVICE_OK;
}

int sceUserServiceTerminate(void) {
 return USER_SERVICE_OK;
}

}

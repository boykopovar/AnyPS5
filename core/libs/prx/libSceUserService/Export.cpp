#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <cstring>
#include "prx/libSceUserService/UserService.hpp"

extern "C" {

int APS5_VABI sceUserServiceGetAccessibilityChatTranscription(int user_id, int32_t* chat_transcription) {
 if (chat_transcription == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *chat_transcription = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityPressAndHoldDelay(int user_id, int32_t* press_and_hold_delay) {
 if (press_and_hold_delay == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *press_and_hold_delay = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityTriggerEffect(int user_id, int32_t* trigger_effect) {
 if (trigger_effect == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *trigger_effect = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityVibration(int user_id, int32_t* vibration) {
 if (vibration == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *vibration = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityZoomEnabled(int user_id, int32_t* zoom_enabled) {
 if (zoom_enabled == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *zoom_enabled = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAccessibilityZoomFollowFocus(int user_id, int32_t* zoom_follow_focus) {
 if (zoom_follow_focus == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *zoom_follow_focus = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetAgeLevel(int user_id, uint32_t* age_level) {
 if (age_level == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *age_level = 0;
 return USER_SERVICE_OK;
}

// The initial user is reported as logging in once; afterwards there are no user events.
int APS5_VABI sceUserServiceGetEvent(SceUserServiceEvent* event) {
 if (event == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 static std::atomic<bool> loginReported{false};
 if (loginReported.exchange(true)) {
  return USER_SERVICE_ERROR_NO_EVENT;
 }
 constexpr std::uint32_t EventTypeLogin = 0;
 event->event_type = EventTypeLogin;
 event->user_id = USER_SERVICE_INITIAL_USER_ID;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetGamePresets(int user_id, UserServiceGamePresets* presets) {
 // System-wide game presets (difficulty, view inversion, subtitles, audio language) of the user's
 // profile. A fresh console has none set: every field is 0, "not specified", and the title falls
 // back to its own defaults. this_size is the caller's.
 if (presets == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 presets->difficulty = 0;
 presets->priority = 0;
 presets->invert_vertical_view_for_1st_person_view = 0;
 presets->invert_horizontal_view_for_1st_person_view = 0;
 presets->invert_vertical_view_for_3rd_person_view = 0;
 presets->invert_horizontal_view_for_3rd_person_view = 0;
 presets->display_sub_titles = 0;
 presets->audio_language = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetInitialUser(int* user_id) {
 if (user_id == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *user_id = USER_SERVICE_INITIAL_USER_ID;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* user_id_list) {
 if (user_id_list == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 user_id_list->user_id[0] = USER_SERVICE_INITIAL_USER_ID;
 user_id_list->user_id[1] = USER_SERVICE_USER_ID_INVALID;
 user_id_list->user_id[2] = USER_SERVICE_USER_ID_INVALID;
 user_id_list->user_id[3] = USER_SERVICE_USER_ID_INVALID;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserName(int user_id, char* name, size_t size) {
 if (name == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 if (size < USER_SERVICE_MAX_USER_NAME_LENGTH + 1) {
  return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
 }
 std::strncpy(name, USER_SERVICE_INITIAL_USER_NAME, size);
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserNumber(int user_id, int32_t* number) {
 if (number == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *number = 1;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceInitialize(const void* params) {
 (void)params;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceInitialize2(void) {
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceTerminate(void) {
 return USER_SERVICE_OK;
}

// No PSN account exists, so the platform privacy setting reports the feature as not permitted.
int APS5_VABI sceUserServiceGetPlatformPrivacyWs1(int32_t user_id, int32_t* value) {
    (void)user_id;
    if (!value) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    *value = 0;
    return 0;
}

int APS5_VABI sceUserServiceGetForegroundUser(int* user_id) {
 if (user_id == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *user_id = USER_SERVICE_INITIAL_USER_ID;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetRegisteredUserIdList(UserServiceRegisteredUserIdList* user_id_list) {
 if (user_id_list == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 user_id_list->user_id[0] = USER_SERVICE_INITIAL_USER_ID;
 for (int i = 1; i < 16; ++i) {
  user_id_list->user_id[i] = USER_SERVICE_USER_ID_INVALID;
 }
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserColor(int user_id, int* color) {
 if (color == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *color = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetNpAccountId(int user_id, uint64_t* account_id) {
 if (account_id == nullptr || user_id != USER_SERVICE_INITIAL_USER_ID) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *account_id = 0;
 return USER_SERVICE_OK;
}

}

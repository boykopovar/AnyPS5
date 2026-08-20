#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceUserServiceGetAccessibilityChatTranscription(int user_id, int32_t* chat_transcription) {
 (void)user_id;
 (void)chat_transcription;
 return 0;
}

int sceUserServiceGetAccessibilityPressAndHoldDelay(int user_id, int32_t* press_and_hold_delay) {
 (void)user_id;
 (void)press_and_hold_delay;
 return 0;
}

int sceUserServiceGetAccessibilityTriggerEffect(int user_id, int32_t* trigger_effect) {
 (void)user_id;
 (void)trigger_effect;
 return 0;
}

int sceUserServiceGetAccessibilityVibration(int user_id, int32_t* vibration) {
 (void)user_id;
 (void)vibration;
 return 0;
}

int sceUserServiceGetAccessibilityZoomEnabled(int user_id, int32_t* zoom_enabled) {
 (void)user_id;
 (void)zoom_enabled;
 return 0;
}

int sceUserServiceGetAgeLevel(int user_id, uint32_t* age_level) {
 (void)user_id;
 (void)age_level;
 return 0;
}

int sceUserServiceGetEvent(SceUserServiceEvent* event) {
 (void)event;
 return 0;
}

int sceUserServiceGetGamePresets(int user_id, UserServiceGamePresets* presets) {
 (void)user_id;
 (void)presets;
 return 0;
}

int sceUserServiceGetInitialUser(int* user_id) {
 (void)user_id;
 return 0;
}

int sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* user_id_list) {
 (void)user_id_list;
 return 0;
}

int sceUserServiceGetUserName(int user_id, char* name, size_t size) {
 (void)user_id;
 (void)name;
 (void)size;
 return 0;
}

int sceUserServiceGetUserNumber(int user_id, int32_t* number) {
 (void)user_id;
 (void)number;
 return 0;
}

int sceUserServiceInitialize(const void* params) {
 (void)params;
 return 0;
}

int sceUserServiceInitialize2(void) {
 return 0;
}

}

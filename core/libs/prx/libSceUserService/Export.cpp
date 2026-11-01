#include "prx/libSceSystemService/include/SystemService.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <array>
#include <cstring>
#include <deque>
#include <mutex>
#include "prx/libSceUserService/UserService.hpp"

namespace {

std::atomic<bool> initialized{false};
std::mutex userMutex;

struct UserEvent {
    std::uint32_t eventType;
    int userId;
};

std::deque<UserEvent> eventQueue;
std::array<bool, USER_SERVICE_MAX_USERS> userConnected{true, false, false, false};

int UserIndex(int userId) {
    if (userId >= USER_SERVICE_INITIAL_USER_ID && userId < USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS) {
        return userId - USER_SERVICE_INITIAL_USER_ID;
    }
    return -1;
}

bool IsUserLoggedIn(int userId) {
 const int index = UserIndex(userId);
 if (index < 0) {
  return false;
 }
 std::lock_guard lock(userMutex);
 return userConnected[index];
}

int ReportAccessibilitySetting(int user_id, int32_t* setting, int32_t value) {
 if (!IsUserLoggedIn(user_id)) {
  return USER_SERVICE_ERROR_NOT_LOGGED_IN;
 }
 if (setting == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *setting = value;
 return USER_SERVICE_OK;
}

void PopulateInitialLoginEvents() {
 std::lock_guard lock(userMutex);
 eventQueue.clear();
 constexpr std::uint32_t EventTypeLogin = 0;
 if (userConnected[0]) {
  eventQueue.push_back({EventTypeLogin, USER_SERVICE_INITIAL_USER_ID});
 }
 for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
  if (userConnected[i]) {
   eventQueue.push_back({EventTypeLogin, USER_SERVICE_INITIAL_USER_ID + i});
  }
 }
}


// Per-user keyboard settings. Nothing in AnyPS5 stores a keyboard type or input variant, so every known user reports
// the keyboard type of the configured system language and 0 for the input variants (see
// docs/dev/debt/libSceUserService.md).
static int32_t KeyboardTypeForSystemLanguage() {
 constexpr int param_id_lang = 1;
 constexpr int32_t fallback = 20;
 constexpr int32_t keyboard_types[] = {37, 20, 26, 21, 17, 28, 29, 33, 34, 38, 39, 40, 23, 35, 16,
                                       30, 31, 32, 19, 36, 22, 41, 25, 44, 48, 43, 47, 42, 46, 45};
 int language = -1;
 if (sceSystemServiceParamGetInt(param_id_lang, &language) != 0 || language < 0 ||
     language >= static_cast<int>(sizeof(keyboard_types) / sizeof(keyboard_types[0]))) {
  return fallback;
 }
 return keyboard_types[language];
}

static int32_t NoInputVariant() {
 return 0;
}

static int ReportKeyboardSetting(int user_id, int32_t* type, int32_t (*value)()) {
 if (type == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 if (UserIndex(user_id) < 0) {
  return USER_SERVICE_ERROR_UNKNOWN_USER;
 }
 *type = value();
 return USER_SERVICE_OK;
}
}

extern "C" {

void UserServiceSetUserConnected_nid_postfix(int userIndex, bool connected) {
    if (userIndex < 0 || userIndex >= USER_SERVICE_MAX_USERS) return;
    std::lock_guard lock(userMutex);
    if (userConnected[userIndex] != connected) {
        userConnected[userIndex] = connected;
        if (initialized.load()) {
            constexpr std::uint32_t EventTypeLogin = 0;
            constexpr std::uint32_t EventTypeLogout = 1;
            const std::uint32_t eventType = connected ? EventTypeLogin : EventTypeLogout;
            const int userId = USER_SERVICE_INITIAL_USER_ID + userIndex;
            bool collapsed = false;
            for (auto it = eventQueue.begin(); it != eventQueue.end(); ++it) {
                if (it->userId == userId) {
                    if (it->eventType != eventType) {
                        eventQueue.erase(it);
                    }
                    collapsed = true;
                    break;
                }
            }
            if (!collapsed) {
                eventQueue.push_back({eventType, userId});
            }
        }
    }
}

int APS5_VABI sceUserServiceGetAccessibilityChatTranscription(int user_id, int32_t* chat_transcription) {
 return ReportAccessibilitySetting(user_id, chat_transcription, 0);
}

int APS5_VABI sceUserServiceGetAccessibilityKeyremapEnable(std::int32_t userId, std::int32_t* enabled) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceUserServiceGetAccessibilityPressAndHoldDelay(int user_id, int32_t* press_and_hold_delay) {
 return ReportAccessibilitySetting(user_id, press_and_hold_delay, 0);
}

int APS5_VABI sceUserServiceGetAccessibilityTriggerEffect(int user_id, int32_t* trigger_effect) {
 return ReportAccessibilitySetting(user_id, trigger_effect, 1);
}

int APS5_VABI sceUserServiceGetAccessibilityVibration(int user_id, int32_t* vibration) {
 return ReportAccessibilitySetting(user_id, vibration, 1);
}

int APS5_VABI sceUserServiceGetAccessibilityZoom(std::int32_t userId, std::int32_t* zoom) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceUserServiceGetAccessibilityZoomEnabled(int user_id, int32_t* zoom_enabled) {
 return ReportAccessibilitySetting(user_id, zoom_enabled, 0);
}

int APS5_VABI sceUserServiceGetAccessibilityZoomFollowFocus(int user_id, int32_t* zoom_follow_focus) {
 return ReportAccessibilitySetting(user_id, zoom_follow_focus, 1);
}

int APS5_VABI sceUserServiceGetAgeLevel(int user_id, uint32_t* age_level) {
 if (!IsUserLoggedIn(user_id)) {
  return USER_SERVICE_ERROR_NOT_LOGGED_IN;
 }
 if (age_level == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 constexpr std::uint32_t Unrestricted = 0xFFFFFFFFu;
 *age_level = Unrestricted;
 return USER_SERVICE_OK;
}

// The initial user is reported as logging in once per initialization; afterwards there are no user events.
int APS5_VABI sceUserServiceGetEvent(SceUserServiceEvent* event) {
 if (!initialized.load()) {
  return USER_SERVICE_ERROR_NOT_INITIALIZED;
 }
 if (event == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 std::lock_guard lock(userMutex);
 if (eventQueue.empty()) {
  return USER_SERVICE_ERROR_NO_EVENT;
 }
 const auto item = eventQueue.front();
 eventQueue.pop_front();
 event->event_type = item.eventType;
 event->user_id = item.userId;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetGamePresets(int user_id, UserServiceGamePresets* presets) {
 // System-wide game presets (difficulty, view inversion, subtitles, audio language) of the user's
 // profile. A fresh console has none set: every field is 0, "not specified", and the title falls
 // back to its own defaults. this_size is the caller's.
 const int index = UserIndex(user_id);
 if (presets == nullptr || index < 0) {
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
 if (!initialized.load()) {
  return USER_SERVICE_ERROR_NOT_INITIALIZED;
 }
 if (user_id_list == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 std::lock_guard lock(userMutex);
 for (int i = 0; i < 4; ++i) {
  if (i < USER_SERVICE_MAX_USERS && userConnected[i]) {
   user_id_list->user_id[i] = USER_SERVICE_INITIAL_USER_ID + i;
  } else {
   user_id_list->user_id[i] = USER_SERVICE_USER_ID_INVALID;
  }
 }
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserName(int user_id, char* name, size_t size) {
 if (name == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 const int index = UserIndex(user_id);
 if (index < 0) {
  return USER_SERVICE_ERROR_UNKNOWN_USER;
 }
 if (size < USER_SERVICE_MAX_USER_NAME_LENGTH + 1) {
  std::memset(name, 0, size);
  return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
 }
 std::strncpy(name, USER_SERVICE_USER_NAMES[index], size);
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserNumber(int user_id, int32_t* number) {
 const int index = UserIndex(user_id);
 if (number == nullptr || index < 0) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *number = index + 1;
 return USER_SERVICE_OK;
}

static bool ValidThreadPriority(int priority) {
 return priority >= USER_SERVICE_THREAD_PRIORITY_MIN && priority <= USER_SERVICE_THREAD_PRIORITY_MAX;
}

static int MarkInitialized() {
 if (initialized.exchange(true)) {
  return USER_SERVICE_ERROR_ALREADY_INITIALIZED;
 }
 PopulateInitialLoginEvents();
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceInitialize(const UserServiceInitParam* params) {
 if (params != nullptr) {
  if (!ValidThreadPriority(params->priority)) {
   return USER_SERVICE_ERROR_INVALID_ARGUMENT;
  }
 }
 return MarkInitialized();
}

int APS5_VABI sceUserServiceInitialize2(std::int32_t priority, KernelCpumask affinityMask) {
 if (!ValidThreadPriority(priority)) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 return MarkInitialized();
}

int APS5_VABI sceUserServiceTerminate(void) {
 if (!initialized.exchange(false)) {
  return USER_SERVICE_ERROR_NOT_INITIALIZED;
 }
 std::lock_guard lock(userMutex);
 eventQueue.clear();
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetPlatformPrivacyWs1(int32_t user_id, int32_t* value) {
    if (!IsUserLoggedIn(user_id)) {
        return USER_SERVICE_ERROR_NOT_LOGGED_IN;
    }
    if (value == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    *value = 0;
    return USER_SERVICE_OK;
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
 std::lock_guard lock(userMutex);
 for (int i = 0; i < 16; ++i) {
  if (i < USER_SERVICE_MAX_USERS && userConnected[i]) {
   user_id_list->user_id[i] = USER_SERVICE_INITIAL_USER_ID + i;
  } else {
   user_id_list->user_id[i] = USER_SERVICE_USER_ID_INVALID;
  }
 }
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetUserColor(int user_id, int* color) {
 const int index = UserIndex(user_id);
 if (color == nullptr || index < 0) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *color = index;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetKeyboardType(int user_id, int32_t* type) {
 return ReportKeyboardSetting(user_id, type, KeyboardTypeForSystemLanguage);
}

int APS5_VABI sceUserServiceGetJapaneseInputType(int user_id, int32_t* type) {
 return ReportKeyboardSetting(user_id, type, NoInputVariant);
}

int APS5_VABI sceUserServiceGetTraditionalChineseInputType(int user_id, int32_t* type) {
 return ReportKeyboardSetting(user_id, type, NoInputVariant);
}

int APS5_VABI sceUserServiceGetNpAccountId(int user_id, uint64_t* account_id) {
 if (!IsUserLoggedIn(user_id)) {
  return USER_SERVICE_ERROR_UNKNOWN_USER;
 }
 if (account_id == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 *account_id = 0;
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceSetGlsCameraContrast(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalDvdRegion(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetChatStatus(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSharePlayFramerateHost(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPasscode(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAppearOfflineSetting(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsAccessTokenUstream(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBcTags(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpCountryCode(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetJapaneseInputType(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBroadcastersComment(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraTransparency(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsHrtfEnabled(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetCurrentUserGroupIndex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcThursdayHoursEnd(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcFridayHoursStart(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountIdTtl(int userId, std::uint64_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetRegisteredHomeUserIdList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVolumeForGenericUSB(int userId, std::uint16_t deviceType, std::uint16_t deviceIndex, int value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetControlCenterFunctionControlSettings(int userId, void* settings) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAudioFocus(int userId, void* audioFocus) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetFriendFlag(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraTransparency(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetUserStatus(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTraditionalChineseInputType(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2MuteAssign(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsCronosEnabled(std::int32_t userId, bool* enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHoldAudioOutDevice(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistWeakenAdjustment(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetDiscPlayerFlag(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSharePlayFramerateHost(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetCaptureStatus2(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetLightBarBaseBrightness(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraContrast(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcWednesdayDuration(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSharePlayResolutionHost(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalBdAge(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalGameAgeLevel(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings2(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPsnPasswordForDebug(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcAdditionalTime(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLfpsSortOrder(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceLogin(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpLoginId(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBroadcastersCommentColor(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalBrowser(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPadSpeakerVolume(int userId, std::uint16_t deviceType, std::uint16_t deviceIndex, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsHintFlag(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIsFakePremium(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAccessibilityKeyremapDataAston(int userId, void* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetKeyRepeatSpeed(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetUserGroupName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalDvdRegion(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraDepthLevel(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalContentControl(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentLanguage(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVolumeForController(int userId, std::uint16_t deviceType, std::uint16_t deviceIndex, int value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsAccessTokenNiconicoLive(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsOverlayPosition(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAppSortOrder(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraBgFilter(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSaturdayDuration(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAccountRemarks(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGazeCalibData(int userId, void* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsAccessTokenNiconicoLive(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2GazeTarget(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpAccountUpgradeFlag(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsMessageFilterLevel(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetImePointerMode(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalDvd(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSparkEqProfile(int userId, std::uint32_t profileIndex, const void* profile) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcTuesdayHoursStart(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceRecognitionTutorialState(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetFriendFlag(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventCalendarType(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSaturdayHoursEnd(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountIdParam(int userId, std::uint8_t* value, int size, std::uint64_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistUsePreset(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetUserGroupName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAutoLoginEnabled(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIPDLeft(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHintShowFlag(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2CinematicAudioRotation(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetLoginFlag(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcThursdayDuration(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSundayHoursStart(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceRecognitionLastUsedOsk(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsFacebookEnabled(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetShareStatus(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentEnableFeedback(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsKratosUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetCreatedVersion(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSharePlayFlags(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistUsePreset(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyChatTranscriptionLanguageType(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetA11yControllerADeviceUniqueIds(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerLedBrightness(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetKeyRepeatStartingTime(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerAlreadyNoticed(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsAccessTokenUstream(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetProControllerUniqueId3(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHrtf(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsTtsFlags(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpAge(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsFloatingMessage(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPlayTogetherFlags(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2HideCameraImage(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsAccessTokenTwitch(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetCaptureStatus(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpSubAccount(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyChatTranscriptionFlag(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsMuteEnabled(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBcTitle(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2Vibration(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality2(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIsQuickSignup(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings_2(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHomeDirectory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSundayDuration(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIPDLeft(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetKratosPrimaryUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceLogout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpDateOfBirth(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcWednesdayHoursStart(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSparkEqParam(int userId, std::uint32_t* parameters) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventSortEvent(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality3(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings_1(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcMondayHoursEnd(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsFakePlus(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcTuesdayHoursEnd(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartyMuteList(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpNpId(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountIdClockType(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerBtAddress(int userId, std::uint64_t* address) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSparkEqIndex(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPsnPasswordForDebug(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalGameWhiteList(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCommunityId(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceUpdateCronosCountryList(const char* countryList) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGazeCalibData(int userId, void* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpAuthErrorFlag(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistUseAudiogramData(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAccessibilityKeyremapEnable(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcFlag(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcFridayDuration(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsStreamingMode(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcFridayHoursEnd(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerUniqueId1(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentStatusBitFactory(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTeamShowAboutTeam(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventSortEvent(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLfpsSortOrder(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistMakeVoicesClearer(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentEnableByButton(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountIdClockType(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceTerminateForShellCore(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalMorpheus(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountId(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetCaptureStatus(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAgeVerified(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraBgFilter(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsTtsVolume(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraSize(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsAnonymousUserId(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraReflection(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmuZoom(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBcTitle(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcFridayHoursEnd(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetLoginFlag(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistAdjustmentLevelLeft(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetFaceRecognitionDeleteCount(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTriggerEffectStrength(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsTwitterEnabled(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSaturdayHoursStart(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHintShowFlag3(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentSessionTimeout(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalContentControl(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraEffect(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcTuesdayHoursEnd(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartyMuteListA(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraEliminationLevel(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastersCommentColor(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalBrowser(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistSeparateLR(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsUserStorageAccountBound(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2Brightness(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraChromaKeyLevel(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerUniqueId2(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBroadcastChannel(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraEdgeLevel(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraPosition(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerAName(int userId, void* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcWednesdayHoursEnd(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetMonoOutput(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyChatTranscriptionVoiceType(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTopMenuTutorialFlag(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIPDRight(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSaturdayHoursEnd(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetImeLastUnit(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpNpId(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTriggerEffectStrength(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsRecievedMessageHidden(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentSessionTimeout(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLanguageFilter(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2Vibration(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsLoggedIn(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcMondayHoursStart(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAppSortOrder(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetMouseHandType(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsQuickSignup(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsKratosPrimaryUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentEnableWup(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraDepthLevel(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmuBrightness(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsHintFlag(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsLoggedInWithoutLock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetShareStatus3(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsTtsVolume(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetMicLevel(void* level, void* settings) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetImeLastUnit(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcMode(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcThursdayHoursEnd(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSaveDataAutoUpload(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAccessibilityKeyremapDataAston(int userId, void* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVolumeForSidetone(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetProControllerUniqueId2(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetFriendCustomListLastFocus(int userId, std::uint64_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2GazeTarget(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetKeyRepeatSpeed(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalDvd(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetImePointerMode(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVolumeForController(int userId, std::uint16_t deviceType, std::uint16_t deviceIndex, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastChannel(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountIdParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHintShowFlag2(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistPresetType(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings_3(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsAnonymousUserId(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetA11yControllerBDeviceUniqueIds(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentEnableWup(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpDateOfBirth(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetKeyboardType(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalGame(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistSeparateLR(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentTtsVolume(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcMondayDuration(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpSubAccount(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetShareStatus2(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCamCrop(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceRecognitionLastUsedOsk(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountId(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceRecognitionTutorialState(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality5(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistUseAudiogramData(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcFlag(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIPDRight(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcPlayTimeLastUpdated(int userId, std::uint64_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcTuesdayHoursStart(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationBehavior(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPadProfileDataHuntList(int userId, void* profiles, std::uint32_t count, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistAudiogramData(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHintShowFlag(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTopMenuLimitItem(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSundayHoursEnd(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalBdAge(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSaveDataSort(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHrtf(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAppearOfflineSetting(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentLanguage(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIsRemotePlayAllowed(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsStreamingMode(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetChatStatus(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentStatusBit(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpCountryCode(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalGameWhiteList(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2ScreenSize(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTriggerEffectEnabled(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerInitialSetupProgress(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVolumeForGenericUSB(int userId, std::uint16_t deviceType, std::uint16_t deviceIndex, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetControlCenterFunctionControlSettings(int userId, void* settings) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsInitSpectating(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsMuteEnabled(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpLanguageCode2(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetFriendCustomListLastFocus(int userId, std::uint64_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyMuteList(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAccessibilityKeyremapData(int userId, void* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcFridayHoursStart(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcMondayHoursEnd(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetControlCenterStatus(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSparkEqProfile(int userId, std::uint32_t profileIndex, void* profile) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsTtsPitch(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBroadcastService(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAutoLoginEnabled(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSaturdayHoursStart(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings_1(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2MuteAssign(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPasscode(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetMousePointerSpeed(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastUiLayout(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSharePlayFlags(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIsFakePlus(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAccountRemarks(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAccessibilityZoom(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountIdTtl(int userId, std::uint64_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSaveDataSort(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPlayTogetherFlags(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistAdjustmentLevelRight(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsTtsSpeed(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpEnv(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraSize(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetDiscPlayerFlag(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2PlayAreaShape(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsCameraHidden(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetUserStatus(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventFilterTeamEvent(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPsCloudFeedbackTime(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetUserGroupIndex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyMuteListA(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpLanguageCode2(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetShareStatus2(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetForegroundUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsAccessTokenTwitch(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpOfflineAccountAdult(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGamePresets(int userId, void* presets) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetImeInputType(int userId, std::uint64_t language, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVolumeForMorpheusSidetone(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality4(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistPresetType(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistWeakenAdjustment(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcWednesdayDuration(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsGuestUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPadProfileDataHuntList(int userId, void* profiles, std::uint32_t count, std::uint32_t flags) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAccessibilityKeyremapData(int userId, void* data) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetCaptureStatus2(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentTtsSpeed(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcWednesdayHoursEnd(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAgeVerified(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraEdgeLevel(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartyChatTranscriptionLanguageType(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentTtsVolume(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcTuesdayDuration(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetUserWithHearingAssist(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpOnlineId(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHintShowFlag3(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings_3(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsFacebookEnabled(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpEnv(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraBrightness(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceDestroyUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTopMenuNotificationFlag(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetCreatedVersion(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraChromaKeyLevel(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpOfflineAccountId(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetShareStatus3(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetControlCenterStatus(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetMicLevel(void* level, void* settings) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTeamShowAboutTeam(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventVsh(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerBName(int userId, void* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpAge(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPlayingBigAppTitleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHintShowFlag2(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetCaptureStatus3(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSparkEqIndex(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcMondayDuration(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventSortTitle(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpOnlineId(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetA11yControllerBName(int userId, void* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSaveDataAutoSync(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetDefaultNewUserGroupName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2Brightness(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHoldAudioOutDevice(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVibrationStrength(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceUnregisterEventCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTopMenuNotificationFlag(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSundayDuration(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistAudiogramData(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSaveDataAutoSync(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcFridayDuration(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsTtsSpeed(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartyChatTranscriptionFlag(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSaturdayDuration(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartySettingFlags(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetImeInputSettings(int userId, std::uint64_t language, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsRecDisabled(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetA11yControllerAName(int userId, void* name) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventFilterTeamEvent(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerBDeviceUniqueIds(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraEliminationLevel(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVibrationStrength(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpAccountUpgradeFlag(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetDeletedUserInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSecureHomeDirectory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistAdjustmentLevelRight(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPlatformPrivacyWs1Internal(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsRecDisabled(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraBrightness(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpLanguageCode(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcThursdayHoursStart(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGazeActivate(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPlatformPrivacyWs1(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVolumeForSidetone(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSaveDataTutorialFlag(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcPlayTime(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastService(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsFakePremium(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality5(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSaveDataAutoUpload(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSaveDataTutorialFlag(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality4(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpLanguageCode(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcThursdayDuration(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetFaceRecognitionDeleteCount(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentEnable(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventCalendarType(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetProControllerAlreadyNoticed(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsTtsFlags(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastersComment(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcTzOffset(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcPlayTime(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentStatusBit(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings2(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventUiFlag(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBroadcastUiLayout(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVibrationEnabled(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcAdditionalTime(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsSharePlayClientUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetMonoOutput(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpLoginId(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetShareStatus(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsRecievedMessageHidden(int userId, bool value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality2(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2GazeActivate(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraPosition(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsMessageFilterLevel(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetUserGroupNameList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentEnableFeedback(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGazeActivate(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsTwitterEnabled(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTopMenuLimitItem(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpAuthErrorFlag(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsCameraHidden(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetMousePointerSpeed(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetFaceRecognitionRegisterCount(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2PlayAreaShape(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetUserName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerADeviceUniqueIds(int userId, char* value, std::size_t size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsFloatingMessage(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceInitializeForShellCore(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBcTags(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcMode(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetImeInputType(int userId, std::uint64_t language, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerUniqueId3(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2GazeActivate(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetCaptureStatus3(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraReflection(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings_2(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetFaceRecognitionRegisterCount(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetKeyRepeatStartingTime(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsRemotePlayAllowed(int userId, bool* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcMondayHoursStart(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality3(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPsCloudFeedbackTime(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentTtsSpeed(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentEnable(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcWednesdayHoursStart(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTopMenuTutorialFlag(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsPersonalizedHrtfEnabled(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPadSpeakerVolume(int userId, std::uint16_t deviceType, std::uint16_t deviceIndex, int value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAccessibilityKeyremapEnableAston(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2ScreenSize(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsInitSpectating(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetLastLoginOrder(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsTtsPitch(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetMouseHandType(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventUiFlag(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetImeInputSettings(int userId, std::uint64_t language, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetProControllerUniqueId1(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcPlayTimeLastUpdated(int userId, std::uint64_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmuBrightness(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceRegisterEventCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalMorpheus(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcTuesdayDuration(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsOverlayPosition(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventSortTitle(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmuZoom(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVolumeForMorpheusSidetone(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLanguageFilter(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVibrationEnabled(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSharePlayResolutionHost(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSundayHoursStart(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpOfflineAccountAdult(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTriggerEffectEnabled(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistAdjustmentLevelLeft(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetLightBarBaseBrightness(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentStatusBitFactory(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistMakeVoicesClearer(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCommunityId(int userId, const char* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCamCrop(int userId, void* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcTzOffset(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalGame(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcThursdayHoursStart(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIsHrtfEnabled(int userId, std::int32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalGameAgeLevel(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationBehavior(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetUserGroupNum(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartySettingFlags(int userId, std::uint32_t value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraEffect(int userId, std::uint32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSundayHoursEnd(int userId, std::int32_t* value) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsSpartacusEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

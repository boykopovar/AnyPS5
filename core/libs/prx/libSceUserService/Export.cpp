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
#include <algorithm>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "prx/libSceUserService/UserService.hpp"

namespace {

std::atomic<bool> initialized{false};
std::mutex userMutex;

struct UserEvent {
    std::uint32_t eventType;
    int userId;
};

std::deque<UserEvent> eventQueue;
std::unordered_map<std::uint64_t, std::uint32_t> userIntegerSettings;
std::unordered_map<std::uint64_t, std::string> userStringSettings;
std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> userDataSettings;
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


template <typename T>
std::uint32_t EncodeUserIntegerSetting(T value) {
    if constexpr (std::is_same_v<T, bool>) {
        return value ? 1u : 0u;
    } else {
        static_assert(sizeof(T) == sizeof(std::uint32_t));
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }
}

template <typename T>
T DecodeUserIntegerSetting(std::uint32_t bits) {
    if constexpr (std::is_same_v<T, bool>) {
        return bits != 0;
    } else {
        static_assert(sizeof(T) == sizeof(bits));
        T value;
        std::memcpy(&value, &bits, sizeof(bits));
        return value;
    }
}

template <typename T>
int ReadUserIntegerSetting(int userId, T* output, std::uint32_t key, std::int32_t defaultValue) {
    if (!IsUserLoggedIn(userId)) {
        return USER_SERVICE_ERROR_NOT_LOGGED_IN;
    }
    if (output == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    const int index = UserIndex(userId);
    const std::uint64_t storageKey = (static_cast<std::uint64_t>(index) << 32) | key;
    std::lock_guard lock(userMutex);
    const auto item = userIntegerSettings.find(storageKey);
    const std::uint32_t bits = item == userIntegerSettings.end() ? static_cast<std::uint32_t>(defaultValue) : item->second;
    *output = DecodeUserIntegerSetting<T>(bits);
    return USER_SERVICE_OK;
}

template <typename T>
int WriteUserIntegerSetting(int userId, T value, std::uint32_t key) {
    if (!IsUserLoggedIn(userId)) {
        return USER_SERVICE_ERROR_NOT_LOGGED_IN;
    }
    const int index = UserIndex(userId);
    const std::uint64_t storageKey = (static_cast<std::uint64_t>(index) << 32) | key;
    std::lock_guard lock(userMutex);
    userIntegerSettings[storageKey] = EncodeUserIntegerSetting(value);
    return USER_SERVICE_OK;
}

int ReadUserStringSetting(int userId, char* output, std::size_t size, std::uint32_t key, std::size_t storageSize) {
    if (!IsUserLoggedIn(userId)) {
        return USER_SERVICE_ERROR_NOT_LOGGED_IN;
    }
    if (output == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (size == 0) {
        return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    }
    const int index = UserIndex(userId);
    const std::uint64_t storageKey = (static_cast<std::uint64_t>(index) << 32) | key;
    std::lock_guard lock(userMutex);
    const auto item = userStringSettings.find(storageKey);
    const std::string value = item == userStringSettings.end() ? std::string{} : item->second;
    if (value.size() + 1 > storageSize || value.size() + 1 > size) {
        output[0] = '\0';
        return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    }
    std::memcpy(output, value.data(), value.size());
    output[value.size()] = '\0';
    return USER_SERVICE_OK;
}

int WriteUserStringSetting(int userId, const char* value, std::uint32_t key, std::size_t storageSize) {
    if (!IsUserLoggedIn(userId)) {
        return USER_SERVICE_ERROR_NOT_LOGGED_IN;
    }
    if (value == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    const std::size_t valueSize = std::strlen(value);
    if (valueSize + 1 > storageSize) {
        return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    }
    const int index = UserIndex(userId);
    const std::uint64_t storageKey = (static_cast<std::uint64_t>(index) << 32) | key;
    std::lock_guard lock(userMutex);
    userStringSettings[storageKey] = value;
    return USER_SERVICE_OK;
}

int ReadUserDataSetting(int userId, void* output, std::size_t size, std::uint32_t key, std::size_t storageSize) {
    if (!IsUserLoggedIn(userId)) {
        return USER_SERVICE_ERROR_NOT_LOGGED_IN;
    }
    if (output == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (size < storageSize) {
        return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    }
    const int index = UserIndex(userId);
    const std::uint64_t storageKey = (static_cast<std::uint64_t>(index) << 32) | key;
    std::lock_guard lock(userMutex);
    std::memset(output, 0, storageSize);
    const auto item = userDataSettings.find(storageKey);
    if (item != userDataSettings.end()) {
        std::memcpy(output, item->second.data(), std::min(storageSize, item->second.size()));
    }
    return USER_SERVICE_OK;
}

int WriteUserDataSetting(int userId, const void* value, std::size_t size, std::uint32_t key, std::size_t storageSize) {
    if (!IsUserLoggedIn(userId)) {
        return USER_SERVICE_ERROR_NOT_LOGGED_IN;
    }
    if (value == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (size != storageSize) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    const int index = UserIndex(userId);
    const std::uint64_t storageKey = (static_cast<std::uint64_t>(index) << 32) | key;
    const auto* bytes = static_cast<const std::uint8_t*>(value);
    std::lock_guard lock(userMutex);
    userDataSettings[storageKey] = std::vector<std::uint8_t>(bytes, bytes + size);
    return USER_SERVICE_OK;
}

struct UserImeLanguageSettings {
    std::uint64_t language;
    std::uint32_t inputTypeKey;
    std::uint32_t inputSettingsKey;
};

constexpr std::array<UserImeLanguageSettings, 28> userImeLanguageSettings{{
    {0x01000000ull, 0x07808017u, 0x0780803eu},
    {0x08000000ull, 0x0780801au, 0x07808041u},
    {0x00000001ull, 0x07808001u, 0x07808028u},
    {0x00000040ull, 0x07808002u, 0x07808029u},
    {0x00040000ull, 0x07808003u, 0x0780802au},
    {0x00000004ull, 0x07808004u, 0x0780802bu},
    {0x00000800ull, 0x07808005u, 0x0780802cu},
    {0x00000010ull, 0x07808006u, 0x0780802du},
    {0x02000000ull, 0x07808018u, 0x0780803fu},
    {0x00000002ull, 0x07808007u, 0x0780802eu},
    {0x10000000ull, 0x0780801bu, 0x07808042u},
    {0x100000000ull, 0x0780801fu, 0x07808046u},
    {0x20000000ull, 0x0780801cu, 0x07808043u},
    {0x00000020ull, 0x07808008u, 0x0780802fu},
    {0x00002000ull, 0x07808009u, 0x07808030u},
    {0x00004000ull, 0x0780800au, 0x07808031u},
    {0x00000080ull, 0x0780800bu, 0x07808032u},
    {0x00000100ull, 0x0780800cu, 0x07808033u},
    {0x00020000ull, 0x0780800eu, 0x07808035u},
    {0x00000200ull, 0x0780800du, 0x07808034u},
    {0x80000000ull, 0x0780801eu, 0x07808045u},
    {0x00000400ull, 0x0780800fu, 0x07808036u},
    {0x00008000ull, 0x07808013u, 0x0780803au},
    {0x00000008ull, 0x07808010u, 0x07808037u},
    {0x00100000ull, 0x07808011u, 0x07808038u},
    {0x00001000ull, 0x07808012u, 0x07808039u},
    {0x04000000ull, 0x07808019u, 0x07808040u},
    {0x20000000000ull, 0x07808014u, 0x0780803bu},
}};

const UserImeLanguageSettings* FindUserImeLanguageSettings(std::uint64_t language) {
    const auto item = std::find_if(userImeLanguageSettings.begin(), userImeLanguageSettings.end(), [language](const auto& setting) {
        return setting.language == language;
    });
    return item == userImeLanguageSettings.end() ? nullptr : &*item;
}


int ReadKeyboardSetting(int userId, std::int32_t* type, std::uint32_t key, std::int32_t defaultValue) {
 if (type == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 if (UserIndex(userId) < 0) {
  return USER_SERVICE_ERROR_UNKNOWN_USER;
 }
 return ReadUserIntegerSetting(userId, type, key, defaultValue);
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
 return ReadKeyboardSetting(user_id, type, 0x07809004u, KeyboardTypeForSystemLanguage());
}

int APS5_VABI sceUserServiceGetJapaneseInputType(int user_id, int32_t* type) {
 return ReadKeyboardSetting(user_id, type, 0x07809001u, NoInputVariant());
}

int APS5_VABI sceUserServiceGetTraditionalChineseInputType(int user_id, int32_t* type) {
 return ReadKeyboardSetting(user_id, type, 0x07809002u, NoInputVariant());
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
    return WriteUserIntegerSetting(userId, value, 0x0780c021u);
}

int APS5_VABI sceUserServiceSetParentalDvdRegion(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07805c03u);
}

int APS5_VABI sceUserServiceSetChatStatus(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07801800u);
}

int APS5_VABI sceUserServiceSetSharePlayFramerateHost(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x80bu);
}

int APS5_VABI sceUserServiceSetPasscode(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 2, 16);
}

int APS5_VABI sceUserServiceGetAppearOfflineSetting(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsAccessTokenUstream(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780c00bu, 41);
}

int APS5_VABI sceUserServiceSetGlsBcTags(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c026u);
}

int APS5_VABI sceUserServiceSetNpCountryCode(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780b00eu, 3);
}

int APS5_VABI sceUserServiceSetJapaneseInputType(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07809001u);
}

int APS5_VABI sceUserServiceSetGlsBroadcastersComment(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0620u, 841);
}

int APS5_VABI sceUserServiceSetGlsCameraTransparency(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c01bu);
}

int APS5_VABI sceUserServiceGetIsHrtfEnabled(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a011u, 1);
}

int APS5_VABI sceUserServiceGetCurrentUserGroupIndex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcThursdayHoursEnd(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b812u, 1440);
}

int APS5_VABI sceUserServiceGetPbtcFridayHoursStart(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b814u, 0);
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountIdTtl(int userId, std::uint64_t value) {
    return WriteUserDataSetting(userId, &value, sizeof(value), 0x0780f802u, sizeof(value));
}

int APS5_VABI sceUserServiceGetRegisteredHomeUserIdList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVolumeForGenericUSB(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetControlCenterFunctionControlSettings(int userId, void* settings) {
    return WriteUserDataSetting(userId, settings, 64, 0x08aeu, 64);
}

int APS5_VABI sceUserServiceGetAudioFocus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetFriendFlag(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0xau);
}

int APS5_VABI sceUserServiceGetGlsCameraTransparency(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c01bu, -1);
}

int APS5_VABI sceUserServiceGetUserStatus(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07800f00u, 2);
}

int APS5_VABI sceUserServiceSetTraditionalChineseInputType(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07809002u);
}

int APS5_VABI sceUserServiceGetHmd2MuteAssign(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a104u, 1);
}

int APS5_VABI sceUserServiceGetIsCronosEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHoldAudioOutDevice(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a00fu);
}

int APS5_VABI sceUserServiceSetHearingAssistWeakenAdjustment(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806e07u);
}

int APS5_VABI sceUserServiceSetDiscPlayerFlag(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07800a00u);
}

int APS5_VABI sceUserServiceGetSharePlayFramerateHost(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x80bu, 0);
}

int APS5_VABI sceUserServiceGetCaptureStatus2(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806540u, 0);
}

int APS5_VABI sceUserServiceSetLightBarBaseBrightness(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a007u);
}

int APS5_VABI sceUserServiceGetGlsCameraContrast(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c021u, -1);
}

int APS5_VABI sceUserServiceGetPbtcWednesdayDuration(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b80du, -1);
}

int APS5_VABI sceUserServiceGetSharePlayResolutionHost(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x80cu, 0);
}

int APS5_VABI sceUserServiceSetParentalBdAge(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07805c02u);
}

int APS5_VABI sceUserServiceSetParentalGameAgeLevel(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07805c07u);
}

int APS5_VABI sceUserServiceSetNotificationSettings2(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07801300u);
}

int APS5_VABI sceUserServiceGetPsnPasswordForDebug(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780b005u, 31);
}

int APS5_VABI sceUserServiceGetPbtcAdditionalTime(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b804u, 0);
}

int APS5_VABI sceUserServiceSetGlsLfpsSortOrder(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x821u);
}

int APS5_VABI sceUserServiceLogin(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpLoginId(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x07800600u, 65);
}

int APS5_VABI sceUserServiceSetGlsBroadcastersCommentColor(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x621u);
}

int APS5_VABI sceUserServiceSetParentalBrowser(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07805c05u);
}

int APS5_VABI sceUserServiceGetPadSpeakerVolume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsHintFlag(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c022u, 0);
}

int APS5_VABI sceUserServiceSetIsFakePremium(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b040u);
}

int APS5_VABI sceUserServiceGetAccessibilityKeyremapDataAston(int userId, void* data) {
    return ReadUserDataSetting(userId, data, 16, 0x08f2u, 16);
}

int APS5_VABI sceUserServiceSetKeyRepeatSpeed(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07809006u);
}

int APS5_VABI sceUserServiceSetUserGroupName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalDvdRegion(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07805c03u, 0);
}

int APS5_VABI sceUserServiceSetGlsCameraDepthLevel(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c01eu);
}

int APS5_VABI sceUserServiceGetParentalContentControl(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07805c08u, -1);
}

int APS5_VABI sceUserServiceGetVoiceAgentLanguage(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d202u, -1);
}

int APS5_VABI sceUserServiceSetVolumeForController(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsAccessTokenNiconicoLive(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780c00eu, 128);
}

int APS5_VABI sceUserServiceSetGlsOverlayPosition(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c030u);
}

int APS5_VABI sceUserServiceGetAppSortOrder(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0xeu, 0);
}

int APS5_VABI sceUserServiceGetGlsCameraBgFilter(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c018u, -1);
}

int APS5_VABI sceUserServiceSetPbtcSaturdayDuration(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b816u);
}

int APS5_VABI sceUserServiceSetAccountRemarks(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x07804000u, 32);
}

int APS5_VABI sceUserServiceSetGazeCalibData(int userId, void* data) {
    const int result = WriteUserDataSetting(userId, data, 2000, 0x08c7u, 2000);
    if (result != USER_SERVICE_OK) {
        return result;
    }
    return WriteUserDataSetting(userId, static_cast<std::uint8_t*>(data) + 2000, 2000, 0x08c8u, 2000);
}

int APS5_VABI sceUserServiceSetGlsAccessTokenNiconicoLive(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780c00eu, 128);
}

int APS5_VABI sceUserServiceGetHmd2GazeTarget(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a03eu, 2);
}

int APS5_VABI sceUserServiceSetNpAccountUpgradeFlag(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b00bu);
}

int APS5_VABI sceUserServiceGetGlsMessageFilterLevel(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c010u, 0);
}

int APS5_VABI sceUserServiceSetImePointerMode(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07808083u);
}

int APS5_VABI sceUserServiceGetParentalDvd(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07805c04u, 0);
}

int APS5_VABI sceUserServiceSetSparkEqProfile(int userId, std::uint32_t profileIndex, const void* profile) {
    if (profileIndex < 0x80u || profileIndex > 0x82u) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return WriteUserDataSetting(userId, profile, 73, 0x0780a017u + (profileIndex & 3u), 73);
}

int APS5_VABI sceUserServiceGetPbtcTuesdayHoursStart(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b80bu, 0);
}

int APS5_VABI sceUserServiceGetVoiceRecognitionTutorialState(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x12u, 0);
}

int APS5_VABI sceUserServiceGetFriendFlag(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0xau, 0);
}

int APS5_VABI sceUserServiceGetEventCalendarType(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x830u, 0);
}

int APS5_VABI sceUserServiceSetPbtcSaturdayHoursEnd(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b818u);
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountIdParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistUsePreset(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806e01u);
}

int APS5_VABI sceUserServiceGetUserGroupName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAutoLoginEnabled(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x07800400u);
}

int APS5_VABI sceUserServiceGetIPDLeft(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a00du, -1);
}

int APS5_VABI sceUserServiceSetHintShowFlag(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07801900u);
}

int APS5_VABI sceUserServiceGetHmd2CinematicAudioRotation(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a107u, 0);
}

int APS5_VABI sceUserServiceSetLoginFlag(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07800800u);
}

int APS5_VABI sceUserServiceGetPbtcThursdayDuration(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b810u, -1);
}

int APS5_VABI sceUserServiceGetPbtcSundayHoursStart(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b81au, 0);
}

int APS5_VABI sceUserServiceSetVoiceRecognitionLastUsedOsk(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x868u);
}

int APS5_VABI sceUserServiceGetGlsIsFacebookEnabled(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c001u, 0);
}

int APS5_VABI sceUserServiceSetShareStatus(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x700u);
}

int APS5_VABI sceUserServiceGetVoiceAgentEnableFeedback(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d20au, -1);
}

int APS5_VABI sceUserServiceIsKratosUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetCreatedVersion(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07800e00u);
}

int APS5_VABI sceUserServiceGetSharePlayFlags(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07807c03u, 0);
}

int APS5_VABI sceUserServiceGetHearingAssistUsePreset(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806e01u, 0);
}

int APS5_VABI sceUserServiceSetPartyChatTranscriptionLanguageType(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c605u);
}

int APS5_VABI sceUserServiceSetA11yControllerADeviceUniqueIds(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780aa01u, 53);
}

int APS5_VABI sceUserServiceGetA11yControllerLedBrightness(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780aa21u, 0);
}

int APS5_VABI sceUserServiceGetKeyRepeatStartingTime(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07809005u, 16);
}

int APS5_VABI sceUserServiceSetGlsLiveQuality(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c003u);
}

int APS5_VABI sceUserServiceGetProControllerAlreadyNoticed(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a508u, 0);
}

int APS5_VABI sceUserServiceSetGlsAccessTokenUstream(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780c00bu, 41);
}

int APS5_VABI sceUserServiceSetProControllerUniqueId3(int userId, void* value) {
    return WriteUserDataSetting(userId, value, 16, 0x08e3u, 16);
}

int APS5_VABI sceUserServiceSetHrtf(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a010u);
}

int APS5_VABI sceUserServiceSetGlsTtsFlags(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c027u);
}

int APS5_VABI sceUserServiceGetNpAge(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b012u, -1);
}

int APS5_VABI sceUserServiceSetGlsFloatingMessage(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c011u);
}

int APS5_VABI sceUserServiceGetPlayTogetherFlags(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x826u, 0);
}

int APS5_VABI sceUserServiceGetHmd2HideCameraImage(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a10du, 0);
}

int APS5_VABI sceUserServiceGetGlsAccessTokenTwitch(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780c00cu, 41);
}

int APS5_VABI sceUserServiceSetCaptureStatus(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806501u);
}

int APS5_VABI sceUserServiceSetNpSubAccount(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b009u);
}

int APS5_VABI sceUserServiceSetPartyChatTranscriptionFlag(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c604u);
}

int APS5_VABI sceUserServiceSetGlsIsMuteEnabled(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c005u);
}

int APS5_VABI sceUserServiceSetGlsBcTitle(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780c043u, 601);
}

int APS5_VABI sceUserServiceSetHmd2Vibration(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a105u);
}

int APS5_VABI sceUserServiceGetGlsLiveQuality2(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c012u, 0);
}

int APS5_VABI sceUserServiceSetIsQuickSignup(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x103u);
}

int APS5_VABI sceUserServiceSetNotificationSettings_2(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07801500u);
}

int APS5_VABI sceUserServiceGetHomeDirectory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSundayDuration(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b819u);
}

int APS5_VABI sceUserServiceSetIPDLeft(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a00du);
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
    return WriteUserStringSetting(userId, value, 0x0780b011u, 11);
}

int APS5_VABI sceUserServiceSetPbtcWednesdayHoursStart(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b80eu);
}

int APS5_VABI sceUserServiceGetSparkEqParam(int userId, std::uint32_t* parameters) {
    if (parameters == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    std::uint32_t profileIndex = 0;
    const int indexResult = ReadUserIntegerSetting(userId, &profileIndex, 0x0780a01au, 0);
    if (indexResult != USER_SERVICE_OK) {
        return indexResult;
    }
    std::array<std::int8_t, 5> bands{};
    if (profileIndex < 3) {
        constexpr std::array<std::uint64_t, 3> defaultProfileIds{0, 0x0000000300020304u,
                                                                 0x00000002020300ffu};
        std::memcpy(bands.data(), &defaultProfileIds[profileIndex], bands.size());
    } else if (profileIndex >= 0x80u && profileIndex <= 0x82u) {
        std::array<std::uint8_t, 73> profile{};
        const int profileResult = ReadUserDataSetting(userId, profile.data(), profile.size(),
                                                       0x0780a017u + (profileIndex & 3u), profile.size());
        if (profileResult != USER_SERVICE_OK) {
            return profileResult;
        }
        for (std::size_t i = 0; i < bands.size(); ++i) {
            bands[i] = static_cast<std::int8_t>(profile[65 + i]);
        }
    } else {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    std::array<std::uint32_t, 7> result{};
    for (std::size_t i = 0; i < bands.size(); ++i) {
        const float value = static_cast<float>(bands[i]);
        std::memcpy(&result[i + 1], &value, sizeof(value));
    }
    result[6] = profileIndex == 0 ? 1u : 0u;
    std::memcpy(parameters, result.data(), sizeof(result));
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceSetEventSortEvent(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x817u);
}

int APS5_VABI sceUserServiceSetGlsLiveQuality3(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c013u);
}

int APS5_VABI sceUserServiceSetNotificationSettings_1(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07801400u);
}

int APS5_VABI sceUserServiceGetPbtcMondayHoursEnd(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b809u, 1440);
}

int APS5_VABI sceUserServiceGetIsFakePlus(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b001u, 0);
}

int APS5_VABI sceUserServiceGetPbtcTuesdayHoursEnd(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b80cu, 1440);
}

int APS5_VABI sceUserServiceGetPartyMuteList(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0813u, 512);
}

int APS5_VABI sceUserServiceGetNpNpId(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780b00du, 65);
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountIdClockType(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780f803u);
}

int APS5_VABI sceUserServiceGetProControllerBtAddress(int userId, std::uint64_t* address) {
    if (address == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    std::array<std::uint8_t, 6> bytes{};
    const int result = ReadUserDataSetting(userId, bytes.data(), bytes.size(), 0x0780a507u, bytes.size());
    if (result != USER_SERVICE_OK) {
        return result;
    }
    *address = 0;
    std::memcpy(address, bytes.data(), bytes.size());
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetSparkEqIndex(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a01au, 0);
}

int APS5_VABI sceUserServiceSetPsnPasswordForDebug(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780b005u, 31);
}

int APS5_VABI sceUserServiceSetParentalGameWhiteList(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x07805c40u, 2000);
}

int APS5_VABI sceUserServiceGetGlsCommunityId(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0848u, 37);
}

int APS5_VABI sceUserServiceUpdateCronosCountryList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGazeCalibData(int userId, void* data) {
    const int result = ReadUserDataSetting(userId, data, 2000, 0x08c7u, 2000);
    if (result != USER_SERVICE_OK) {
        return result;
    }
    return ReadUserDataSetting(userId, static_cast<std::uint8_t*>(data) + 2000, 2000, 0x08c8u, 2000);
}

int APS5_VABI sceUserServiceSetNpAuthErrorFlag(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b004u);
}

int APS5_VABI sceUserServiceGetHearingAssistUseAudiogramData(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806e06u, 0);
}

int APS5_VABI sceUserServiceSetAccessibilityKeyremapEnable(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806c02u);
}

int APS5_VABI sceUserServiceSetPbtcFlag(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b801u);
}

int APS5_VABI sceUserServiceGetPbtcFridayDuration(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b813u, -1);
}

int APS5_VABI sceUserServiceSetGlsStreamingMode(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c02fu);
}

int APS5_VABI sceUserServiceSetPbtcFridayHoursEnd(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b815u);
}

int APS5_VABI sceUserServiceGetProControllerUniqueId1(int userId, void* value) {
    return ReadUserDataSetting(userId, value, 16, 0x08e1u, 16);
}

int APS5_VABI sceUserServiceSetVoiceAgentStatusBitFactory(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780d209u);
}

int APS5_VABI sceUserServiceSetTeamShowAboutTeam(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x835u);
}

int APS5_VABI sceUserServiceGetEventSortEvent(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x817u, 0);
}

int APS5_VABI sceUserServiceGetGlsLfpsSortOrder(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x821u, 0);
}

int APS5_VABI sceUserServiceGetHearingAssistMakeVoicesClearer(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806e08u, 0);
}

int APS5_VABI sceUserServiceGetVoiceAgentEnableByButton(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d204u, -1);
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountIdClockType(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780f803u, 0);
}

int APS5_VABI sceUserServiceTerminateForShellCore(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalMorpheus(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07805c06u);
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountId(int userId, void* value) {
    return WriteUserDataSetting(userId, value, 64, 0x082du, 64);
}

int APS5_VABI sceUserServiceGetCaptureStatus(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806501u, 1969928);
}

int APS5_VABI sceUserServiceSetAgeVerified(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b003u);
}

int APS5_VABI sceUserServiceSetGlsCameraBgFilter(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c018u);
}

int APS5_VABI sceUserServiceSetGlsTtsVolume(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c02au);
}

int APS5_VABI sceUserServiceSetGlsCameraSize(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c017u);
}

int APS5_VABI sceUserServiceSetGlsAnonymousUserId(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780c014u, 32);
}

int APS5_VABI sceUserServiceSetGlsCameraReflection(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c01du);
}

int APS5_VABI sceUserServiceGetHmuZoom(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a00bu, 1);
}

int APS5_VABI sceUserServiceGetGlsBcTitle(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780c043u, 601);
}

int APS5_VABI sceUserServiceGetPbtcFridayHoursEnd(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b815u, 1440);
}

int APS5_VABI sceUserServiceGetLoginFlag(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07800800u, 0);
}

int APS5_VABI sceUserServiceGetHearingAssistAdjustmentLevelLeft(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806e04u, 0);
}

int APS5_VABI sceUserServiceSetFaceRecognitionDeleteCount(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0xdu);
}

int APS5_VABI sceUserServiceGetTriggerEffectStrength(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a040u, 1);
}

int APS5_VABI sceUserServiceSetGlsIsTwitterEnabled(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c002u);
}

int APS5_VABI sceUserServiceGetPbtcSaturdayHoursStart(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b817u, 0);
}

int APS5_VABI sceUserServiceSetHintShowFlag3(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07801f00u);
}

int APS5_VABI sceUserServiceGetVoiceAgentSessionTimeout(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d206u, -1);
}

int APS5_VABI sceUserServiceSetParentalContentControl(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07805c08u);
}

int APS5_VABI sceUserServiceSetGlsCameraEffect(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c019u);
}

int APS5_VABI sceUserServiceSetPbtcTuesdayHoursEnd(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b80cu);
}

int APS5_VABI sceUserServiceGetPartyMuteListA(int userId, void* value) {
    return ReadUserDataSetting(userId, value, 240, 0x084bu, 240);
}

int APS5_VABI sceUserServiceGetGlsCameraEliminationLevel(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c01cu, -1);
}

int APS5_VABI sceUserServiceGetGlsBroadcastersCommentColor(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x621u, 0);
}

int APS5_VABI sceUserServiceGetParentalBrowser(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07805c05u, 0);
}

int APS5_VABI sceUserServiceSetHearingAssistSeparateLR(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806e03u);
}

int APS5_VABI sceUserServiceIsUserStorageAccountBound(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2Brightness(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a101u);
}

int APS5_VABI sceUserServiceSetGlsCameraChromaKeyLevel(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c01au);
}

int APS5_VABI sceUserServiceGetProControllerUniqueId2(int userId, void* value) {
    return ReadUserDataSetting(userId, value, 16, 0x08e2u, 16);
}

int APS5_VABI sceUserServiceSetGlsBroadcastChannel(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780c007u, 32);
}

int APS5_VABI sceUserServiceGetGlsCameraEdgeLevel(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c01fu, -1);
}

int APS5_VABI sceUserServiceGetGlsCameraPosition(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c016u, -1);
}

int APS5_VABI sceUserServiceGetA11yControllerAName(int userId, void* name) {
    return ReadUserDataSetting(userId, name, 82, 0x0780aa06u, 82);
}

int APS5_VABI sceUserServiceSetPbtcWednesdayHoursEnd(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b80fu);
}

int APS5_VABI sceUserServiceSetMonoOutput(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a01bu);
}

int APS5_VABI sceUserServiceSetPartyChatTranscriptionVoiceType(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c606u);
}

int APS5_VABI sceUserServiceGetNotificationSettings(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 4, 0);
}

int APS5_VABI sceUserServiceGetTopMenuTutorialFlag(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x10u, 0);
}

int APS5_VABI sceUserServiceSetIPDRight(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a00eu);
}

int APS5_VABI sceUserServiceGetPbtcSaturdayHoursEnd(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b818u, 1440);
}

int APS5_VABI sceUserServiceGetImeLastUnit(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07808081u, 0);
}

int APS5_VABI sceUserServiceSetNpNpId(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780b00du, 65);
}

int APS5_VABI sceUserServiceSetTriggerEffectStrength(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a040u);
}

int APS5_VABI sceUserServiceGetGlsIsRecievedMessageHidden(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c00au, 0);
}

int APS5_VABI sceUserServiceSetVoiceAgentSessionTimeout(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780d206u);
}

int APS5_VABI sceUserServiceSetGlsLanguageFilter(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x614u);
}

int APS5_VABI sceUserServiceGetHmd2Vibration(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a105u, 1);
}

int APS5_VABI sceUserServiceIsLoggedIn(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcMondayHoursStart(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b808u, 0);
}

int APS5_VABI sceUserServiceSetAppSortOrder(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0xeu);
}

int APS5_VABI sceUserServiceSetMouseHandType(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a008u);
}

int APS5_VABI sceUserServiceGetIsQuickSignup(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x103u, 0);
}

int APS5_VABI sceUserServiceIsKratosPrimaryUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentEnableWup(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780d203u);
}

int APS5_VABI sceUserServiceGetGlsCameraDepthLevel(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c01eu, -1);
}

int APS5_VABI sceUserServiceGetHmuBrightness(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a00au, 32);
}

int APS5_VABI sceUserServiceSetGlsHintFlag(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c022u);
}

int APS5_VABI sceUserServiceIsLoggedInWithoutLock(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetShareStatus3(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c520u);
}

int APS5_VABI sceUserServiceGetGlsTtsVolume(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c02au, -1);
}

int APS5_VABI sceUserServiceGetMicLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetImeLastUnit(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07808081u);
}

int APS5_VABI sceUserServiceSetPbtcMode(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b802u);
}

int APS5_VABI sceUserServiceSetPbtcThursdayHoursEnd(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b812u);
}

int APS5_VABI sceUserServiceGetSaveDataAutoUpload(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780e402u, 1);
}

int APS5_VABI sceUserServiceSetAccessibilityKeyremapDataAston(int userId, void* data) {
    return WriteUserDataSetting(userId, data, 16, 0x08f2u, 16);
}

int APS5_VABI sceUserServiceGetVolumeForSidetone(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a006u, 3);
}

int APS5_VABI sceUserServiceSetProControllerUniqueId2(int userId, void* value) {
    return WriteUserDataSetting(userId, value, 16, 0x08e2u, 16);
}

int APS5_VABI sceUserServiceGetFriendCustomListLastFocus(int userId, std::uint64_t* value) {
    return ReadUserDataSetting(userId, value, sizeof(*value), 0x084au, sizeof(*value));
}

int APS5_VABI sceUserServiceSetHmd2GazeTarget(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a03eu);
}

int APS5_VABI sceUserServiceGetKeyRepeatSpeed(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07809006u, 4);
}

int APS5_VABI sceUserServiceSetParentalDvd(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07805c04u);
}

int APS5_VABI sceUserServiceGetImePointerMode(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07808083u, 0);
}

int APS5_VABI sceUserServiceGetVolumeForController(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastChannel(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780c007u, 32);
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountIdParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHintShowFlag2(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07801b00u, 0);
}

int APS5_VABI sceUserServiceSetHearingAssistPresetType(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806e02u);
}

int APS5_VABI sceUserServiceGetNotificationSettings_3(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07801600u, -1048549);
}

int APS5_VABI sceUserServiceGetGlsAnonymousUserId(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780c014u, 32);
}

int APS5_VABI sceUserServiceSetA11yControllerBDeviceUniqueIds(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780aa10u, 53);
}

int APS5_VABI sceUserServiceGetVoiceAgentEnableWup(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d203u, -1);
}

int APS5_VABI sceUserServiceGetNpDateOfBirth(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780b011u, 11);
}

int APS5_VABI sceUserServiceSetKeyboardType(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07809004u);
}

int APS5_VABI sceUserServiceSetParentalGame(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07805c01u);
}

int APS5_VABI sceUserServiceGetHearingAssistSeparateLR(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806e03u, 0);
}

int APS5_VABI sceUserServiceGetVoiceAgentTtsVolume(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d207u, -1);
}

int APS5_VABI sceUserServiceGetPbtcMondayDuration(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b807u, -1);
}

int APS5_VABI sceUserServiceGetNpSubAccount(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b009u, 0);
}

int APS5_VABI sceUserServiceGetShareStatus2(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x827u, 0);
}

int APS5_VABI sceUserServiceGetGlsCamCrop(int userId, void* value) {
    return ReadUserDataSetting(userId, value, 8, 0x0822u, 8);
}

int APS5_VABI sceUserServiceGetVoiceRecognitionLastUsedOsk(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x868u, 0);
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountId(int userId, void* value) {
    return ReadUserDataSetting(userId, value, 64, 0x082du, 64);
}

int APS5_VABI sceUserServiceSetVoiceRecognitionTutorialState(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x12u);
}

int APS5_VABI sceUserServiceGetGlsLiveQuality5(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c02bu, 0);
}

int APS5_VABI sceUserServiceSetHearingAssistUseAudiogramData(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806e06u);
}

int APS5_VABI sceUserServiceGetPbtcFlag(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b801u, 0);
}

int APS5_VABI sceUserServiceGetIPDRight(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a00eu, -1);
}

int APS5_VABI sceUserServiceSetPbtcPlayTimeLastUpdated(int userId, std::uint64_t value) {
    return WriteUserDataSetting(userId, &value, sizeof(value), 0x0852u, sizeof(value));
}

int APS5_VABI sceUserServiceSetPbtcTuesdayHoursStart(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b80bu);
}

int APS5_VABI sceUserServiceGetNotificationBehavior(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x83fu, 0);
}

int APS5_VABI sceUserServiceSetPadProfileDataHuntList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistAudiogramData(int userId, void* value) {
    return ReadUserDataSetting(userId, value, 14, 0x091cu, 14);
}

int APS5_VABI sceUserServiceGetHintShowFlag(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07801900u, 0);
}

int APS5_VABI sceUserServiceSetTopMenuLimitItem(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0xfu);
}

int APS5_VABI sceUserServiceSetPbtcSundayHoursEnd(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b81bu);
}

int APS5_VABI sceUserServiceGetParentalBdAge(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07805c02u, 0);
}

int APS5_VABI sceUserServiceGetSaveDataSort(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780e401u, 1);
}

int APS5_VABI sceUserServiceGetHrtf(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a010u, 3);
}

int APS5_VABI sceUserServiceSetAppearOfflineSetting(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentLanguage(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780d202u);
}

int APS5_VABI sceUserServiceSetIsRemotePlayAllowed(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x07807821u);
}

int APS5_VABI sceUserServiceGetGlsStreamingMode(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c02fu, 0);
}

int APS5_VABI sceUserServiceGetChatStatus(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07801800u, 0);
}

int APS5_VABI sceUserServiceSetVoiceAgentStatusBit(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780d208u);
}

int APS5_VABI sceUserServiceGetNpCountryCode(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780b00eu, 3);
}

int APS5_VABI sceUserServiceGetParentalGameWhiteList(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x07805c40u, 2000);
}

int APS5_VABI sceUserServiceSetHmd2ScreenSize(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a102u);
}

int APS5_VABI sceUserServiceGetTriggerEffectEnabled(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a040u, 1);
}

int APS5_VABI sceUserServiceGetA11yControllerInitialSetupProgress(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780aa07u, 0);
}

int APS5_VABI sceUserServiceGetVolumeForGenericUSB(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetControlCenterFunctionControlSettings(int userId, void* settings) {
    return ReadUserDataSetting(userId, settings, 64, 0x08aeu, 64);
}

int APS5_VABI sceUserServiceSetGlsInitSpectating(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x60fu);
}

int APS5_VABI sceUserServiceGetGlsIsMuteEnabled(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c005u, 1);
}

int APS5_VABI sceUserServiceGetNpLanguageCode2(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780b010u, 36);
}

int APS5_VABI sceUserServiceSetFriendCustomListLastFocus(int userId, std::uint64_t value) {
    return WriteUserDataSetting(userId, &value, sizeof(value), 0x084au, sizeof(value));
}

int APS5_VABI sceUserServiceSetPartyMuteList(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0813u, 512);
}

int APS5_VABI sceUserServiceSetAccessibilityKeyremapData(int userId, void* data) {
    return WriteUserDataSetting(userId, data, 16, 0x0809u, 16);
}

int APS5_VABI sceUserServiceSetPbtcFridayHoursStart(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b814u);
}

int APS5_VABI sceUserServiceSetPbtcMondayHoursEnd(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b809u);
}

int APS5_VABI sceUserServiceGetControlCenterStatus(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x8b8u, 0);
}

int APS5_VABI sceUserServiceGetSparkEqProfile(int userId, std::uint32_t profileIndex, void* profile) {
    if (profile == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (profileIndex < 3u) {
        constexpr std::array<std::uint64_t, 3> defaultProfileIds{0, 0x0000000300020304u, 0x00000002020300ffu};
        std::memcpy(static_cast<std::uint8_t*>(profile) + 65, &defaultProfileIds[profileIndex], sizeof(std::uint64_t));
        return USER_SERVICE_OK;
    }
    if (profileIndex < 0x80u || profileIndex > 0x82u) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return ReadUserDataSetting(userId, profile, 73, 0x0780a017u + (profileIndex & 3u), 73);
}

int APS5_VABI sceUserServiceGetGlsTtsPitch(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c029u, 99);
}

int APS5_VABI sceUserServiceSetGlsBroadcastService(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c008u);
}

int APS5_VABI sceUserServiceGetAutoLoginEnabled(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x07800400u, 0);
}

int APS5_VABI sceUserServiceSetPbtcSaturdayHoursStart(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b817u);
}

int APS5_VABI sceUserServiceGetNotificationSettings_1(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07801400u, -8078341);
}

int APS5_VABI sceUserServiceSetHmd2MuteAssign(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a104u);
}

int APS5_VABI sceUserServiceGetPasscode(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 2, 16);
}

int APS5_VABI sceUserServiceSetMousePointerSpeed(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a009u);
}

int APS5_VABI sceUserServiceGetGlsBroadcastUiLayout(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c006u, 0);
}

int APS5_VABI sceUserServiceSetSharePlayFlags(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07807c03u);
}

int APS5_VABI sceUserServiceSetIsFakePlus(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b001u);
}

int APS5_VABI sceUserServiceGetAccountRemarks(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x07804000u, 32);
}

int APS5_VABI sceUserServiceSetAccessibilityZoom(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806c01u);
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountIdTtl(int userId, std::uint64_t* value) {
    return ReadUserDataSetting(userId, value, sizeof(*value), 0x0780f802u, sizeof(*value));
}

int APS5_VABI sceUserServiceSetSaveDataSort(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780e401u);
}

int APS5_VABI sceUserServiceSetPlayTogetherFlags(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x826u);
}

int APS5_VABI sceUserServiceSetHearingAssistAdjustmentLevelRight(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806e05u);
}

int APS5_VABI sceUserServiceGetGlsTtsSpeed(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c028u, 99);
}

int APS5_VABI sceUserServiceGetNpEnv(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780b007u, 17);
}

int APS5_VABI sceUserServiceGetGlsCameraSize(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c017u, -1);
}

int APS5_VABI sceUserServiceGetDiscPlayerFlag(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07800a00u, 0);
}

int APS5_VABI sceUserServiceSetHmd2PlayAreaShape(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a106u);
}

int APS5_VABI sceUserServiceSetGlsIsCameraHidden(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c004u);
}

int APS5_VABI sceUserServiceSetUserStatus(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07800f00u);
}

int APS5_VABI sceUserServiceGetEventFilterTeamEvent(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x831u, 0);
}

int APS5_VABI sceUserServiceSetPsCloudFeedbackTime(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c801u);
}

int APS5_VABI sceUserServiceSetUserGroupIndex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyMuteListA(int userId, void* value) {
    return WriteUserDataSetting(userId, value, 240, 0x084bu, 240);
}

int APS5_VABI sceUserServiceSetNpLanguageCode2(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780b010u, 36);
}

int APS5_VABI sceUserServiceSetShareStatus2(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x827u);
}

int APS5_VABI sceUserServiceSetForegroundUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsAccessTokenTwitch(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780c00cu, 41);
}

int APS5_VABI sceUserServiceGetNpOfflineAccountAdult(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x838u, 0);
}

int APS5_VABI sceUserServiceSetGamePresets(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 4);
}

int APS5_VABI sceUserServiceGetImeInputType(int userId, std::uint64_t language, std::int32_t* value) {
    const auto* settings = FindUserImeLanguageSettings(language);
    if (settings == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return ReadUserIntegerSetting(userId, value, settings->inputTypeKey, 0);
}

int APS5_VABI sceUserServiceGetVolumeForMorpheusSidetone(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a00cu, 0);
}

int APS5_VABI sceUserServiceSetGlsLiveQuality4(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c023u);
}

int APS5_VABI sceUserServiceGetHearingAssistPresetType(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806e02u, 0);
}

int APS5_VABI sceUserServiceGetHearingAssistWeakenAdjustment(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806e07u, 0);
}

int APS5_VABI sceUserServiceSetPbtcWednesdayDuration(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b80du);
}

int APS5_VABI sceUserServiceIsGuestUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPadProfileDataHuntList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAccessibilityKeyremapData(int userId, void* data) {
    return ReadUserDataSetting(userId, data, 16, 0x0809u, 16);
}

int APS5_VABI sceUserServiceSetCaptureStatus2(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806540u);
}

int APS5_VABI sceUserServiceGetVoiceAgentTtsSpeed(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d205u, -2147483648);
}

int APS5_VABI sceUserServiceGetPbtcWednesdayHoursEnd(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b80fu, 1440);
}

int APS5_VABI sceUserServiceGetAgeVerified(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b003u, 0);
}

int APS5_VABI sceUserServiceSetGlsCameraEdgeLevel(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c01fu);
}

int APS5_VABI sceUserServiceGetPartyChatTranscriptionLanguageType(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c605u, 0);
}

int APS5_VABI sceUserServiceSetVoiceAgentTtsVolume(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780d207u);
}

int APS5_VABI sceUserServiceGetPbtcTuesdayDuration(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b80au, -1);
}

int APS5_VABI sceUserServiceGetUserWithHearingAssist(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpOnlineId(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780b00cu, 17);
}

int APS5_VABI sceUserServiceGetHintShowFlag3(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07801f00u, 0);
}

int APS5_VABI sceUserServiceSetNotificationSettings_3(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07801600u);
}

int APS5_VABI sceUserServiceSetGlsIsFacebookEnabled(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c001u);
}

int APS5_VABI sceUserServiceSetNpEnv(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780b007u, 17);
}

int APS5_VABI sceUserServiceSetGlsCameraBrightness(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c020u);
}

int APS5_VABI sceUserServiceDestroyUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTopMenuNotificationFlag(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806403u);
}

int APS5_VABI sceUserServiceGetCreatedVersion(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07800e00u, -1);
}

int APS5_VABI sceUserServiceGetGlsCameraChromaKeyLevel(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c01au, -1);
}

int APS5_VABI sceUserServiceGetNpOfflineAccountId(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780b004u, 37);
}

int APS5_VABI sceUserServiceGetShareStatus3(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c520u, 0);
}

int APS5_VABI sceUserServiceSetControlCenterStatus(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x8b8u);
}

int APS5_VABI sceUserServiceSetMicLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTeamShowAboutTeam(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x835u, 0);
}

int APS5_VABI sceUserServiceGetEventVsh(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerBName(int userId, void* name) {
    return ReadUserDataSetting(userId, name, 82, 0x0780aa15u, 82);
}

int APS5_VABI sceUserServiceSetNpAge(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b012u);
}

int APS5_VABI sceUserServiceGetPlayingBigAppTitleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHintShowFlag2(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07801b00u);
}

int APS5_VABI sceUserServiceSetCaptureStatus3(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806502u);
}

int APS5_VABI sceUserServiceSetSparkEqIndex(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a01au);
}

int APS5_VABI sceUserServiceSetPbtcMondayDuration(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b807u);
}

int APS5_VABI sceUserServiceSetEventSortTitle(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x818u);
}

int APS5_VABI sceUserServiceSetNpOnlineId(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780b00cu, 17);
}

int APS5_VABI sceUserServiceSetA11yControllerBName(int userId, void* name) {
    return WriteUserDataSetting(userId, name, 82, 0x0780aa15u, 82);
}

int APS5_VABI sceUserServiceGetSaveDataAutoSync(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780e440u, 1);
}

int APS5_VABI sceUserServiceGetDefaultNewUserGroupName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2Brightness(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a101u, 16);
}

int APS5_VABI sceUserServiceGetHoldAudioOutDevice(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a00fu, 0);
}

int APS5_VABI sceUserServiceSetVibrationStrength(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a005u);
}

int APS5_VABI sceUserServiceUnregisterEventCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTopMenuNotificationFlag(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806403u, 0);
}

int APS5_VABI sceUserServiceGetPbtcSundayDuration(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b819u, -1);
}

int APS5_VABI sceUserServiceSetHearingAssistAudiogramData(int userId, void* value) {
    return WriteUserDataSetting(userId, value, 14, 0x091cu, 14);
}

int APS5_VABI sceUserServiceSetSaveDataAutoSync(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780e440u);
}

int APS5_VABI sceUserServiceSetPbtcFridayDuration(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b813u);
}

int APS5_VABI sceUserServiceSetGlsTtsSpeed(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c028u);
}

int APS5_VABI sceUserServiceGetPartyChatTranscriptionFlag(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c604u, 0);
}

int APS5_VABI sceUserServiceGetPbtcSaturdayDuration(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b816u, -1);
}

int APS5_VABI sceUserServiceGetPartySettingFlags(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c601u, 0);
}

int APS5_VABI sceUserServiceGetImeInputSettings(int userId, std::uint64_t language, std::uint32_t* value) {
    const auto* settings = FindUserImeLanguageSettings(language);
    if (settings == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return ReadUserIntegerSetting(userId, value, settings->inputSettingsKey, 0);
}

int APS5_VABI sceUserServiceSetGlsIsRecDisabled(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c009u);
}

int APS5_VABI sceUserServiceSetA11yControllerAName(int userId, void* name) {
    return WriteUserDataSetting(userId, name, 82, 0x0780aa06u, 82);
}

int APS5_VABI sceUserServiceSetEventFilterTeamEvent(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x831u);
}

int APS5_VABI sceUserServiceGetA11yControllerBDeviceUniqueIds(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780aa10u, 53);
}

int APS5_VABI sceUserServiceSetGlsCameraEliminationLevel(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c01cu);
}

int APS5_VABI sceUserServiceGetVibrationStrength(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a005u, 1);
}

int APS5_VABI sceUserServiceGetNpAccountUpgradeFlag(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b00bu, 0);
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
    return ReadUserIntegerSetting(userId, value, 0x07806e05u, 0);
}

int APS5_VABI sceUserServiceGetPlatformPrivacyWs1Internal(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07801200u, -1);
}

int APS5_VABI sceUserServiceGetGlsIsRecDisabled(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c009u, 1);
}

int APS5_VABI sceUserServiceGetGlsCameraBrightness(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c020u, -1);
}

int APS5_VABI sceUserServiceSetNpLanguageCode(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0780b00fu, 6);
}

int APS5_VABI sceUserServiceGetPbtcThursdayHoursStart(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b811u, 0);
}

int APS5_VABI sceUserServiceSetGazeActivate(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a03bu);
}

int APS5_VABI sceUserServiceSetPlatformPrivacyWs1(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07801200u);
}

int APS5_VABI sceUserServiceSetVolumeForSidetone(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a006u);
}

int APS5_VABI sceUserServiceSetSaveDataTutorialFlag(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780e403u);
}

int APS5_VABI sceUserServiceSetPbtcPlayTime(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b805u);
}

int APS5_VABI sceUserServiceGetGlsBroadcastService(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c008u, 0);
}

int APS5_VABI sceUserServiceGetIsFakePremium(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b040u, 0);
}

int APS5_VABI sceUserServiceSetGlsLiveQuality5(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c02bu);
}

int APS5_VABI sceUserServiceSetSaveDataAutoUpload(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780e402u);
}

int APS5_VABI sceUserServiceGetSaveDataTutorialFlag(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780e403u, 0);
}

int APS5_VABI sceUserServiceGetGlsLiveQuality4(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c023u, 0);
}

int APS5_VABI sceUserServiceGetNpLanguageCode(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780b00fu, 6);
}

int APS5_VABI sceUserServiceSetPbtcThursdayDuration(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b810u);
}

int APS5_VABI sceUserServiceGetFaceRecognitionDeleteCount(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0xdu, 0);
}

int APS5_VABI sceUserServiceGetVoiceAgentEnable(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d201u, -1);
}

int APS5_VABI sceUserServiceSetEventCalendarType(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x830u);
}

int APS5_VABI sceUserServiceSetProControllerAlreadyNoticed(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a508u);
}

int APS5_VABI sceUserServiceGetGlsTtsFlags(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c027u, 0);
}

int APS5_VABI sceUserServiceGetGlsBroadcastersComment(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0620u, 841);
}

int APS5_VABI sceUserServiceGetPbtcTzOffset(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b803u, 0);
}

int APS5_VABI sceUserServiceGetPbtcPlayTime(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b805u, 0);
}

int APS5_VABI sceUserServiceGetVoiceAgentStatusBit(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d208u, 0);
}

int APS5_VABI sceUserServiceGetNotificationSettings2(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07801300u, -1015797);
}

int APS5_VABI sceUserServiceGetEventUiFlag(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x819u, 0);
}

int APS5_VABI sceUserServiceSetGlsBroadcastUiLayout(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c006u);
}

int APS5_VABI sceUserServiceSetVibrationEnabled(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a005u);
}

int APS5_VABI sceUserServiceSetPbtcAdditionalTime(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b804u);
}

int APS5_VABI sceUserServiceIsSharePlayClientUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetMonoOutput(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a01bu, 0);
}

int APS5_VABI sceUserServiceSetNpLoginId(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x07800600u, 65);
}

int APS5_VABI sceUserServiceGetShareStatus(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x700u, 0);
}

int APS5_VABI sceUserServiceSetGlsIsRecievedMessageHidden(int userId, bool value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c00au);
}

int APS5_VABI sceUserServiceSetGlsLiveQuality2(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c012u);
}

int APS5_VABI sceUserServiceGetHmd2GazeActivate(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a03bu, 0);
}

int APS5_VABI sceUserServiceSetGlsCameraPosition(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c016u);
}

int APS5_VABI sceUserServiceSetGlsMessageFilterLevel(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c010u);
}

int APS5_VABI sceUserServiceGetUserGroupNameList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentEnableFeedback(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780d20au);
}

int APS5_VABI sceUserServiceGetGazeActivate(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a03bu, 0);
}

int APS5_VABI sceUserServiceGetGlsIsTwitterEnabled(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c002u, 0);
}

int APS5_VABI sceUserServiceGetTopMenuLimitItem(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0xfu, 0);
}

int APS5_VABI sceUserServiceGetNpAuthErrorFlag(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b004u, 0);
}

int APS5_VABI sceUserServiceGetGlsIsCameraHidden(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c004u, 1);
}

int APS5_VABI sceUserServiceGetMousePointerSpeed(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a009u, 4);
}

int APS5_VABI sceUserServiceSetFaceRecognitionRegisterCount(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0xcu);
}

int APS5_VABI sceUserServiceGetHmd2PlayAreaShape(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a106u, 0);
}

int APS5_VABI sceUserServiceSetUserName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerADeviceUniqueIds(int userId, char* value, std::size_t size) {
    return ReadUserStringSetting(userId, value, size, 0x0780aa01u, 53);
}

int APS5_VABI sceUserServiceGetGlsFloatingMessage(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c011u, 0);
}

int APS5_VABI sceUserServiceInitializeForShellCore(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBcTags(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c026u, 0);
}

int APS5_VABI sceUserServiceGetPbtcMode(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b802u, 0);
}

int APS5_VABI sceUserServiceSetImeInputType(int userId, std::uint64_t language, std::int32_t value) {
    const auto* settings = FindUserImeLanguageSettings(language);
    if (settings == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return WriteUserIntegerSetting(userId, value, settings->inputTypeKey);
}

int APS5_VABI sceUserServiceGetProControllerUniqueId3(int userId, void* value) {
    return ReadUserDataSetting(userId, value, 16, 0x08e3u, 16);
}

int APS5_VABI sceUserServiceSetHmd2GazeActivate(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a03bu);
}

int APS5_VABI sceUserServiceGetCaptureStatus3(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806502u, 0);
}

int APS5_VABI sceUserServiceGetGlsCameraReflection(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c01du, 0);
}

int APS5_VABI sceUserServiceGetNotificationSettings_2(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07801500u, -1015797);
}

int APS5_VABI sceUserServiceGetGlsLiveQuality(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c003u, 0);
}

int APS5_VABI sceUserServiceGetFaceRecognitionRegisterCount(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0xcu, 0);
}

int APS5_VABI sceUserServiceSetKeyRepeatStartingTime(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07809005u);
}

int APS5_VABI sceUserServiceGetIsRemotePlayAllowed(int userId, bool* value) {
    return ReadUserIntegerSetting(userId, value, 0x07807821u, 0);
}

int APS5_VABI sceUserServiceSetPbtcMondayHoursStart(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b808u);
}

int APS5_VABI sceUserServiceGetGlsLiveQuality3(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c013u, 0);
}

int APS5_VABI sceUserServiceGetPsCloudFeedbackTime(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c801u, 0);
}

int APS5_VABI sceUserServiceSetVoiceAgentTtsSpeed(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780d205u);
}

int APS5_VABI sceUserServiceSetVoiceAgentEnable(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780d201u);
}

int APS5_VABI sceUserServiceGetPbtcWednesdayHoursStart(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b80eu, 0);
}

int APS5_VABI sceUserServiceSetTopMenuTutorialFlag(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x10u);
}

int APS5_VABI sceUserServiceGetIsPersonalizedHrtfEnabled(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a01cu, 0);
}

int APS5_VABI sceUserServiceSetPadSpeakerVolume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAccessibilityKeyremapEnableAston(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07806c07u, 0);
}

int APS5_VABI sceUserServiceGetHmd2ScreenSize(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a102u, 5);
}

int APS5_VABI sceUserServiceGetGlsInitSpectating(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x60fu, 0);
}

int APS5_VABI sceUserServiceGetLastLoginOrder(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07800900u, 0);
}

int APS5_VABI sceUserServiceSetGlsTtsPitch(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c029u);
}

int APS5_VABI sceUserServiceGetMouseHandType(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a008u, 0);
}

int APS5_VABI sceUserServiceSetEventUiFlag(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x819u);
}

int APS5_VABI sceUserServiceSetImeInputSettings(int userId, std::uint64_t language, std::uint32_t value) {
    const auto* settings = FindUserImeLanguageSettings(language);
    if (settings == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return WriteUserIntegerSetting(userId, value, settings->inputSettingsKey);
}

int APS5_VABI sceUserServiceSetProControllerUniqueId1(int userId, void* value) {
    return WriteUserDataSetting(userId, value, 16, 0x08e1u, 16);
}

int APS5_VABI sceUserServiceGetPbtcPlayTimeLastUpdated(int userId, std::uint64_t* value) {
    return ReadUserDataSetting(userId, value, sizeof(*value), 0x0852u, sizeof(*value));
}

int APS5_VABI sceUserServiceSetHmuBrightness(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a00au);
}

int APS5_VABI sceUserServiceRegisterEventCallback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalMorpheus(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07805c06u, 0);
}

int APS5_VABI sceUserServiceSetPbtcTuesdayDuration(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b80au);
}

int APS5_VABI sceUserServiceGetGlsOverlayPosition(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c030u, -1);
}

int APS5_VABI sceUserServiceGetEventSortTitle(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x818u, 0);
}

int APS5_VABI sceUserServiceSetHmuZoom(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a00bu);
}

int APS5_VABI sceUserServiceSetVolumeForMorpheusSidetone(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a00cu);
}

int APS5_VABI sceUserServiceGetGlsLanguageFilter(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x614u, 0);
}

int APS5_VABI sceUserServiceGetVibrationEnabled(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a005u, 1);
}

int APS5_VABI sceUserServiceSetSharePlayResolutionHost(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x80cu);
}

int APS5_VABI sceUserServiceSetPbtcSundayHoursStart(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b81au);
}

int APS5_VABI sceUserServiceSetNpOfflineAccountAdult(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x838u);
}

int APS5_VABI sceUserServiceSetTriggerEffectEnabled(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a040u);
}

int APS5_VABI sceUserServiceSetHearingAssistAdjustmentLevelLeft(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806e04u);
}

int APS5_VABI sceUserServiceGetLightBarBaseBrightness(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780a007u, 0);
}

int APS5_VABI sceUserServiceGetVoiceAgentStatusBitFactory(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780d209u, 0);
}

int APS5_VABI sceUserServiceSetHearingAssistMakeVoicesClearer(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x07806e08u);
}

int APS5_VABI sceUserServiceSetGlsCommunityId(int userId, const char* value) {
    return WriteUserStringSetting(userId, value, 0x0848u, 37);
}

int APS5_VABI sceUserServiceSetGlsCamCrop(int userId, void* value) {
    return WriteUserDataSetting(userId, value, 8, 0x0822u, 8);
}

int APS5_VABI sceUserServiceSetPbtcTzOffset(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b803u);
}

int APS5_VABI sceUserServiceGetParentalGame(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07805c01u, 0);
}

int APS5_VABI sceUserServiceSetPbtcThursdayHoursStart(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780b811u);
}

int APS5_VABI sceUserServiceSetIsHrtfEnabled(int userId, std::int32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780a011u);
}

int APS5_VABI sceUserServiceGetParentalGameAgeLevel(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x07805c07u, 0);
}

int APS5_VABI sceUserServiceSetNotificationBehavior(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x83fu);
}

int APS5_VABI sceUserServiceGetUserGroupNum(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartySettingFlags(int userId, std::uint32_t value) {
    return WriteUserIntegerSetting(userId, value, 0x0780c601u);
}

int APS5_VABI sceUserServiceGetGlsCameraEffect(int userId, std::uint32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780c019u, 0);
}

int APS5_VABI sceUserServiceGetPbtcSundayHoursEnd(int userId, std::int32_t* value) {
    return ReadUserIntegerSetting(userId, value, 0x0780b81bu, 1440);
}

int APS5_VABI sceUserServiceGetIsSpartacusEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

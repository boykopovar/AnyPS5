extern "C" {
#include <cstdio>
#include <cerrno>
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <atomic>
#include <array>
#include <cerrno>
#include <cstring>
#include <deque>
#include <mutex>
#include <vector>
#include <shared_mutex>
#include <memory>
#include <condition_variable>
#include <algorithm>
#include <thread>
#include "prx/libSceUserService/UserService.hpp"

namespace {

std::atomic<bool> initialized{false};
bool terminating{};
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
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(setting, &value, sizeof(value));
 errno = savedErrno;
 if (!copied) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
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
 const int32_t result = value();
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(type, &result, sizeof(result));
 errno = savedErrno;
 if (!copied) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 return USER_SERVICE_OK;
}
bool UserGroupSlotExists(int index) {
    if (index < 0 || index > 128) {
        return false;
    }
    char path[64];
    std::snprintf(path, sizeof(path), "/system_data/priv/home/user_list.dat.%d", index);
    FileStat status{};
    return sceKernelStat(path, &status) == 0;
}

struct RegisteredEventCallback {
 UserServiceEventCallback callback;
 void* userData;
 std::mutex mutex;
 std::condition_variable idle;
 bool active{true};
 std::size_t inFlight{};
};

std::vector<std::shared_ptr<RegisteredEventCallback>> eventCallbacks;
std::shared_mutex eventCallbacksMutex;
struct CallbackInvocation {
 const RegisteredEventCallback* entry;
 CallbackInvocation* previous;
};
thread_local CallbackInvocation* callbacksOnThisThread{};

std::size_t CurrentThreadCallbackDepth(const RegisteredEventCallback* entry) {
 std::size_t depth{};
 for (auto* invocation = callbacksOnThisThread; invocation != nullptr; invocation = invocation->previous) {
  depth += invocation->entry == entry;
 }
 return depth;
}

void DeactivateAndWait(const std::shared_ptr<RegisteredEventCallback>& entry) {
 std::unique_lock lock(entry->mutex);
 entry->active = false;
 const std::size_t currentThreadDepth = CurrentThreadCallbackDepth(entry.get());
 entry->idle.wait(lock, [&] { return entry->inFlight <= currentThreadDepth; });
}


void RemoveDrainedCallbacks();

void DispatchUserEvent(const std::vector<std::shared_ptr<RegisteredEventCallback>>& callbacks,
                       const SceUserServiceEvent& event) {
 for (const auto& entry : callbacks) {
  UserServiceEventCallback callback;
  void* userData;
  {
   std::lock_guard lock(entry->mutex);
   if (!entry->active) {
    continue;
   }
   ++entry->inFlight;
   callback = entry->callback;
   userData = entry->userData;
  }
  CallbackInvocation invocation{entry.get(), callbacksOnThisThread};
  callbacksOnThisThread = &invocation;
  try {
   callback(&event, userData);
  } catch (...) {
   callbacksOnThisThread = invocation.previous;
   {
    std::lock_guard lock(entry->mutex);
    --entry->inFlight;
   }
   entry->idle.notify_all();
   RemoveDrainedCallbacks();
   throw;
  }
  callbacksOnThisThread = invocation.previous;
  {
   std::lock_guard lock(entry->mutex);
   --entry->inFlight;
  }
  entry->idle.notify_all();
  RemoveDrainedCallbacks();
 }
}
struct CallbackNotification {
 std::vector<std::shared_ptr<RegisteredEventCallback>> callbacks;
 SceUserServiceEvent event;
};

class EventDispatcher {
 std::mutex mutex;
 std::condition_variable changed;
 std::deque<CallbackNotification> notifications;
 bool stopping{};
 std::thread worker;
public:
 EventDispatcher() : worker([this] {
  std::unique_lock lock(mutex);
  while (true) {
   changed.wait(lock, [&] { return stopping || !notifications.empty(); });
   if (stopping) return;
   auto notification = std::move(notifications.front());
   notifications.pop_front();
   lock.unlock();
   DispatchUserEvent(notification.callbacks, notification.event);
   lock.lock();
  }
 }) {}
 ~EventDispatcher() {
  {
   std::lock_guard lock(mutex);
   stopping = true;
  }
  changed.notify_all();
  worker.join();
 }
 void Enqueue(CallbackNotification notification) {
  {
   std::lock_guard lock(mutex);
   notifications.push_back(std::move(notification));
  }
  changed.notify_one();
 }
};

void NotifyUserEvent(std::uint32_t eventType, int userId) {
 std::shared_lock lock(eventCallbacksMutex);
 if (!initialized.load() || eventCallbacks.empty()) return;
 static EventDispatcher dispatcher;
 dispatcher.Enqueue({eventCallbacks, {eventType, userId}});
}

void RemoveDrainedCallbacks() {
 std::unique_lock lock(eventCallbacksMutex);
 std::erase_if(eventCallbacks, [](const auto& entry) {
  std::lock_guard entryLock(entry->mutex);
  return !entry->active && entry->inFlight == 0;
 });
}

void ClearEventCallbacks() {
 std::vector<std::shared_ptr<RegisteredEventCallback>> callbacks;
 {
  std::unique_lock lock(eventCallbacksMutex);
  callbacks = eventCallbacks;
  for (const auto& entry : callbacks) {
   std::lock_guard entryLock(entry->mutex);
   entry->active = false;
  }
 }
 for (const auto& entry : callbacks) {
  DeactivateAndWait(entry);
 }
 RemoveDrainedCallbacks();
}
}


int APS5_VABI sceUserServiceSetPartyMuteListA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpLanguageCode2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetShareStatus2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetForegroundUser(int userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsAccessTokenTwitch(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpOfflineAccountAdult(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGamePresets(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetImeInputType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVolumeForMorpheusSidetone(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality4(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistPresetType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistWeakenAdjustment(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcWednesdayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsGuestUser(std::int32_t userId) {
    return (static_cast<std::uint32_t>(userId) & 0xe0000000u) == 0x20000000u ? 1 : 0;
}

int APS5_VABI sceUserServiceGetPadProfileDataHuntList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAccessibilityKeyremapData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetCaptureStatus2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentTtsSpeed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcWednesdayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAgeVerified(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraEdgeLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartyChatTranscriptionLanguageType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentTtsVolume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcTuesdayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetUserWithHearingAssist(std::int32_t* userId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpOnlineId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHintShowFlag3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings_3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsFacebookEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpEnv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraBrightness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceDestroyUser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTopMenuNotificationFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetCreatedVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraChromaKeyLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpOfflineAccountId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetShareStatus3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetControlCenterStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetMicLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTeamShowAboutTeam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventVsh(SceUserServiceEvent* event) {
 return sceUserServiceGetEvent(event);
}

int APS5_VABI sceUserServiceGetA11yControllerBName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpAge(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPlayingBigAppTitleId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHintShowFlag2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetCaptureStatus3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSparkEqIndex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcMondayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventSortTitle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpOnlineId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetA11yControllerBName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSaveDataAutoSync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetDefaultNewUserGroupName(char* name, std::size_t size) {
    if (name == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (size < 129) {
        return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    }
    int groupIndex = 0;
    while (groupIndex < 128 && UserGroupSlotExists(groupIndex)) {
        ++groupIndex;
    }
    char text[129];
    const int length = std::snprintf(text, sizeof(text), "User Group %d", groupIndex + 1);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(name, text, static_cast<std::size_t>(length) + 1);
    errno = savedErrno;
    if (!copied) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetHmd2Brightness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHoldAudioOutDevice(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVibrationStrength(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceUnregisterEventCallback(UserServiceEventCallback callback, void* userData) {
 if (!initialized.load()) {
  return USER_SERVICE_ERROR_NOT_INITIALIZED;
 }
 if (callback == nullptr) {
  return USER_SERVICE_ERROR_INVALID_ARGUMENT;
 }
 std::vector<std::shared_ptr<RegisteredEventCallback>> removed;
 {
  std::unique_lock lock(eventCallbacksMutex);
  if (!initialized.load()) {
   return USER_SERVICE_ERROR_NOT_INITIALIZED;
  }
  for (const auto& entry : eventCallbacks) {
   if (entry->callback == callback && entry->userData == userData) {
    std::lock_guard entryLock(entry->mutex);
    entry->active = false;
    removed.push_back(entry);
   }
  }
 }
 for (const auto& entry : removed) {
  DeactivateAndWait(entry);
 }
 RemoveDrainedCallbacks();
 return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceGetTopMenuNotificationFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSundayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistAudiogramData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSaveDataAutoSync(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcFridayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsTtsSpeed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartyChatTranscriptionFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSaturdayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartySettingFlags(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetImeInputSettings(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsRecDisabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetA11yControllerAName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventFilterTeamEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerBDeviceUniqueIds(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraEliminationLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVibrationStrength(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpAccountUpgradeFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetDeletedUserInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSecureHomeDirectory(std::uint32_t user_id, char* path, std::size_t size) {
    if (path == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    char local[512];
    const int result = std::snprintf(local, sizeof(local), "/%s/priv/home/%08x", sceKernelGetFsSandboxRandomWord(), user_id);
    if (result < 0 || static_cast<std::size_t>(result) >= size) return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    const std::size_t copyLength = static_cast<std::size_t>(result) + 1;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(path, local, copyLength);
    errno = savedErrno;
    if (!copied) return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    return USER_SERVICE_OK;
}

}

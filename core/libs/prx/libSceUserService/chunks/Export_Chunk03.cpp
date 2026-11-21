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


int APS5_VABI sceUserServiceGetGlsMessageFilterLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetImePointerMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalDvd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSparkEqProfile(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcTuesdayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceRecognitionTutorialState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetFriendFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventCalendarType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSaturdayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountIdParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistUsePreset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetUserGroupName(int groupIndex, char* name, std::size_t size) {
    if (name == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (size < 129) {
        return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    }
    if (size != 129) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (!UserGroupSlotExists(groupIndex)) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    char output[129]{};
    char path[64];
    std::snprintf(path, sizeof(path), "/system_data/priv/home/user_group_name.dat.%d", groupIndex);
    const int descriptor = sceKernelOpen(path, 0, 0);
    if (descriptor >= 0) {
        const std::int64_t bytesRead = sceKernelRead(descriptor, output, 128);
        sceKernelClose(descriptor);
        if (bytesRead <= 0) std::snprintf(output, sizeof(output), "User Group %d", groupIndex + 1);
    } else {
        std::snprintf(output, sizeof(output), "User Group %d", groupIndex + 1);
    }
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(name, output, sizeof(output));
    errno = savedErrno;
    return copied ? USER_SERVICE_OK : USER_SERVICE_ERROR_INVALID_ARGUMENT;
}

int APS5_VABI sceUserServiceSetAutoLoginEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIPDLeft(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHintShowFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2CinematicAudioRotation(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetLoginFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcThursdayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSundayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceRecognitionLastUsedOsk(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsFacebookEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetShareStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentEnableFeedback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsKratosUser(void) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceUserServiceSetCreatedVersion(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSharePlayFlags(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistUsePreset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyChatTranscriptionLanguageType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetA11yControllerADeviceUniqueIds(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerLedBrightness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetKeyRepeatStartingTime(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerAlreadyNoticed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsAccessTokenUstream(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetProControllerUniqueId3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHrtf(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsTtsFlags(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpAge(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsFloatingMessage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPlayTogetherFlags(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2HideCameraImage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsAccessTokenTwitch(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetCaptureStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpSubAccount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyChatTranscriptionFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsMuteEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBcTitle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2Vibration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIsQuickSignup(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings_2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHomeDirectory(std::uint32_t user_id, char* path, std::size_t size) {
    if (path == nullptr) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    char local[64];
    const int result = std::snprintf(local, sizeof(local), "/user/home/%08x", user_id);
    if (result < 0 || static_cast<std::size_t>(result) >= size) {
        return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    }
    const std::size_t copyLength = static_cast<std::size_t>(result) + 1;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(path, local, copyLength);
    errno = savedErrno;
    if (!copied) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceSetPbtcSundayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIPDLeft(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetKratosPrimaryUser(int* userId) {
 if (userId == nullptr) {
  NotImplemented_nid_no_patch(__func__);
 }
 return USER_SERVICE_ERROR_NOT_SUPPORTED;
}

int APS5_VABI sceUserServiceLogout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpDateOfBirth(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcWednesdayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSparkEqParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventSortEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNotificationSettings_1(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcMondayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsFakePlus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcTuesdayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartyMuteList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpNpId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountIdClockType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerBtAddress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSparkEqIndex(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPsnPasswordForDebug(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalGameWhiteList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCommunityId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceUpdateCronosCountryList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGazeCalibData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpAuthErrorFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistUseAudiogramData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAccessibilityKeyremapEnable(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

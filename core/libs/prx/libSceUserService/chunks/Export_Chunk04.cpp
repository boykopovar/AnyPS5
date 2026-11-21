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


int APS5_VABI sceUserServiceSetPbtcFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcFridayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsStreamingMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcFridayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerUniqueId1(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentStatusBitFactory(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTeamShowAboutTeam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventSortEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLfpsSortOrder(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistMakeVoicesClearer(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentEnableByButton(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountIdClockType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceTerminateForShellCore(void) {
 return sceUserServiceTerminate();
}

int APS5_VABI sceUserServiceSetParentalMorpheus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSystemLoggerHashedAccountId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetCaptureStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAgeVerified(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraBgFilter(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsTtsVolume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsAnonymousUserId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraReflection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmuZoom(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBcTitle(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcFridayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetLoginFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistAdjustmentLevelLeft(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetFaceRecognitionDeleteCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTriggerEffectStrength(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsTwitterEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSaturdayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHintShowFlag3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentSessionTimeout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalContentControl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraEffect(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcTuesdayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPartyMuteListA(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraEliminationLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastersCommentColor(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalBrowser(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistSeparateLR(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsUserStorageAccountBound(void) {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceUserServiceSetHmd2Brightness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraChromaKeyLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerUniqueId2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBroadcastChannel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraEdgeLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraPosition(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerAName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcWednesdayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetMonoOutput(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyChatTranscriptionVoiceType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTopMenuTutorialFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIPDRight(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcSaturdayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetImeLastUnit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpNpId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTriggerEffectStrength(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsRecievedMessageHidden(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentSessionTimeout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLanguageFilter(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2Vibration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsLoggedIn(int userId) {
 if (!initialized.load()) {
  return USER_SERVICE_ERROR_NOT_INITIALIZED;
 }
 return IsUserLoggedIn(userId) ? 1 : 0;
}

int APS5_VABI sceUserServiceGetPbtcMondayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAppSortOrder(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetMouseHandType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsQuickSignup(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

bool APS5_VABI sceUserServiceIsKratosPrimaryUser(int) {
 return false;
}

int APS5_VABI sceUserServiceSetVoiceAgentEnableWup(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraDepthLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmuBrightness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsHintFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsLoggedInWithoutLock(int userId) {
 if (!initialized.load()) {
  return USER_SERVICE_ERROR_NOT_INITIALIZED;
 }
 return IsUserLoggedIn(userId) ? 1 : 0;
}

int APS5_VABI sceUserServiceSetShareStatus3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsTtsVolume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetMicLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetImeLastUnit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcThursdayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSaveDataAutoUpload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAccessibilityKeyremapDataAston(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVolumeForSidetone(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetProControllerUniqueId2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetFriendCustomListLastFocus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2GazeTarget(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetKeyRepeatSpeed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalDvd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

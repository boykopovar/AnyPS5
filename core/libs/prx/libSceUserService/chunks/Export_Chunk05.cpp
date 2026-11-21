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


int APS5_VABI sceUserServiceGetImePointerMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVolumeForController(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastChannel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountIdParam(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHintShowFlag2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistPresetType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings_3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsAnonymousUserId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetA11yControllerBDeviceUniqueIds(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentEnableWup(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpDateOfBirth(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetKeyboardType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetParentalGame(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistSeparateLR(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentTtsVolume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcMondayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpSubAccount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetShareStatus2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCamCrop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceRecognitionLastUsedOsk(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceRecognitionTutorialState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality5(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistUseAudiogramData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIPDRight(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcPlayTimeLastUpdated(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcTuesdayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationBehavior(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPadProfileDataHuntList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHearingAssistAudiogramData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHintShowFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTopMenuLimitItem(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSundayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalBdAge(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSaveDataSort(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHrtf(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAppearOfflineSetting(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentLanguage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIsRemotePlayAllowed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsStreamingMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetChatStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentStatusBit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpCountryCode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetParentalGameWhiteList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2ScreenSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTriggerEffectEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerInitialSetupProgress(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVolumeForGenericUSB(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetControlCenterFunctionControlSettings(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsInitSpectating(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsMuteEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpLanguageCode2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetFriendCustomListLastFocus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPartyMuteList(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAccessibilityKeyremapData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcFridayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcMondayHoursEnd(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetControlCenterStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSparkEqProfile(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsTtsPitch(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBroadcastService(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAutoLoginEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcSaturdayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings_1(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2MuteAssign(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPasscode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetMousePointerSpeed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastUiLayout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSharePlayFlags(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetIsFakePlus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAccountRemarks(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetAccessibilityZoom(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSystemLoggerHashedAccountIdTtl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSaveDataSort(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPlayTogetherFlags(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHearingAssistAdjustmentLevelRight(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsTtsSpeed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpEnv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetDiscPlayerFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2PlayAreaShape(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsCameraHidden(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetUserStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventFilterTeamEvent(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPsCloudFeedbackTime(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetUserGroupIndex(int index) {
    if (index != -1 && (index < 0 || index > 128 || !UserGroupSlotExists(index))) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    const int descriptor = sceKernelOpen("/system_data/priv/home/user_list.dat", 2, 0);
    if (descriptor < 0) {
        return descriptor;
    }
    const std::int32_t storedIndex = index == -1 ? 0x7fffffff : index;
    const std::int64_t bytesWritten = sceKernelPwrite(descriptor, &storedIndex, sizeof(storedIndex), 0x44);
    sceKernelClose(descriptor);
    if (bytesWritten < 0) {
        return static_cast<int>(bytesWritten);
    }
    return bytesWritten == sizeof(storedIndex) ? USER_SERVICE_OK : SCE_KERNEL_ERROR_EIO;
}

}

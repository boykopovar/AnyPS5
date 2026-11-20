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


int APS5_VABI sceUserServiceGetHearingAssistAdjustmentLevelRight(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPlatformPrivacyWs1Internal(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsRecDisabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraBrightness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpLanguageCode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcThursdayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGazeActivate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPlatformPrivacyWs1(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVolumeForSidetone(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSaveDataTutorialFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcPlayTime(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastService(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsFakePremium(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality5(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetSaveDataAutoUpload(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetSaveDataTutorialFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality4(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpLanguageCode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcThursdayDuration(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetFaceRecognitionDeleteCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentEnable(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventCalendarType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetProControllerAlreadyNoticed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsTtsFlags(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsBroadcastersComment(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcTzOffset(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcPlayTime(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetVoiceAgentStatusBit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetEventUiFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsBroadcastUiLayout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVibrationEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcAdditionalTime(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceIsSharePlayClientUser(std::int32_t userId) {
    return (static_cast<std::uint32_t>(userId) & 0xf0000000u) == 0x30000000u ? 1 : 0;
}

int APS5_VABI sceUserServiceGetMonoOutput(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetNpLoginId(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetShareStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsIsRecievedMessageHidden(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsLiveQuality2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2GazeActivate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsCameraPosition(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsMessageFilterLevel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetUserGroupNameList(int groupIndex, char* nameList, std::size_t size,
                                                std::int32_t* count) {
    const int savedErrno = errno;
    if (nameList == nullptr || count == nullptr) {
        errno = savedErrno;
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    if (size < 0x110) {
        errno = savedErrno;
        return USER_SERVICE_ERROR_BUFFER_TOO_SHORT;
    }
    const bool groupValid = groupIndex >= -1 && groupIndex <= 128 && (groupIndex < 0 || UserGroupSlotExists(groupIndex));
    errno = savedErrno;
    if (size != 0x110 || !groupValid) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    const bool countWritable = GuestWritable_nid_no_patch(count, sizeof(*count));
    const bool namesWritable = GuestWritable_nid_no_patch(nameList, size);
    errno = savedErrno;
    if (!countWritable || !namesWritable) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    char path[64];
    if (groupIndex < 0) {
        std::snprintf(path, sizeof(path), "/system_data/priv/home/user_list.dat");
    } else {
        std::snprintf(path, sizeof(path), "/system_data/priv/home/user_list.dat.%d", groupIndex);
    }
    std::array<std::int32_t, 16> userIds;
    userIds.fill(USER_SERVICE_USER_ID_INVALID);
    const int descriptor = sceKernelOpen(path, 0, 0);
    if (descriptor < 0) {
        errno = savedErrno;
        return descriptor;
    }
    const std::int64_t bytesRead = sceKernelRead(descriptor, userIds.data(), 0x40);
    sceKernelClose(descriptor);
    if (bytesRead < 0) {
        errno = savedErrno;
        return static_cast<int>(bytesRead);
    }
    std::array<char, 0x110> localNames{};
    std::int32_t localCount = 0;
    for (std::size_t index = 0; index < userIds.size() && userIds[index] != USER_SERVICE_USER_ID_INVALID; ++index) {
        char userName[USER_SERVICE_MAX_USER_NAME_LENGTH + 1]{};
        const int result = sceUserServiceGetUserName(userIds[index], userName, sizeof(userName));
        if (result != USER_SERVICE_OK) {
            errno = savedErrno;
            return result;
        }
        std::memcpy(localNames.data() + index * sizeof(userName), userName, sizeof(userName));
        ++localCount;
    }
    const bool countCopied = GuestCopyOut_nid_no_patch(count, &localCount, sizeof(localCount));
    const bool namesCopied = GuestCopyOut_nid_no_patch(nameList, localNames.data(), localNames.size());
    errno = savedErrno;
    if (!countCopied || !namesCopied) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return USER_SERVICE_OK;
}

int APS5_VABI sceUserServiceSetVoiceAgentEnableFeedback(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGazeActivate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsTwitterEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetTopMenuLimitItem(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNpAuthErrorFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsIsCameraHidden(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetMousePointerSpeed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetFaceRecognitionRegisterCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2PlayAreaShape(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetUserName(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetA11yControllerADeviceUniqueIds(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsFloatingMessage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceInitializeForShellCore(void*) {
 return MarkInitialized();
}

int APS5_VABI sceUserServiceGetGlsBcTags(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcMode(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetImeInputType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetProControllerUniqueId3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmd2GazeActivate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetCaptureStatus3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsCameraReflection(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetNotificationSettings_2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetFaceRecognitionRegisterCount(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetKeyRepeatStartingTime(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsRemotePlayAllowed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPbtcMondayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsLiveQuality3(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPsCloudFeedbackTime(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentTtsSpeed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetVoiceAgentEnable(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcWednesdayHoursStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetTopMenuTutorialFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetIsPersonalizedHrtfEnabled(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetPadSpeakerVolume(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetAccessibilityKeyremapEnableAston(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetHmd2ScreenSize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetGlsInitSpectating(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetLastLoginOrder(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetGlsTtsPitch(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetMouseHandType(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetEventUiFlag(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetImeInputSettings(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetProControllerUniqueId1(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceGetPbtcPlayTimeLastUpdated(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceSetHmuBrightness(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceUserServiceRegisterEventCallback(UserServiceEventCallback callback, void* userData) {
 if (!initialized.load()) {
  return USER_SERVICE_ERROR_NOT_INITIALIZED;
 }
 if (callback == nullptr) {
  return USER_SERVICE_OK;
 }
 std::unique_lock lock(eventCallbacksMutex);
 if (!initialized.load()) {
  return USER_SERVICE_ERROR_NOT_INITIALIZED;
 }
 for (const auto& entry : eventCallbacks) {
  std::lock_guard entryLock(entry->mutex);
  if (entry->active && entry->callback == callback && entry->userData == userData) {
   return USER_SERVICE_OK;
  }
 }
 auto entry = std::make_shared<RegisteredEventCallback>();
 entry->callback = callback;
 entry->userData = userData;
 eventCallbacks.push_back(std::move(entry));
 return USER_SERVICE_OK;
}

}

#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Fn>
void RequireThrows(Fn fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

class GuardedPage {
public:
    std::size_t size;
    unsigned char* data;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (data == nullptr) {
            std::abort();
        }
        DWORD previous = 0;
        if (VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) == 0) {
            std::abort();
        }
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) {
            std::abort();
        }
        size = static_cast<std::size_t>(pageSize);
        data = static_cast<unsigned char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (data == MAP_FAILED) {
            std::abort();
        }
        if (mprotect(data + size, size, PROT_NONE) != 0) {
            std::abort();
        }
#endif
    }

    void SetFirstPageReadOnly(bool readOnly) {
#ifdef _WIN32
        DWORD previous = 0;
        const DWORD protect = readOnly ? PAGE_READONLY : PAGE_READWRITE;
        if (VirtualProtect(data, size, protect, &previous) == 0) {
            std::abort();
        }
#else
        const int protect = readOnly ? PROT_READ : (PROT_READ | PROT_WRITE);
        if (mprotect(data, size, protect) != 0) {
            std::abort();
        }
#endif
    }

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
};

template <typename Predicate>
void WaitUntil(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate()) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

struct CallbackEventData {
    std::atomic<int> callCount{};
    std::atomic<uint32_t> lastEventType{0xFFFFFFFFu};
    std::atomic<int> lastUserId{-1};
};

void APS5_VABI TestEventCallback(const SceUserServiceEvent* event, void* userData) {
    if (event != nullptr && userData != nullptr) {
        auto* data = static_cast<CallbackEventData*>(userData);
        data->lastEventType.store(event->event_type);
        data->lastUserId.store(event->user_id);
        data->callCount.fetch_add(1);
    }
}

void TestPreInitializationBounds() {
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + i) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + i) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    }

    const int invalidIds[] = {
        0,
        1,
        -1,
        USER_SERVICE_USER_ID_INVALID,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        0x20000000,
        static_cast<int>(0xFFFFFFFFu)
    };
    for (int invalidId : invalidIds) {
        Require(sceUserServiceIsLoggedIn(invalidId) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        Require(sceUserServiceIsLoggedInWithoutLock(invalidId) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    }

    UserServiceLoginUserIdList list{};
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEventVsh(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceRegisterEventCallback(TestEventCallback, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(TestEventCallback, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestMultiLoginStateTransitions() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 1);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID) == 1);

    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + i) == 0);
        Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + i) == 0);
    }

    const int invalidIds[] = {
        0,
        1,
        -1,
        USER_SERVICE_USER_ID_INVALID,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        0x20000000,
        static_cast<int>(0xFFFFFFFFu)
    };
    for (int invalidId : invalidIds) {
        Require(sceUserServiceIsLoggedIn(invalidId) == 0);
        Require(sceUserServiceIsLoggedInWithoutLock(invalidId) == 0);
    }

    UserServiceLoginUserIdList list{};
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(list.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, true);
        Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + i) == 1);
        Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + i) == 1);
    }

    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(list.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
    }

    UserServiceSetUserConnected_nid_postfix(-1, true);
    UserServiceSetUserConnected_nid_postfix(4, true);
    UserServiceSetUserConnected_nid_postfix(100, true);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(list.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
    }
}

void TestLogoutSequencing() {
    UserServiceLoginUserIdList list{};

    UserServiceSetUserConnected_nid_postfix(3, false);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 3) == 0);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + 3) == 0);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(list.user_id[2] == USER_SERVICE_INITIAL_USER_ID + 2);
    Require(list.user_id[3] == USER_SERVICE_USER_ID_INVALID);

    UserServiceSetUserConnected_nid_postfix(2, false);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 2) == 0);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + 2) == 0);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(list.user_id[2] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[3] == USER_SERVICE_USER_ID_INVALID);

    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 1) == 0);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + 1) == 0);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[2] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[3] == USER_SERVICE_USER_ID_INVALID);

    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 0);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID) == 0);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(list.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    UserServiceSetUserConnected_nid_postfix(2, true);
    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[2] == USER_SERVICE_INITIAL_USER_ID + 2);
    Require(list.user_id[3] == USER_SERVICE_USER_ID_INVALID);

    UserServiceSetUserConnected_nid_postfix(1, true);
    UserServiceSetUserConnected_nid_postfix(3, true);
    UserServiceSetUserConnected_nid_postfix(1, false);
    UserServiceSetUserConnected_nid_postfix(3, false);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 1);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 1) == 0);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 2) == 1);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 3) == 0);

    UserServiceSetUserConnected_nid_postfix(2, false);
}

void TestEventQueueAndVshNotifications() {
    SceUserServiceEvent event{};
    while (sceUserServiceGetEvent(&event) == USER_SERVICE_OK) {}
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NO_EVENT);

    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetEventVsh(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    UserServiceSetUserConnected_nid_postfix(1, true);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_OK);
    Require(event.event_type == 1u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(2, true);
    UserServiceSetUserConnected_nid_postfix(2, false);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(1, true);
    UserServiceSetUserConnected_nid_postfix(2, true);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID + 2);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(1, false);
    UserServiceSetUserConnected_nid_postfix(2, false);
    while (sceUserServiceGetEvent(&event) == USER_SERVICE_OK) {}
}

void TestEventFaultRetention() {
    SceUserServiceEvent drain{};
    while (sceUserServiceGetEvent(&drain) == USER_SERVICE_OK) {}
    Require(sceUserServiceGetEvent(&drain) == USER_SERVICE_ERROR_NO_EVENT);

    GuardedPage guard;
    auto* validEvent = reinterpret_cast<SceUserServiceEvent*>(guard.data);

    UserServiceSetUserConnected_nid_postfix(1, true);
    RequireThrows([&] {
        sceUserServiceGetEvent(reinterpret_cast<SceUserServiceEvent*>(guard.data + guard.size));
    });
    RequireThrows([&] {
        sceUserServiceGetEvent(reinterpret_cast<SceUserServiceEvent*>(guard.data + guard.size - 4));
    });
    guard.SetFirstPageReadOnly(true);
    RequireThrows([&] {
        sceUserServiceGetEvent(validEvent);
    });
    guard.SetFirstPageReadOnly(false);

    std::memset(validEvent, 0xA5, sizeof(*validEvent));
    Require(sceUserServiceGetEvent(validEvent) == USER_SERVICE_OK);
    Require(validEvent->event_type == 0u);
    Require(validEvent->user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceGetEvent(validEvent) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(1, false);
    RequireThrows([&] {
        sceUserServiceGetEventVsh(reinterpret_cast<SceUserServiceEvent*>(guard.data + guard.size));
    });
    RequireThrows([&] {
        sceUserServiceGetEventVsh(reinterpret_cast<SceUserServiceEvent*>(guard.data + guard.size - 4));
    });
    guard.SetFirstPageReadOnly(true);
    RequireThrows([&] {
        sceUserServiceGetEventVsh(validEvent);
    });
    guard.SetFirstPageReadOnly(false);

    std::memset(validEvent, 0xA5, sizeof(*validEvent));
    Require(sceUserServiceGetEventVsh(validEvent) == USER_SERVICE_OK);
    Require(validEvent->event_type == 1u);
    Require(validEvent->user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceGetEventVsh(validEvent) == USER_SERVICE_ERROR_NO_EVENT);
}

void TestEventCallbackRegistrationAndDelivery() {
    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    CallbackEventData data1;
    Require(sceUserServiceRegisterEventCallback(TestEventCallback, &data1) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(TestEventCallback, &data1) == USER_SERVICE_OK);

    CallbackEventData data2;
    Require(sceUserServiceUnregisterEventCallback(TestEventCallback, &data2) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(1, true);
    WaitUntil([&] { return data1.callCount.load() >= 1; });
    Require(data1.lastEventType.load() == 0u);
    Require(data1.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);

    UserServiceSetUserConnected_nid_postfix(1, false);
    WaitUntil([&] { return data1.callCount.load() >= 2; });
    Require(data1.lastEventType.load() == 1u);
    Require(data1.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);

    Require(sceUserServiceUnregisterEventCallback(TestEventCallback, &data1) == USER_SERVICE_OK);

    const int callsBefore = data1.callCount.load();
    UserServiceSetUserConnected_nid_postfix(1, true);
    UserServiceSetUserConnected_nid_postfix(1, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(data1.callCount.load() == callsBefore);

    SceUserServiceEvent event{};
    while (sceUserServiceGetEvent(&event) == USER_SERVICE_OK) {}
}

void TestMultiCallbackSelectiveUnregister() {
    CallbackEventData pair1Data;
    CallbackEventData pair2Data;
    Require(sceUserServiceRegisterEventCallback(TestEventCallback, &pair1Data) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(TestEventCallback, &pair2Data) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(2, true);
    WaitUntil([&] { return pair1Data.callCount.load() >= 1 && pair2Data.callCount.load() >= 1; });
    Require(pair1Data.callCount.load() == 1);
    Require(pair1Data.lastEventType.load() == 0u);
    Require(pair1Data.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 2);
    Require(pair2Data.callCount.load() == 1);
    Require(pair2Data.lastEventType.load() == 0u);
    Require(pair2Data.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 2);

    Require(sceUserServiceUnregisterEventCallback(TestEventCallback, &pair1Data) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(2, false);
    WaitUntil([&] { return pair2Data.callCount.load() >= 2; });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(pair1Data.callCount.load() == 1);
    Require(pair2Data.callCount.load() == 2);
    Require(pair2Data.lastEventType.load() == 1u);
    Require(pair2Data.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 2);

    Require(sceUserServiceUnregisterEventCallback(TestEventCallback, &pair2Data) == USER_SERVICE_OK);

    SceUserServiceEvent event{};
    while (sceUserServiceGetEvent(&event) == USER_SERVICE_OK) {}
}

void TestLifecycleReinitialization() {
    UserServiceSetUserConnected_nid_postfix(0, true);
    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, false);
    }

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceRegisterEventCallback(TestEventCallback, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(TestEventCallback, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

} // namespace

int main() {
    TestPreInitializationBounds();
    TestMultiLoginStateTransitions();
    TestLogoutSequencing();
    TestEventQueueAndVshNotifications();
    TestEventFaultRetention();
    TestEventCallbackRegistrationAndDelivery();
    TestMultiCallbackSelectiveUnregister();
    TestLifecycleReinitialization();
    return 0;
}

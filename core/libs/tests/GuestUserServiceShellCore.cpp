#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

template <typename Predicate>
void WaitUntil(Predicate predicate) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!predicate()) {
        Require(std::chrono::steady_clock::now() < deadline);
        std::this_thread::yield();
    }
}

class GuardedPage {
public:
    std::size_t size{};
    unsigned char* data{};

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(
            VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
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
        data = static_cast<unsigned char*>(
            mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (data == MAP_FAILED) {
            std::abort();
        }
        if (mprotect(data + size, size, PROT_NONE) != 0) {
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

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

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

    SceUserServiceEvent* NoAccessEventPtr() const {
        return reinterpret_cast<SceUserServiceEvent*>(data + size);
    }

    SceUserServiceEvent* StraddlingEventPtr() const {
        return reinterpret_cast<SceUserServiceEvent*>(data + size - sizeof(SceUserServiceEvent) / 2);
    }

    SceUserServiceEvent* FirstPageEventPtr() const {
        return reinterpret_cast<SceUserServiceEvent*>(data);
    }
};

struct CallbackState {
    std::atomic<int> callCount{0};
    std::atomic<std::uint32_t> lastEventType{0xFFFFFFFFu};
    std::atomic<int> lastUserId{-1};
};

void APS5_VABI TestEventCallback(const SceUserServiceEvent* event, void* userData) {
    if (event != nullptr && userData != nullptr) {
        auto* state = static_cast<CallbackState*>(userData);
        state->lastEventType.store(event->event_type);
        state->lastUserId.store(event->user_id);
        state->callCount.fetch_add(1);
    }
}

void VerifyNotInitializedState(GuardedPage& guard) {
    const int queryIds[] = {
        USER_SERVICE_INITIAL_USER_ID,
        USER_SERVICE_INITIAL_USER_ID + 1,
        USER_SERVICE_INITIAL_USER_ID + 2,
        USER_SERVICE_INITIAL_USER_ID + 3,
        USER_SERVICE_USER_ID_INVALID,
        0,
        -1,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS
    };
    for (int id : queryIds) {
        Require(sceUserServiceIsLoggedIn(id) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        Require(sceUserServiceIsLoggedInWithoutLock(id) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    }

    UserServiceLoginUserIdList list{};
    std::memset(&list, 0xAA, sizeof(list));
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(list.user_id[i] == static_cast<int>(0xAAAAAAAAu));
    }
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    SceUserServiceEvent event{};
    std::memset(&event, 0xAA, sizeof(event));
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(event.event_type == 0xAAAAAAAAu);
    Require(event.user_id == static_cast<int>(0xAAAAAAAAu));
    Require(sceUserServiceGetEvent(guard.NoAccessEventPtr()) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(guard.StraddlingEventPtr()) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    guard.SetFirstPageReadOnly(true);
    Require(sceUserServiceGetEvent(guard.FirstPageEventPtr()) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    guard.SetFirstPageReadOnly(false);

    std::memset(&event, 0xAA, sizeof(event));
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(event.event_type == 0xAAAAAAAAu);
    Require(event.user_id == static_cast<int>(0xAAAAAAAAu));
    Require(sceUserServiceGetEventVsh(guard.NoAccessEventPtr()) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEventVsh(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    CallbackState state;
    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceRegisterEventCallback(TestEventCallback, &state) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(TestEventCallback, &state) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceTerminateForShellCore() == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void VerifyInitialUserAndLoginList() {
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 1);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID) == 1);
    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + i) == 0);
        Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + i) == 0);
    }

    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    UserServiceLoginUserIdList list{};
    std::memset(&list, 0x55, sizeof(list));
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(list.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }
}

template <typename Getter>
void RequireThrowsOnUnwritableEvent(Getter getter, SceUserServiceEvent* ptr) {
    bool threw = false;
    try {
        getter(ptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

void TestEmptyQueueAndUnwritableRetention(GuardedPage& guard) {
    Require(sceUserServiceGetEvent(guard.NoAccessEventPtr()) == USER_SERVICE_ERROR_NO_EVENT);
    Require(sceUserServiceGetEvent(guard.StraddlingEventPtr()) == USER_SERVICE_ERROR_NO_EVENT);
    guard.SetFirstPageReadOnly(true);
    Require(sceUserServiceGetEvent(guard.FirstPageEventPtr()) == USER_SERVICE_ERROR_NO_EVENT);
    guard.SetFirstPageReadOnly(false);

    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetEventVsh(guard.NoAccessEventPtr()) == USER_SERVICE_ERROR_NO_EVENT);
    Require(sceUserServiceGetEventVsh(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    UserServiceSetUserConnected_nid_postfix(1, true);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 1) == 1);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + 1) == 1);

    RequireThrowsOnUnwritableEvent(sceUserServiceGetEvent, guard.NoAccessEventPtr());
    RequireThrowsOnUnwritableEvent(sceUserServiceGetEvent, guard.StraddlingEventPtr());
    guard.SetFirstPageReadOnly(true);
    RequireThrowsOnUnwritableEvent(sceUserServiceGetEvent, guard.FirstPageEventPtr());
    guard.SetFirstPageReadOnly(false);

    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 1) == 0);
    RequireThrowsOnUnwritableEvent(sceUserServiceGetEventVsh, guard.NoAccessEventPtr());

    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_OK);
    Require(event.event_type == 1u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NO_EVENT);
}

void TestShellCoreEventCallbacks(GuardedPage& guard) {
    Require(sceUserServiceInitializeForShellCore(
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(0x1234))) == USER_SERVICE_OK);
    Require(sceUserServiceInitializeForShellCore(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);

    VerifyInitialUserAndLoginList();

    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NO_EVENT);

    TestEmptyQueueAndUnwritableRetention(guard);

    CallbackState state;
    CallbackState unusedState;
    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(nullptr, &state) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceUnregisterEventCallback(nullptr, &state) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceRegisterEventCallback(TestEventCallback, &state) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(TestEventCallback, &state) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(TestEventCallback, &unusedState) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(1, true);
    WaitUntil([&] { return state.callCount.load() >= 1; });
    Require(state.callCount.load() == 1);
    Require(state.lastEventType.load() == 0u);
    Require(state.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);

    UserServiceSetUserConnected_nid_postfix(1, false);
    WaitUntil([&] { return state.callCount.load() >= 2; });
    Require(state.callCount.load() == 2);
    Require(state.lastEventType.load() == 1u);
    Require(state.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);

    while (sceUserServiceGetEvent(&event) == USER_SERVICE_OK) {}

    UserServiceSetUserConnected_nid_postfix(2, true);
    UserServiceSetUserConnected_nid_postfix(2, false);
    Require(sceUserServiceTerminateForShellCore() == USER_SERVICE_OK);

    const int callsAfterTerminate = state.callCount.load();
    VerifyNotInitializedState(guard);

    Require(sceUserServiceInitializeForShellCore(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(1, true);
    UserServiceSetUserConnected_nid_postfix(1, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(state.callCount.load() == callsAfterTerminate);

    Require(sceUserServiceTerminateForShellCore() == USER_SERVICE_OK);
    VerifyNotInitializedState(guard);
}

void TestCrossLifecycleInterop(GuardedPage& guard) {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    VerifyInitialUserAndLoginList();
    SceUserServiceEvent event{};
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);
    Require(sceUserServiceTerminateForShellCore() == USER_SERVICE_OK);
    VerifyNotInitializedState(guard);

    Require(sceUserServiceInitializeForShellCore(nullptr) == USER_SERVICE_OK);
    VerifyInitialUserAndLoginList();
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0u);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NO_EVENT);
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    VerifyNotInitializedState(guard);
}

}

int main() {
    GuardedPage guard;
    VerifyNotInitializedState(guard);
    TestShellCoreEventCallbacks(guard);
    TestCrossLifecycleInterop(guard);
    std::puts("UserService shell-core lifecycle tests passed");
    return 0;
}

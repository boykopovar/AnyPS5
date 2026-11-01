#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdlib>
#include <cstring>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <initializer_list>
#include <mutex>
#include <thread>

namespace {

void Require(bool value) { if (!value) std::abort(); }

struct CallbackRemovalState {
    int result{USER_SERVICE_ERROR_NOT_INITIALIZED};
    int calls{};
    int* removedCalls{};
};

std::condition_variable callbackChanged;
std::mutex callbackMutex;
bool callbackStarted{};
bool releaseCallback{};

void APS5_VABI CountCallback(const SceUserServiceEvent*, void* userData) {
    ++*static_cast<int*>(userData);
}

void APS5_VABI RemoveNextCallback(const SceUserServiceEvent*, void* userData) {
    auto* state = static_cast<CallbackRemovalState*>(userData);
    ++state->calls;
    state->result = sceUserServiceUnregisterEventCallback(CountCallback, state->removedCalls);
}

void APS5_VABI RemoveSelfCallback(const SceUserServiceEvent*, void* userData) {
    auto* state = static_cast<CallbackRemovalState*>(userData);
    ++state->calls;
    state->result = sceUserServiceUnregisterEventCallback(RemoveSelfCallback, userData);
}

void APS5_VABI BlockingCallback(const SceUserServiceEvent*, void*) {
    std::unique_lock lock(callbackMutex);
    callbackStarted = true;
    callbackChanged.notify_all();
    callbackChanged.wait(lock, [] { return releaseCallback; });
}

void TestCallbackRemoval() {
    CallbackRemovalState state;
    int removedCalls = 0;
    state.removedCalls = &removedCalls;
    Require(sceUserServiceRegisterEventCallback(RemoveNextCallback, &state) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(CountCallback, &removedCalls) == USER_SERVICE_OK);
    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(state.calls == 1 && state.result == USER_SERVICE_OK);
    Require(removedCalls == 0);
    Require(sceUserServiceUnregisterEventCallback(RemoveNextCallback, &state) == USER_SERVICE_OK);

    state = {};
    Require(sceUserServiceRegisterEventCallback(RemoveSelfCallback, &state) == USER_SERVICE_OK);
    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(state.calls == 1 && state.result == USER_SERVICE_OK);
    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(state.calls == 1);
}

void TestTerminateWaitsForCallback() {
    UserServiceSetUserConnected_nid_postfix(0, true);
    {
        std::lock_guard lock(callbackMutex);
        callbackStarted = false;
        releaseCallback = false;
    }
    Require(sceUserServiceRegisterEventCallback(BlockingCallback, nullptr) == USER_SERVICE_OK);
    std::thread notifier([] { UserServiceSetUserConnected_nid_postfix(0, false); });
    {
        std::unique_lock lock(callbackMutex);
        callbackChanged.wait(lock, [] { return callbackStarted; });
    }
    std::atomic<bool> terminateReturned{};
    int terminateResult = USER_SERVICE_ERROR_INVALID_ARGUMENT;
    std::thread terminator([&] {
        terminateResult = sceUserServiceTerminate();
        terminateReturned = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(!terminateReturned.load());
    {
        std::lock_guard lock(callbackMutex);
        releaseCallback = true;
    }
    callbackChanged.notify_all();
    notifier.join();
    terminator.join();
    Require(terminateReturned.load() && terminateResult == USER_SERVICE_OK);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    UserServiceSetUserConnected_nid_postfix(0, true);
}

void RequireNotInitialized() {
    UserServiceLoginUserIdList list{};
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    SceUserServiceEvent event{};
    std::memset(&event, 0xAA, sizeof(event));
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(event.user_id == static_cast<int>(0xAAAAAAAAu));
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void RequireLoginOnce() {
    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0 && event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);
}

int InitializeWithPriority(int priority) {
    const UserServiceInitParam param{priority};
    return sceUserServiceInitialize(&param);
}

void TestInitialUserLogout() {
    SceUserServiceEvent event{};
    UserServiceLoginUserIdList list{};
    int initial = -1;
    Require(sceUserServiceGetInitialUser(&initial) == USER_SERVICE_OK && initial == USER_SERVICE_INITIAL_USER_ID);

    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 1 && event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_USER_ID_INVALID);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0 && event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(0, false);
    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_USER_ID_INVALID);
    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0 && event.user_id == USER_SERVICE_INITIAL_USER_ID);
}

void TestThreadPriority() {
    for (int priority : {0, 0xFF, 0x300, -1}) {
        Require(InitializeWithPriority(priority) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceInitialize2(priority, 1) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
    RequireNotInitialized();

    Require(sceUserServiceInitialize2(0x100, 1) == USER_SERVICE_OK);
    RequireLoginOnce();
    for (int priority : {0x100, 700, 0x2FF}) {
        Require(InitializeWithPriority(priority) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
        Require(sceUserServiceInitialize2(priority, 1) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    }
    for (int priority : {0, 0xFF, 0x300, -1}) {
        Require(InitializeWithPriority(priority) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceInitialize2(priority, 1) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);

    Require(InitializeWithPriority(0x2FF) == USER_SERVICE_OK);
    RequireLoginOnce();
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
}

}

int main() {
    RequireNotInitialized();
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    UserServiceLoginUserIdList list{};
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    RequireLoginOnce();

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
    RequireNotInitialized();

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    RequireLoginOnce();

    UserServiceSetUserConnected_nid_postfix(1, true);
    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0 && event.user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_INITIAL_USER_ID + 1);

    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 1 && event.user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_USER_ID_INVALID);

    for (int i = 0; i < 50; ++i) {
        UserServiceSetUserConnected_nid_postfix(1, true);
        UserServiceSetUserConnected_nid_postfix(1, false);
    }
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(2, true);
    UserServiceSetUserConnected_nid_postfix(2, false);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(2, true);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0 && event.user_id == USER_SERVICE_INITIAL_USER_ID + 2);
    UserServiceSetUserConnected_nid_postfix(2, false);
    UserServiceSetUserConnected_nid_postfix(2, true);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);
    UserServiceSetUserConnected_nid_postfix(2, false);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 1 && event.user_id == USER_SERVICE_INITIAL_USER_ID + 2);

    TestInitialUserLogout();
    TestCallbackRemoval();
    TestTerminateWaitsForCallback();
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);

    TestThreadPriority();
}

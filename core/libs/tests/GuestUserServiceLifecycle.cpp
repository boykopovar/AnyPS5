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
#include <barrier>
#include <source_location>
#include "prx/libSceUserService/Export.cpp"

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "UserService lifecycle failure at line %u\n", location.line());
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

void APS5_VABI CompletionCallback(const SceUserServiceEvent*, void* data) {
    static_cast<std::atomic<bool>*>(data)->store(true);
}

void NotifyAndWait(int index, bool connected) {
    std::atomic<bool> completed{};
    Require(sceUserServiceRegisterEventCallback(CompletionCallback, &completed) == USER_SERVICE_OK);
    UserServiceSetUserConnected_nid_postfix(index, connected);
    WaitUntil([&] { return completed.load(); });
    Require(sceUserServiceUnregisterEventCallback(CompletionCallback, &completed) == USER_SERVICE_OK);
}

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
    NotifyAndWait(0, false);
    Require(state.calls == 1 && state.result == USER_SERVICE_OK);
    Require(removedCalls == 0);
    Require(sceUserServiceUnregisterEventCallback(RemoveNextCallback, &state) == USER_SERVICE_OK);

    state = {};
    Require(sceUserServiceRegisterEventCallback(RemoveSelfCallback, &state) == USER_SERVICE_OK);
    NotifyAndWait(0, true);
    Require(state.calls == 1 && state.result == USER_SERVICE_OK);
    NotifyAndWait(0, false);
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
    std::shared_ptr<RegisteredEventCallback> registration;
    {
        std::shared_lock lock(eventCallbacksMutex);
        registration = eventCallbacks.back();
    }
    UserServiceSetUserConnected_nid_postfix(0, false);
    {
        std::unique_lock lock(callbackMutex);
        callbackChanged.wait(lock, [] { return callbackStarted; });
    }
    std::atomic<bool> unregisterReturned{};
    std::atomic<bool> secondUnregisterReturned{};
    std::thread unregisterer([&] {
        Require(sceUserServiceUnregisterEventCallback(BlockingCallback, nullptr) == USER_SERVICE_OK);
        unregisterReturned = true;
    });
    WaitUntil([&] {
        std::lock_guard lock(registration->mutex);
        return !registration->active;
    });
    {
        std::shared_lock lock(eventCallbacksMutex);
        Require(std::find(eventCallbacks.begin(), eventCallbacks.end(), registration) != eventCallbacks.end());
    }
    std::thread secondUnregisterer([&] {
        const int result = sceUserServiceUnregisterEventCallback(BlockingCallback, nullptr);
        Require(result == USER_SERVICE_OK || result == USER_SERVICE_ERROR_NOT_INITIALIZED);
        secondUnregisterReturned = true;
    });
    std::atomic<bool> terminateReturned{};
    int terminateResult = USER_SERVICE_ERROR_INVALID_ARGUMENT;
    std::thread terminator([&] {
        terminateResult = sceUserServiceTerminate();
        terminateReturned = true;
    });
    WaitUntil([&] {
        std::shared_lock lock(eventCallbacksMutex);
        return terminating || terminateReturned.load();
    });
    Require(!terminateReturned.load());
    Require(!unregisterReturned.load());
    {
        std::lock_guard lock(callbackMutex);
        releaseCallback = true;
    }
    callbackChanged.notify_all();
    unregisterer.join();
    secondUnregisterer.join();
    terminator.join();
    Require(terminateReturned.load() && terminateResult == USER_SERVICE_OK);
    Require(unregisterReturned.load() && secondUnregisterReturned.load());
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    UserServiceSetUserConnected_nid_postfix(0, true);
}

struct CrossUnregisterState {
    std::atomic<int> active{};
    std::atomic<int> calls{};
    UserServiceEventCallback other{};
    CrossUnregisterState* otherState{};
};

void APS5_VABI CrossUnregisterCallback(const SceUserServiceEvent*, void* data) {
    auto& state = *static_cast<CrossUnregisterState*>(data);
    Require(state.active.fetch_add(1) == 0);
    ++state.calls;
    Require(sceUserServiceUnregisterEventCallback(state.other, state.otherState) == USER_SERVICE_OK);
    --state.active;
}

void TestSerializedCrossUnregister() {
    CrossUnregisterState a, b;
    a.other = b.other = CrossUnregisterCallback;
    a.otherState = &b;
    b.otherState = &a;
    Require(sceUserServiceRegisterEventCallback(CrossUnregisterCallback, &a) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(CrossUnregisterCallback, &b) == USER_SERVICE_OK);
    bool next0, next1;
    {
        std::lock_guard lock(userMutex);
        next0 = !userConnected[0];
        next1 = !userConnected[1];
    }
    std::barrier ready(3);
    std::thread first([&] { ready.arrive_and_wait(); UserServiceSetUserConnected_nid_postfix(0, next0); });
    std::thread second([&] { ready.arrive_and_wait(); UserServiceSetUserConnected_nid_postfix(1, next1); });
    ready.arrive_and_wait();
    first.join();
    second.join();
    NotifyAndWait(0, !next0);
    Require(a.calls + b.calls > 0);
    Require(sceUserServiceUnregisterEventCallback(CrossUnregisterCallback, &a) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(CrossUnregisterCallback, &b) == USER_SERVICE_OK);
}

void TestCallbackArguments() {
    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    int calls{};
    Require(sceUserServiceRegisterEventCallback(CountCallback, &calls) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(CountCallback, &calls) == USER_SERVICE_OK);
    bool next;
    {
        std::lock_guard lock(userMutex);
        next = !userConnected[0];
    }
    NotifyAndWait(0, next);
    Require(calls == 1);
    Require(sceUserServiceUnregisterEventCallback(CountCallback, &calls) == USER_SERVICE_OK);
}

void RequireNotInitialized() {
    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
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
    TestCallbackArguments();
    UserServiceSetUserConnected_nid_postfix(0, true);
    TestCallbackRemoval();
    TestTerminateWaitsForCallback();
    TestSerializedCrossUnregister();
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);

    TestThreadPriority();
}

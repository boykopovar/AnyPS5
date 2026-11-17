#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "Testing.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <thread>

namespace {

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
    TestEventCallbackRegistrationAndDelivery();
    TestLifecycleReinitialization();
    return 0;
}
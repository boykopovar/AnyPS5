#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <thread>

namespace {

void Require(bool condition) {
    if (!condition) {
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

struct TestCallbackData {
    std::atomic<int> count{};
    std::atomic<std::uint32_t> lastEventType{0xFFFFFFFFu};
    std::atomic<int> lastUserId{-1};
};

void APS5_VABI PrimaryCallback(const SceUserServiceEvent* event, void* userData) {
    if (event != nullptr && userData != nullptr) {
        auto* data = static_cast<TestCallbackData*>(userData);
        data->lastEventType.store(event->event_type);
        data->lastUserId.store(event->user_id);
        data->count.fetch_add(1);
    }
}

void APS5_VABI SecondaryCallback(const SceUserServiceEvent* event, void* userData) {
    if (event != nullptr && userData != nullptr) {
        auto* data = static_cast<TestCallbackData*>(userData);
        data->lastEventType.store(event->event_type);
        data->lastUserId.store(event->user_id);
        data->count.fetch_add(1);
    }
}

void TestUninitializedEventBounds() {
    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEventVsh(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEventVsh(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceRegisterEventCallback(PrimaryCallback, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(PrimaryCallback, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestInitializationParameters() {
    UserServiceInitParam params{};

    params.priority = 0;
    Require(sceUserServiceInitialize(&params) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    params.priority = USER_SERVICE_THREAD_PRIORITY_MIN - 1;
    Require(sceUserServiceInitialize(&params) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    params.priority = USER_SERVICE_THREAD_PRIORITY_MAX + 1;
    Require(sceUserServiceInitialize(&params) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    params.priority = -1;
    Require(sceUserServiceInitialize(&params) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    params.priority = 1000;
    Require(sceUserServiceInitialize(&params) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    params.priority = USER_SERVICE_THREAD_PRIORITY_MIN;
    Require(sceUserServiceInitialize(&params) == USER_SERVICE_OK);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    Require(sceUserServiceInitialize(&params) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    Require(sceUserServiceInitialize2(USER_SERVICE_THREAD_PRIORITY_MIN, 1) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);

    SceUserServiceEvent initialEvent{};
    Require(sceUserServiceGetEvent(&initialEvent) == USER_SERVICE_OK);
    Require(initialEvent.event_type == 0u);
    Require(initialEvent.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetEvent(&initialEvent) == USER_SERVICE_ERROR_NO_EVENT);
    Require(sceUserServiceGetEventVsh(&initialEvent) == USER_SERVICE_ERROR_NO_EVENT);
}

void TestPostInitNullParameterBounds() {
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetEventVsh(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceRegisterEventCallback(nullptr, nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(nullptr, reinterpret_cast<void*>(0x1234)) == USER_SERVICE_OK);

    Require(sceUserServiceUnregisterEventCallback(nullptr, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceUnregisterEventCallback(nullptr, reinterpret_cast<void*>(0x1234)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceUnregisterEventCallback(PrimaryCallback, nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceUnregisterEventCallback(PrimaryCallback, reinterpret_cast<void*>(0x1234)) == USER_SERVICE_OK);
}

void TestInvalidUserIndexBounds() {
    for (int invalidIndex : {-1000, -100, -1, 4, 5, 100, 1000}) {
        UserServiceSetUserConnected_nid_postfix(invalidIndex, true);
        UserServiceSetUserConnected_nid_postfix(invalidIndex, false);
    }

    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);
}

void TestMultiUserLoginLogoutQueueBounds() {
    SceUserServiceEvent ev{};
    while (sceUserServiceGetEvent(&ev) == USER_SERVICE_OK) {}

    UserServiceSetUserConnected_nid_postfix(1, true);
    UserServiceSetUserConnected_nid_postfix(2, true);
    UserServiceSetUserConnected_nid_postfix(3, true);

    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_OK);
    Require(ev.event_type == 0u && ev.user_id == USER_SERVICE_INITIAL_USER_ID + 1);

    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_OK);
    Require(ev.event_type == 0u && ev.user_id == USER_SERVICE_INITIAL_USER_ID + 2);

    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_OK);
    Require(ev.event_type == 0u && ev.user_id == USER_SERVICE_INITIAL_USER_ID + 3);

    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(3, false);
    UserServiceSetUserConnected_nid_postfix(1, false);
    UserServiceSetUserConnected_nid_postfix(2, false);

    Require(sceUserServiceGetEventVsh(&ev) == USER_SERVICE_OK);
    Require(ev.event_type == 1u && ev.user_id == USER_SERVICE_INITIAL_USER_ID + 3);

    Require(sceUserServiceGetEventVsh(&ev) == USER_SERVICE_OK);
    Require(ev.event_type == 1u && ev.user_id == USER_SERVICE_INITIAL_USER_ID + 1);

    Require(sceUserServiceGetEventVsh(&ev) == USER_SERVICE_OK);
    Require(ev.event_type == 1u && ev.user_id == USER_SERVICE_INITIAL_USER_ID + 2);

    Require(sceUserServiceGetEventVsh(&ev) == USER_SERVICE_ERROR_NO_EVENT);
}

void TestEventCancellationCoalescingBounds() {
    SceUserServiceEvent ev{};
    while (sceUserServiceGetEvent(&ev) == USER_SERVICE_OK) {}

    UserServiceSetUserConnected_nid_postfix(1, true);
    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(2, true);
    UserServiceSetUserConnected_nid_postfix(2, true);
    UserServiceSetUserConnected_nid_postfix(2, false);
    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(2, false);
    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_ERROR_NO_EVENT);
}

void TestCallbackRegistrationAndDeliveryBounds() {
    TestCallbackData dataA;
    TestCallbackData dataB;

    Require(sceUserServiceRegisterEventCallback(PrimaryCallback, &dataA) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(PrimaryCallback, &dataA) == USER_SERVICE_OK);
    Require(sceUserServiceRegisterEventCallback(SecondaryCallback, &dataB) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(1, true);
    WaitUntil([&] { return dataA.count.load() >= 1 && dataB.count.load() >= 1; });
    Require(dataA.lastEventType.load() == 0u);
    Require(dataA.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(dataB.lastEventType.load() == 0u);
    Require(dataB.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);

    Require(sceUserServiceUnregisterEventCallback(PrimaryCallback, &dataA) == USER_SERVICE_OK);

    UserServiceSetUserConnected_nid_postfix(1, false);
    WaitUntil([&] { return dataB.count.load() >= 2; });
    Require(dataB.lastEventType.load() == 1u);
    Require(dataB.lastUserId.load() == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(dataA.count.load() == 1);

    Require(sceUserServiceUnregisterEventCallback(SecondaryCallback, &dataB) == USER_SERVICE_OK);

    SceUserServiceEvent ev{};
    while (sceUserServiceGetEvent(&ev) == USER_SERVICE_OK) {}
}

void TestLifecycleAndReinitializationBounds() {
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);

    SceUserServiceEvent ev{};
    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEventVsh(&ev) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceRegisterEventCallback(PrimaryCallback, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceUnregisterEventCallback(PrimaryCallback, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_OK);
    Require(ev.event_type == 0u && ev.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_ERROR_NO_EVENT);

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
}

} // namespace

int main() {
    TestUninitializedEventBounds();
    TestInitializationParameters();
    TestPostInitNullParameterBounds();
    TestInvalidUserIndexBounds();
    TestMultiUserLoginLogoutQueueBounds();
    TestEventCancellationCoalescingBounds();
    TestCallbackRegistrationAndDeliveryBounds();
    TestLifecycleAndReinitializationBounds();
    return 0;
}

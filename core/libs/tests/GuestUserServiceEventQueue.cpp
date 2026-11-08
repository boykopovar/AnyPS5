#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT = USER_SERVICE_ERROR_INVALID_ARGUMENT;

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestUninitialized() {
    SceUserServiceEvent event{};
    std::memset(&event, 0xCC, sizeof(event));
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestNullArgument() {
    Require(sceUserServiceGetEvent(nullptr) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
}

void TestEventQueueQuery() {
    SceUserServiceEvent event{};
    std::memset(&event, 0x55, sizeof(event));
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(1, true);
    std::memset(&event, 0, sizeof(event));
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.userId == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(event.eventType == USER_SERVICE_EVENT_TYPE_LOGIN);

    std::memset(&event, 0x55, sizeof(event));
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(1, false);
    std::memset(&event, 0, sizeof(event));
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.userId == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(event.eventType == USER_SERVICE_EVENT_TYPE_LOGOUT);

    std::memset(&event, 0x55, sizeof(event));
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);
}

void TestPostTermination() {
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    SceUserServiceEvent event{};
    std::memset(&event, 0xDD, sizeof(event));
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

}

int main() {
    TestUninitialized();
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    TestNullArgument();
    TestEventQueueQuery();
    TestPostTermination();
    return 0;
}

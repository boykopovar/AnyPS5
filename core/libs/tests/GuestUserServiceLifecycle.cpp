#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdlib>
#include <cstring>
#include <initializer_list>

extern "C" {
int APS5_VABI sceUserServiceInitialize(const UserServiceInitParam* params);
int APS5_VABI sceUserServiceInitialize2(std::int32_t priority, KernelCpumask affinityMask);
int APS5_VABI sceUserServiceTerminate(void);
int APS5_VABI sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* user_id_list);
int APS5_VABI sceUserServiceGetEvent(SceUserServiceEvent* event);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

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

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);

    TestThreadPriority();
}

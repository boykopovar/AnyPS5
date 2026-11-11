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

int CountLoggedInUsers(const UserServiceLoginUserIdList& list) {
    int count = 0;
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        if (list.user_id[i] != USER_SERVICE_USER_ID_INVALID) {
            ++count;
        }
    }
    return count;
}

bool AllLoggedInUsersInRange(const UserServiceLoginUserIdList& list) {
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = list.user_id[i];
        if (userId != USER_SERVICE_USER_ID_INVALID) {
            if (userId < USER_SERVICE_INITIAL_USER_ID ||
                userId >= USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS) {
                return false;
            }
        }
    }
    return true;
}

void TestUninitialized() {
    UserServiceLoginUserIdList list{};
    std::memset(&list, 0xAA, sizeof(list));
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(list.user_id[0] == static_cast<int>(0xAAAAAAAAu));
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestNullArgument() {
    Require(sceUserServiceGetLoginUserIdList(nullptr) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
}

void TestDefaultLoginList() {
    int initialUser = -1;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);

    UserServiceLoginUserIdList list{};
    std::memset(&list, 0x55, sizeof(list));
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);

    Require(list.user_id[0] == initialUser);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[2] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[3] == USER_SERVICE_USER_ID_INVALID);

    Require(CountLoggedInUsers(list) == 1);
    Require(AllLoggedInUsersInRange(list));
}

void TestMultiUserLoginList() {
    UserServiceLoginUserIdList list{};

    UserServiceSetUserConnected_nid_postfix(1, true);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(list.user_id[2] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[3] == USER_SERVICE_USER_ID_INVALID);
    Require(CountLoggedInUsers(list) == 2);
    Require(AllLoggedInUsersInRange(list));

    UserServiceSetUserConnected_nid_postfix(2, true);
    UserServiceSetUserConnected_nid_postfix(3, true);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(list.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
        Require(list.user_id[i] >= 0x10000000);
        Require(list.user_id[i] <= 0x10000003);
    }
    Require(CountLoggedInUsers(list) == 4);
    Require(AllLoggedInUsersInRange(list));

    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[1] == 0x10000001);
    Require(list.user_id[2] == 0x10000002);
    Require(list.user_id[3] == 0x10000003);
    Require(CountLoggedInUsers(list) == 3);
    Require(AllLoggedInUsersInRange(list));

    UserServiceSetUserConnected_nid_postfix(1, false);
    UserServiceSetUserConnected_nid_postfix(2, false);
    UserServiceSetUserConnected_nid_postfix(3, false);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(list.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }
    Require(CountLoggedInUsers(list) == 0);
    Require(AllLoggedInUsersInRange(list));

    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[2] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[3] == USER_SERVICE_USER_ID_INVALID);
    Require(CountLoggedInUsers(list) == 1);
    Require(AllLoggedInUsersInRange(list));
}

void TestPostTermination() {
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    UserServiceLoginUserIdList list{};
    std::memset(&list, 0xBB, sizeof(list));
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(list.user_id[0] == static_cast<int>(0xBBBBBBBBu));
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

}

int main() {
    TestUninitialized();
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    TestNullArgument();
    TestDefaultLoginList();
    TestMultiUserLoginList();
    TestPostTermination();
    return 0;
}

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
    int userId = -1;
    Require(sceUserServiceGetForegroundUser(&userId) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestNullArgument() {
    Require(sceUserServiceGetForegroundUser(nullptr) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
}

void TestForegroundUserQuery() {
    int userId = -1;
    Require(sceUserServiceGetForegroundUser(&userId) == USER_SERVICE_OK);
    Require(userId == USER_SERVICE_INITIAL_USER_ID);
    Require(userId >= 0x10000000);
    Require(userId <= 0x10000003);

    UserServiceSetUserConnected_nid_postfix(1, true);
    Require(sceUserServiceGetForegroundUser(&userId) == USER_SERVICE_OK);
    Require(userId == USER_SERVICE_INITIAL_USER_ID);

    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceGetForegroundUser(&userId) == USER_SERVICE_OK);
    Require(userId == 0x10000001);

    UserServiceSetUserConnected_nid_postfix(0, true);
    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceGetForegroundUser(&userId) == USER_SERVICE_OK);
    Require(userId == USER_SERVICE_INITIAL_USER_ID);
}

void TestPostTermination() {
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    int userId = -1;
    Require(sceUserServiceGetForegroundUser(&userId) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

}

int main() {
    TestUninitialized();
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    TestNullArgument();
    TestForegroundUserQuery();
    TestPostTermination();
    return 0;
}

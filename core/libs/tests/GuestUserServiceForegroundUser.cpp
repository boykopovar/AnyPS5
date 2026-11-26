#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

void TestUninitialized() {
    int userId = -1;
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(&userId) == USER_SERVICE_OK);
    Require(userId == USER_SERVICE_INITIAL_USER_ID);
}

void TestNullArgument() {
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
}

void TestForegroundUserQuery() {
    int userId = -1;
    Require(sceUserServiceGetForegroundUser(&userId) == USER_SERVICE_OK);
    Require(userId == USER_SERVICE_INITIAL_USER_ID);
    Require(userId >= 0x10000000);
    Require(userId <= 0x10000003);
}

void TestPostTermination() {
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    int userId = -1;
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(&userId) == USER_SERVICE_OK);
    Require(userId == USER_SERVICE_INITIAL_USER_ID);
}

} // namespace

int main() {
    TestUninitialized();
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    TestNullArgument();
    TestForegroundUserQuery();
    TestPostTermination();
    return 0;
}

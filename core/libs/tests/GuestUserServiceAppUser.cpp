#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "UserService app user test failure at line %u\n", location.line());
        std::abort();
    }
}

}

int main() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    SceUserServiceUserId userId = USER_SERVICE_USER_ID_INVALID;
    Require(sceUserServiceGetUserIdByAppId(0, nullptr) == USER_SERVICE_ERROR_INVALID_POINTER);
    Require(sceUserServiceGetUserIdByAppId(0, &userId) == USER_SERVICE_OK);
    Require(userId == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    return 0;
}

#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

} // namespace

int main() {
    SceUserServiceEvent ev{};

    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);

    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_OK);
    Require(ev.user_id == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_ERROR_NO_EVENT);

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceGetEvent(&ev) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    return 0;
}

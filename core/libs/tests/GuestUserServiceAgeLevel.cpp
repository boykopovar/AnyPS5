#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>
#include <limits>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

} // namespace

int main() {
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    std::uint32_t initialAge = 0;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &initialAge) == USER_SERVICE_OK);
    Require(initialAge == 0xFFFFFFFFu);

    const int invalidUsers[] = {
        0,
        USER_SERVICE_USER_ID_INVALID,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_INITIAL_USER_ID + 100,
        0x20000000,
        std::numeric_limits<int>::min(),
        std::numeric_limits<int>::max(),
    };

    for (int user : invalidUsers) {
        std::uint32_t age = 0x5a5a5a5au;
        Require(sceUserServiceGetAgeLevel(user, &age) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(age == 0x5a5a5a5au);
        Require(sceUserServiceGetAgeLevel(user, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }

    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        const int user = USER_SERVICE_INITIAL_USER_ID + i;
        std::uint32_t age = 0xa5a5a5a5u;
        Require(sceUserServiceGetAgeLevel(user, &age) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(age == 0xa5a5a5a5u);
        Require(sceUserServiceGetAgeLevel(user, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        UserServiceSetUserConnected_nid_postfix(i, true);
        Require(sceUserServiceGetAgeLevel(user, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        age = 0;
        Require(sceUserServiceGetAgeLevel(user, &age) == USER_SERVICE_OK);
        Require(age == 0xFFFFFFFFu);

        UserServiceSetUserConnected_nid_postfix(i, false);
        age = 0x11223344u;
        Require(sceUserServiceGetAgeLevel(user, &age) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(age == 0x11223344u);
        Require(sceUserServiceGetAgeLevel(user, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }

    UserServiceSetUserConnected_nid_postfix(0, false);
    std::uint32_t toggledAge = 0x778899aau;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &toggledAge) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(toggledAge == 0x778899aau);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &toggledAge) == USER_SERVICE_OK);
    Require(toggledAge == 0xFFFFFFFFu);

    return 0;
}

#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT = USER_SERVICE_ERROR_INVALID_ARGUMENT;
constexpr int SCE_USER_SERVICE_ERROR_NO_SUCH_USER = USER_SERVICE_ERROR_UNKNOWN_USER;

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

} // namespace

int main() {
    int color = -1;

    for (int userIndex = 0; userIndex < USER_SERVICE_MAX_USERS; ++userIndex) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + userIndex;

        Require(sceUserServiceGetUserColor(userId, nullptr) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);

        color = -1;
        Require(sceUserServiceGetUserColor(userId, &color) == USER_SERVICE_OK);
        Require(color >= 0);
    }

    const int invalidUserIds[] = {
        0,
        1,
        0xFF,
        0x0FFFFFFF,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        0x10000004,
        0x10000005,
        0x12345678,
        0x20000000,
        USER_SERVICE_USER_ID_INVALID,
        static_cast<int>(0xFFFFFFFFu)
    };

    for (int invalidUser : invalidUserIds) {
        color = 123;
        Require(sceUserServiceGetUserColor(invalidUser, &color) == SCE_USER_SERVICE_ERROR_NO_SUCH_USER);
        Require(color == 123);

        Require(sceUserServiceGetUserColor(invalidUser, nullptr) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    return 0;
}

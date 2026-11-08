#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int SCE_USER_SERVICE_ERROR_INVALID_USER_ID = USER_SERVICE_ERROR_UNKNOWN_USER;
constexpr int SCE_USER_SERVICE_ERROR_NO_LOGIN_USER = USER_SERVICE_ERROR_NOT_LOGGED_IN;
constexpr int SCE_USER_SERVICE_ERROR_INVALID_ID_ALT = static_cast<int>(0x80960001u);
constexpr int SCE_USER_SERVICE_ERROR_NO_LOGIN_ALT = static_cast<int>(0x80960004u);

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

bool CheckInvalidUserIdError(int error) {
    return error == SCE_USER_SERVICE_ERROR_INVALID_USER_ID ||
           error == SCE_USER_SERVICE_ERROR_INVALID_ID_ALT ||
           error == USER_SERVICE_ERROR_INVALID_ARGUMENT;
}

bool CheckNoLoginUserError(int error) {
    return error == SCE_USER_SERVICE_ERROR_NO_LOGIN_USER ||
           error == SCE_USER_SERVICE_ERROR_NO_LOGIN_ALT ||
           error == USER_SERVICE_ERROR_NOT_INITIALIZED ||
           error == USER_SERVICE_ERROR_INVALID_ARGUMENT;
}

} // namespace

int main() {
    int initialUserId = -1;
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetInitialUser(&initialUserId) == USER_SERVICE_OK);
    Require(initialUserId == USER_SERVICE_INITIAL_USER_ID);
    Require(initialUserId == 0x10000000);

    const char* const expectedNames[USER_SERVICE_MAX_USERS] = {
        "Player", "Player2", "Player3", "Player4"
    };

    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + i;
        char name[32]{};
        Require(sceUserServiceGetUserName(userId, name, sizeof(name)) == USER_SERVICE_OK);
        Require(std::strcmp(name, expectedNames[i]) == 0);

        int32_t number = -1;
        Require(sceUserServiceGetUserNumber(userId, &number) == USER_SERVICE_OK);
        Require(number == i + 1);

        int color = -1;
        Require(sceUserServiceGetUserColor(userId, &color) == USER_SERVICE_OK);
        Require(color == i);
    }

    const int invalidUserIds[] = {
        0,
        1,
        0x0FFFFFFF,
        USER_SERVICE_INITIAL_USER_ID - 1,
        0x10000004,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        0x10000005,
        0x20000000,
        USER_SERVICE_USER_ID_INVALID,
        static_cast<int>(0xFFFFFFFFu)
    };

    for (const int invalidId : invalidUserIds) {
        char name[32]{};
        const int nameResult = sceUserServiceGetUserName(invalidId, name, sizeof(name));
        Require(CheckInvalidUserIdError(nameResult));
        Require(name[0] == '\0');

        Require(CheckInvalidUserIdError(sceUserServiceGetUserName(invalidId, nullptr, sizeof(name))));

        int32_t number = -1;
        Require(CheckInvalidUserIdError(sceUserServiceGetUserNumber(invalidId, &number)));
        Require(number == -1);
        Require(CheckInvalidUserIdError(sceUserServiceGetUserNumber(invalidId, nullptr)));

        int color = -1;
        Require(CheckInvalidUserIdError(sceUserServiceGetUserColor(invalidId, &color)));
        Require(color == -1);
        Require(CheckInvalidUserIdError(sceUserServiceGetUserColor(invalidId, nullptr)));

        int32_t triggerEffect = -1;
        const int effectResult = sceUserServiceGetAccessibilityTriggerEffect(invalidId, &triggerEffect);
        Require(CheckNoLoginUserError(effectResult));
        Require(triggerEffect == -1);
        Require(CheckNoLoginUserError(sceUserServiceGetAccessibilityTriggerEffect(invalidId, nullptr)));

        int32_t vibration = -1;
        const int vibResult = sceUserServiceGetAccessibilityVibration(invalidId, &vibration);
        Require(CheckNoLoginUserError(vibResult));
        Require(vibration == -1);
        Require(CheckNoLoginUserError(sceUserServiceGetAccessibilityVibration(invalidId, nullptr)));
    }

    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, 32) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (std::size_t size = 0; size <= USER_SERVICE_MAX_USER_NAME_LENGTH; ++size) {
        char buffer[32];
        std::memset(buffer, 'Z', sizeof(buffer));
        Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, buffer, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        for (std::size_t j = 0; j < size; ++j) {
            Require(buffer[j] == 0);
        }
        for (std::size_t j = size; j < sizeof(buffer); ++j) {
            Require(buffer[j] == 'Z');
        }
    }

    char exactBuffer[USER_SERVICE_MAX_USER_NAME_LENGTH + 1];
    std::memset(exactBuffer, 0, sizeof(exactBuffer));
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, exactBuffer, sizeof(exactBuffer)) == USER_SERVICE_OK);
    Require(std::strcmp(exactBuffer, "Player") == 0);

    char largeBuffer[64];
    std::memset(largeBuffer, 'X', sizeof(largeBuffer));
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, largeBuffer, 32) == USER_SERVICE_OK);
    Require(std::strcmp(largeBuffer, "Player") == 0);
    Require(largeBuffer[31] == '\0');
    Require(largeBuffer[sizeof(largeBuffer) - 1] == 'X');

    std::memset(largeBuffer, 'X', sizeof(largeBuffer));
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, largeBuffer, sizeof(largeBuffer)) == USER_SERVICE_OK);
    Require(std::strcmp(largeBuffer, "Player") == 0);
    Require(largeBuffer[sizeof(largeBuffer) - 1] == '\0');

    return 0;
}

#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

constexpr int SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT = USER_SERVICE_ERROR_INVALID_ARGUMENT;
constexpr int SCE_USER_SERVICE_ERROR_NO_SUCH_USER = USER_SERVICE_ERROR_UNKNOWN_USER;
constexpr std::size_t SCE_USER_SERVICE_MAX_USER_NAME_LENGTH = USER_SERVICE_MAX_USER_NAME_LENGTH;

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

bool Filled(const char* buffer, std::size_t from, std::size_t to, char value) {
    for (std::size_t i = from; i < to; ++i) {
        if (buffer[i] != value) {
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    char name[64];

    for (int userIndex = 0; userIndex < USER_SERVICE_MAX_USERS; ++userIndex) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + userIndex;
        const char* expectedName = USER_SERVICE_USER_NAMES[userIndex];
        const std::size_t nameLength = std::strlen(expectedName);

        Require(sceUserServiceGetUserName(userId, nullptr, sizeof(name)) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(userId, nullptr, 0) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(userId, nullptr, SCE_USER_SERVICE_MAX_USER_NAME_LENGTH) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(userId, nullptr, SCE_USER_SERVICE_MAX_USER_NAME_LENGTH + 1) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);

        for (std::size_t size = 0; size <= SCE_USER_SERVICE_MAX_USER_NAME_LENGTH; ++size) {
            std::memset(name, 'A', sizeof(name));
            Require(sceUserServiceGetUserName(userId, name, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
            Require(Filled(name, 0, size, 0) && Filled(name, size, sizeof(name), 'A'));
        }

        std::memset(name, 'B', sizeof(name));
        Require(sceUserServiceGetUserName(userId, name, SCE_USER_SERVICE_MAX_USER_NAME_LENGTH + 1) == USER_SERVICE_OK);
        Require(std::strcmp(name, expectedName) == 0);
        Require(name[nameLength] == '\0');
        Require(name[SCE_USER_SERVICE_MAX_USER_NAME_LENGTH] == '\0');
        Require(Filled(name, nameLength, SCE_USER_SERVICE_MAX_USER_NAME_LENGTH + 1, 0));
        Require(Filled(name, SCE_USER_SERVICE_MAX_USER_NAME_LENGTH + 1, sizeof(name), 'B'));

        for (std::size_t size : std::initializer_list<std::size_t>{32, 48, sizeof(name)}) {
            std::memset(name, 'C', sizeof(name));
            Require(sceUserServiceGetUserName(userId, name, size) == USER_SERVICE_OK);
            Require(std::strcmp(name, expectedName) == 0);
            Require(name[nameLength] == '\0');
            Require(Filled(name, nameLength, size, 0));
            Require(Filled(name, size, sizeof(name), 'C'));
        }
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
        std::memset(name, 'X', sizeof(name));
        Require(sceUserServiceGetUserName(invalidUser, name, sizeof(name)) == SCE_USER_SERVICE_ERROR_NO_SUCH_USER);
        Require(Filled(name, 0, sizeof(name), 'X'));

        Require(sceUserServiceGetUserName(invalidUser, name, 0) == SCE_USER_SERVICE_ERROR_NO_SUCH_USER);
        Require(Filled(name, 0, sizeof(name), 'X'));

        Require(sceUserServiceGetUserName(invalidUser, name, 1) == SCE_USER_SERVICE_ERROR_NO_SUCH_USER);
        Require(Filled(name, 0, sizeof(name), 'X'));

        Require(sceUserServiceGetUserName(invalidUser, name, SCE_USER_SERVICE_MAX_USER_NAME_LENGTH) == SCE_USER_SERVICE_ERROR_NO_SUCH_USER);
        Require(Filled(name, 0, sizeof(name), 'X'));

        Require(sceUserServiceGetUserName(invalidUser, name, SCE_USER_SERVICE_MAX_USER_NAME_LENGTH + 1) == SCE_USER_SERVICE_ERROR_NO_SUCH_USER);
        Require(Filled(name, 0, sizeof(name), 'X'));

        Require(sceUserServiceGetUserName(invalidUser, nullptr, sizeof(name)) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(invalidUser, nullptr, 0) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    char exactBuffer[SCE_USER_SERVICE_MAX_USER_NAME_LENGTH + 1];
    std::memset(exactBuffer, 'Z', sizeof(exactBuffer));
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, exactBuffer, sizeof(exactBuffer)) == USER_SERVICE_OK);
    Require(std::strcmp(exactBuffer, USER_SERVICE_INITIAL_USER_NAME) == 0);
    Require(exactBuffer[SCE_USER_SERVICE_MAX_USER_NAME_LENGTH] == '\0');
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(exactBuffer)) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);

    return 0;
}

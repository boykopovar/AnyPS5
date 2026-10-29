#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

void Require(bool value) { if (!value) std::abort(); }

bool Filled(const char* buffer, std::size_t from, std::size_t to, char value) {
    for (std::size_t i = from; i < to; ++i) {
        if (buffer[i] != value) return false;
    }
    return true;
}

}

int main() {
    char name[32];
    const std::size_t length = std::strlen(USER_SERVICE_INITIAL_USER_NAME);

    for (std::size_t size = 0; size <= USER_SERVICE_MAX_USER_NAME_LENGTH; ++size) {
        std::memset(name, 'A', sizeof(name));
        Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        Require(Filled(name, 0, size, 0) && Filled(name, size, sizeof(name), 'A'));
    }
    for (std::size_t size : {USER_SERVICE_MAX_USER_NAME_LENGTH + 1, sizeof(name)}) {
        std::memset(name, 'A', sizeof(name));
        Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, size) == USER_SERVICE_OK);
        Require(std::strcmp(name, USER_SERVICE_INITIAL_USER_NAME) == 0);
        Require(Filled(name, length, size, 0) && Filled(name, size, sizeof(name), 'A'));
    }

    for (int user : {0, USER_SERVICE_USER_ID_INVALID, 1, 0xff, 0x12345678}) {
        std::memset(name, 'A', sizeof(name));
        Require(sceUserServiceGetUserName(user, name, sizeof(name)) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(sceUserServiceGetUserName(user, name, 1) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(Filled(name, 0, sizeof(name), 'A'));
        Require(sceUserServiceGetUserName(user, nullptr, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
}

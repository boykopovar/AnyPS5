#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceUserServiceGetUserName(int user_id, char* name, std::size_t size);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

bool Filled(const char* buffer, std::size_t from, std::size_t to, char value) {
    for (std::size_t i = from; i < to; ++i) {
        if (buffer[i] != value) return false;
    }
    return true;
}

void SetName(const char* value) {
#ifdef _WIN32
    Require(_putenv_s("ANYPS5_USER_NAME", value != nullptr ? value : "") == 0);
#else
    Require((value != nullptr ? ::setenv("ANYPS5_USER_NAME", value, 1) : ::unsetenv("ANYPS5_USER_NAME")) == 0);
#endif
}

bool Named(const char* expected) {
    char name[32];
    std::memset(name, 'A', sizeof(name));
    const std::size_t size = USER_SERVICE_MAX_USER_NAME_LENGTH + 1;
    return sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, size) == USER_SERVICE_OK && std::strcmp(name, expected) == 0 &&
        Filled(name, std::strlen(expected), size, 0) && Filled(name, size, sizeof(name), 'A');
}

bool Rejected(const char* value) {
    SetName(value);
    char name[32];
    try {
        static_cast<void>(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, sizeof(name)));
    } catch (const std::runtime_error& error) {
        return std::string(error.what()).find("ANYPS5_USER_NAME") != std::string::npos;
    }
    return false;
}

}

int main() {
    SetName(nullptr);
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

    SetName("");
    Require(Named(USER_SERVICE_INITIAL_USER_NAME));
    SetName("Suery");
    Require(Named("Suery"));
    SetName("Jos\xc3\xa9");
    Require(Named("Jos\xc3\xa9"));
    SetName("ABCDEFGHIJKLMNOP");
    Require(Named("ABCDEFGHIJKLMNOP"));
    Require(Rejected("ABCDEFGHIJKLMNOPQ"));
    SetName("Suery");
    std::memset(name, 'A', sizeof(name));
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, USER_SERVICE_MAX_USER_NAME_LENGTH) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    SetName(nullptr);
    Require(Named(USER_SERVICE_INITIAL_USER_NAME));
}

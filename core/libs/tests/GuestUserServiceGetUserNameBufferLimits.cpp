#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>

int main() {
    char name[32];
    for (std::size_t size = 0; size <= USER_SERVICE_MAX_USER_NAME_LENGTH; ++size) {
        std::memset(name, 0x5a, sizeof(name));
        int ret = sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, size);
        if (ret != USER_SERVICE_ERROR_BUFFER_TOO_SHORT) {
            std::abort();
        }
    }
    for (std::size_t size = USER_SERVICE_MAX_USER_NAME_LENGTH + 1; size <= sizeof(name); ++size) {
        std::memset(name, 0x5a, sizeof(name));
        int ret = sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, size);
        if (ret != USER_SERVICE_OK) {
            std::abort();
        }
        if (std::strcmp(name, USER_SERVICE_INITIAL_USER_NAME) != 0) {
            std::abort();
        }
    }
    if (sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(name)) != USER_SERVICE_ERROR_INVALID_ARGUMENT) {
        std::abort();
    }
    if (sceUserServiceGetUserName(999999, name, sizeof(name)) != USER_SERVICE_ERROR_UNKNOWN_USER) {
        std::abort();
    }
    return 0;
}

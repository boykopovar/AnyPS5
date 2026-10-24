#ifndef CORE_LIBS_PRX_LIBSCEUSERSERVICE_USERSERVICE_HPP
#define CORE_LIBS_PRX_LIBSCEUSERSERVICE_USERSERVICE_HPP

#include <cstddef>
#include <cstdint>

struct UserServiceInitParam {
 std::int32_t priority;
};

static_assert(sizeof(UserServiceInitParam) == 4);

constexpr int USER_SERVICE_OK = 0;
constexpr int USER_SERVICE_ERROR_NOT_INITIALIZED = static_cast<int>(0x80960002u);
constexpr int USER_SERVICE_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80960003u);
constexpr int USER_SERVICE_ERROR_INVALID_ARGUMENT = -2137653243;
constexpr int USER_SERVICE_ERROR_NO_EVENT = -2137653241;
constexpr int USER_SERVICE_ERROR_NOT_LOGGED_IN = -2137653239;
constexpr int USER_SERVICE_USER_ID_INVALID = -1;
constexpr int USER_SERVICE_INITIAL_USER_ID = 0x10000000;
constexpr int USER_SERVICE_THREAD_PRIORITY_MIN = 0x100;
constexpr int USER_SERVICE_THREAD_PRIORITY_MAX = 0x2FF;
constexpr int USER_SERVICE_ERROR_BUFFER_TOO_SHORT = -2137653238;
constexpr int USER_SERVICE_ERROR_UNKNOWN_USER = static_cast<int>(0x80960105u);
constexpr std::size_t USER_SERVICE_MAX_USER_NAME_LENGTH = 16;
constexpr const char* USER_SERVICE_INITIAL_USER_NAME = "Player";

#endif

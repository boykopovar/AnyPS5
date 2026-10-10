#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <string>

extern "C" {
int APS5_VABI sceUserServiceGetForegroundUser(int* user_id);
int APS5_VABI sceUserServiceGetRegisteredUserIdList(UserServiceRegisteredUserIdList* user_id_list);
int APS5_VABI sceUserServiceGetUserColor(int user_id, int* color);
int APS5_VABI sceUserServiceGetNpAccountId(int user_id, std::uint64_t* account_id);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int unknownUser = 123;

const Case foregroundNull{"GetForegroundUser_NullOutput_ReturnsInvalidArgument", [] {
    RequireEqual(sceUserServiceGetForegroundUser(nullptr), USER_SERVICE_ERROR_INVALID_ARGUMENT, "null output");
}};

const Case foregroundUser{"GetForegroundUser_ValidOutput_ReturnsInitialUser", [] {
    int foreground = -1;
    RequireEqual(sceUserServiceGetForegroundUser(&foreground), USER_SERVICE_OK, "result");
    RequireEqual(foreground, USER_SERVICE_INITIAL_USER_ID, "foreground user");
}};

const Case registeredNull{"GetRegisteredUserIdList_NullOutput_ReturnsInvalidArgument", [] {
    RequireEqual(sceUserServiceGetRegisteredUserIdList(nullptr), USER_SERVICE_ERROR_INVALID_ARGUMENT, "null output");
}};

const Case registeredUsers{"GetRegisteredUserIdList_ValidOutput_ListsOnlyInitialUser", [] {
    UserServiceRegisteredUserIdList registered{};
    RequireEqual(sceUserServiceGetRegisteredUserIdList(&registered), USER_SERVICE_OK, "result");
    RequireEqual(registered.user_id[0], USER_SERVICE_INITIAL_USER_ID, "first registered user");
    for (int index = 1; index < 16; ++index) {
        RequireEqual(registered.user_id[index], USER_SERVICE_USER_ID_INVALID, "registered user slot " + std::to_string(index));
    }
}};

const Case colorNull{"GetUserColor_NullOutput_ReturnsInvalidArgument", [] {
    RequireEqual(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, nullptr), USER_SERVICE_ERROR_INVALID_ARGUMENT, "null output");
}};

const Case colorUnknownUser{"GetUserColor_UnknownUser_ReturnsInvalidArgument", [] {
    int color = -1;
    RequireEqual(sceUserServiceGetUserColor(unknownUser, &color), USER_SERVICE_ERROR_INVALID_ARGUMENT, "unknown user");
}};

const Case colorInitialUser{"GetUserColor_InitialUser_ReturnsColorZero", [] {
    int color = -1;
    RequireEqual(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, &color), USER_SERVICE_OK, "result");
    RequireEqual(color, 0, "user color");
}};

const Case accountNull{"GetNpAccountId_NullOutput_ReturnsInvalidArgument", [] {
    RequireEqual(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr), USER_SERVICE_ERROR_INVALID_ARGUMENT, "null output");
}};

const Case accountUnknownUser{"GetNpAccountId_UnknownUser_ReturnsInvalidArgument", [] {
    std::uint64_t accountId = 0xdeadbeefu;
    RequireEqual(sceUserServiceGetNpAccountId(unknownUser, &accountId), USER_SERVICE_ERROR_INVALID_ARGUMENT, "unknown user");
}};

const Case accountInitialUser{"GetNpAccountId_InitialUser_ReturnsAccountZero", [] {
    std::uint64_t accountId = 0xdeadbeefu;
    RequireEqual(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, &accountId), USER_SERVICE_OK, "result");
    RequireEqual(accountId, std::uint64_t{0}, "account id");
}};

} // namespace

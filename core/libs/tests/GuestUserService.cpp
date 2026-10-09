#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceUserServiceGetForegroundUser(int* user_id);
int APS5_VABI sceUserServiceGetInitialUser(int* user_id);
int APS5_VABI sceUserServiceGetRegisteredUserIdList(UserServiceRegisteredUserIdList* user_id_list);
int APS5_VABI sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* user_id_list);
int APS5_VABI sceUserServiceGetUserNumber(int user_id, std::int32_t* number);
int APS5_VABI sceUserServiceGetUserName(int user_id, char* name, std::size_t size);
int APS5_VABI sceUserServiceGetUserColor(int user_id, int* color);
int APS5_VABI sceUserServiceGetNpAccountId(int user_id, std::uint64_t* account_id);
int APS5_VABI sceUserServiceGetAgeLevel(int user_id, std::uint32_t* age_level);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    int foreground = -1;
    Require(sceUserServiceGetForegroundUser(&foreground) == USER_SERVICE_OK);
    Require(foreground == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    int initial = -1;
    Require(sceUserServiceGetInitialUser(&initial) == USER_SERVICE_OK);
    Require(initial == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceGetRegisteredUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    UserServiceRegisteredUserIdList registered{};
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    UserServiceLoginUserIdList loginList{};
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 4; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    std::int32_t userNumber = -1;
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserNumber(123, &userNumber) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, &userNumber) == USER_SERVICE_OK);
    Require(userNumber == 1);

    char nameBuffer[32]{};
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(nameBuffer)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(123, nameBuffer, sizeof(nameBuffer)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nameBuffer, 5) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nameBuffer, sizeof(nameBuffer)) == USER_SERVICE_OK);
    Require(std::strcmp(nameBuffer, USER_SERVICE_INITIAL_USER_NAME) == 0);

    int color = -1;
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserColor(123, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, &color) == USER_SERVICE_OK);
    Require(color == 0);

    std::uint64_t account_id = 0xdeadbeefu;
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(123, &account_id) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, &account_id) == USER_SERVICE_OK);
    Require(account_id == 0);

    std::uint32_t ageLevel = 99;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAgeLevel(123, &ageLevel) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &ageLevel) == USER_SERVICE_OK);
    Require(ageLevel == 0);
    return 0;
}

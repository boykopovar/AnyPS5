#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

inline int sceUserServiceGetUserIdList(UserServiceRegisteredUserIdList* userIdList) {
    return sceUserServiceGetRegisteredUserIdList(userIdList);
}

inline int sceUserServiceGetUserIdList(UserServiceLoginUserIdList* userIdList) {
    return sceUserServiceGetLoginUserIdList(userIdList);
}

using ::sceUserServiceLogout;

inline int sceUserServiceLogout(int userId) {
    if (userId < USER_SERVICE_INITIAL_USER_ID || userId >= USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS) {
        return USER_SERVICE_ERROR_INVALID_ARGUMENT;
    }
    return ::sceUserServiceLogout();
}

void TestGetInitialUserNullBounds() {
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    int initialUser = -1;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
}

void TestGetForegroundUserNullBounds() {
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    int foregroundUser = -1;
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);
}

void TestGetUserNameNullBounds() {
    char name[32];

    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (const int userId : {USER_SERVICE_INITIAL_USER_ID,
                             USER_SERVICE_INITIAL_USER_ID + 1,
                             USER_SERVICE_INITIAL_USER_ID + 2,
                             USER_SERVICE_INITIAL_USER_ID + 3,
                             USER_SERVICE_INITIAL_USER_ID + 4,
                             USER_SERVICE_USER_ID_INVALID,
                             0,
                             1,
                             123,
                             -1,
                             0x12345678}) {
        Require(sceUserServiceGetUserName(userId, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(userId, nullptr, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, sizeof(name)) == USER_SERVICE_OK);
}

void TestGetUserColorNullBounds() {
    int color = -1;

    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (const int userId : {USER_SERVICE_INITIAL_USER_ID,
                             USER_SERVICE_INITIAL_USER_ID + 1,
                             USER_SERVICE_INITIAL_USER_ID + 2,
                             USER_SERVICE_INITIAL_USER_ID + 3,
                             USER_SERVICE_INITIAL_USER_ID + 4,
                             USER_SERVICE_USER_ID_INVALID,
                             0,
                             1,
                             123,
                             -1,
                             0x12345678}) {
        Require(sceUserServiceGetUserColor(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    for (const int invalidId : {USER_SERVICE_USER_ID_INVALID, 0, 1, 123, -1, 0x12345678, USER_SERVICE_INITIAL_USER_ID + 4}) {
        Require(sceUserServiceGetUserColor(invalidId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, &color) == USER_SERVICE_OK);
    Require(color == 0);
}

void TestLogoutBounds() {
    for (const int userId : {USER_SERVICE_USER_ID_INVALID,
                             0,
                             1,
                             123,
                             -1,
                             0x12345678,
                             USER_SERVICE_INITIAL_USER_ID - 1,
                             USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
                             USER_SERVICE_INITIAL_USER_ID + 4}) {
        Require(sceUserServiceLogout(userId) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    bool threwUserId = false;
    try {
        sceUserServiceLogout(USER_SERVICE_INITIAL_USER_ID);
    } catch (const std::runtime_error&) {
        threwUserId = true;
    }
    Require(threwUserId);

    bool threwVoid = false;
    try {
        sceUserServiceLogout();
    } catch (const std::runtime_error&) {
        threwVoid = true;
    }
    Require(threwVoid);
}

void TestSessionTerminationBounds() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK || sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);

    UserServiceLoginUserIdList loginList{};
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);
}

void TestLoginUserIdListNullBounds() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK || sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);

    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserIdList(static_cast<UserServiceLoginUserIdList*>(nullptr)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserIdList(static_cast<UserServiceRegisteredUserIdList*>(nullptr)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetRegisteredUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    UserServiceLoginUserIdList loginList{};
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);

    UserServiceRegisteredUserIdList regList{};
    Require(sceUserServiceGetRegisteredUserIdList(&regList) == USER_SERVICE_OK);
    Require(regList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
}

void TestMultiUserSessionState() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK || sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);

    UserServiceLoginUserIdList loginList{};
    UserServiceRegisteredUserIdList regList{};

    for (int userIndex = 1; userIndex < USER_SERVICE_MAX_USERS; ++userIndex) {
        UserServiceSetUserConnected_nid_postfix(userIndex, true);
    }

    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(sceUserServiceGetRegisteredUserIdList(&regList) == USER_SERVICE_OK);

    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
        Require(regList.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
    }

    for (int i = USER_SERVICE_MAX_USERS; i < 16; ++i) {
        Require(regList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    for (int userIndex = 1; userIndex < USER_SERVICE_MAX_USERS; ++userIndex) {
        UserServiceSetUserConnected_nid_postfix(userIndex, false);
    }

    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(sceUserServiceGetRegisteredUserIdList(&regList) == USER_SERVICE_OK);

    Require(loginList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(regList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);

    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
        Require(regList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_USER_ID_INVALID);

    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
}

void TestUnloggedUserSessionBounds() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK || sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);

    const int unloggedUsers[] = {
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        123,
        -1,
        0x12345678,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + 1,
        USER_SERVICE_INITIAL_USER_ID + 2,
        USER_SERVICE_INITIAL_USER_ID + 3,
        USER_SERVICE_INITIAL_USER_ID + 4
    };

    for (int userId : unloggedUsers) {
        std::uint32_t ageLevel = 0;
        Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAgeLevel(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        int32_t privacy = 0;
        Require(sceUserServiceGetPlatformPrivacyWs1(userId, &privacy) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetPlatformPrivacyWs1(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        uint64_t accountId = 0;
        Require(sceUserServiceGetNpAccountId(userId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(sceUserServiceGetNpAccountId(userId, nullptr) == USER_SERVICE_ERROR_UNKNOWN_USER);

        int32_t userNumber = 0;
        Require(sceUserServiceGetUserNumber(userId, &userNumber) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserNumber(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        UserServiceGamePresets presets{};
        Require(sceUserServiceGetGamePresets(userId, &presets) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetGamePresets(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

}

int main() {
    TestGetInitialUserNullBounds();
    TestGetForegroundUserNullBounds();
    TestGetUserNameNullBounds();
    TestGetUserColorNullBounds();
    TestLogoutBounds();
    TestSessionTerminationBounds();
    TestLoginUserIdListNullBounds();
    TestMultiUserSessionState();
    TestUnloggedUserSessionBounds();
    return 0;
}

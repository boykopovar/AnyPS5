#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestInitialUserQueryBounds() {
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    int initialUser = -1;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
}

void TestForegroundUserQueryBounds() {
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    int foregroundUser = -1;
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);
}

void TestLoginStateTransitions() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    int initialUser = -1;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);

    int foregroundUser = -1;
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 1);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID) == 1);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 1) == 0);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + 1) == 0);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_USER_ID_INVALID) == 0);

    UserServiceLoginUserIdList loginList{};
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 4; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    SceUserServiceEvent event{};
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_ERROR_NO_EVENT);

    UserServiceSetUserConnected_nid_postfix(1, true);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 1) == 1);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID + 1) == 1);

    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(loginList.user_id[1] == USER_SERVICE_INITIAL_USER_ID + 1);

    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 1);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 0);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID) == 0);

    initialUser = -1;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);

    foregroundUser = -1;
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_USER_ID_INVALID);
    Require(loginList.user_id[1] == USER_SERVICE_INITIAL_USER_ID + 1);

    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 1);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID + 1) == 0);

    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    for (int i = 0; i < 4; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetEvent(&event) == USER_SERVICE_OK);
    Require(event.event_type == 0);
    Require(event.user_id == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 1);

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestUserIdBounds() {
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + i;

        int number = -1;
        Require(sceUserServiceGetUserNumber(userId, &number) == USER_SERVICE_OK);
        Require(number == i + 1);

        int color = -1;
        Require(sceUserServiceGetUserColor(userId, &color) == USER_SERVICE_OK);
        Require(color == i);

        char name[32]{};
        Require(sceUserServiceGetUserName(userId, name, sizeof(name)) == USER_SERVICE_OK);
        Require(std::strcmp(name, USER_SERVICE_USER_NAMES[i]) == 0);

        char shortBuf[16]{};
        for (std::size_t size = 0; size <= USER_SERVICE_MAX_USER_NAME_LENGTH; ++size) {
            Require(sceUserServiceGetUserName(userId, shortBuf, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        }

        Require(sceUserServiceIsGuestUser(userId) == 0);
        Require(sceUserServiceIsSharePlayClientUser(userId) == 0);

        bool spartacus = false;
        Require(sceUserServiceGetIsSpartacusEnabled(userId, &spartacus) == USER_SERVICE_OK);
        Require(spartacus);
    }
}

void TestInvalidUserIdRejection() {
    const int invalidIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_INITIAL_USER_ID + 100,
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        -2,
        -100,
        0x12345678,
        0x20000000,
        0x7FFFFFFF,
        static_cast<int>(0x80000000u),
        static_cast<int>(0xFFFFFFFFu)
    };

    for (int invalidId : invalidIds) {
        int number = -1;
        Require(sceUserServiceGetUserNumber(invalidId, &number) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(number == -1);

        int color = -1;
        Require(sceUserServiceGetUserColor(invalidId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(color == -1);

        char name[32]{};
        Require(sceUserServiceGetUserName(invalidId, name, sizeof(name)) == USER_SERVICE_ERROR_UNKNOWN_USER);

        int32_t ktype = -1;
        Require(sceUserServiceGetKeyboardType(invalidId, &ktype) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(ktype == -1);

        int32_t jpType = -1;
        Require(sceUserServiceGetJapaneseInputType(invalidId, &jpType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(jpType == -1);

        int32_t zhType = -1;
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, &zhType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(zhType == -1);

        uint64_t accountId = 0xDEADBEEF;
        Require(sceUserServiceGetNpAccountId(invalidId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(accountId == 0xDEADBEEF);

        Require(sceUserServiceIsLoggedIn(invalidId) == 0);
        Require(sceUserServiceIsLoggedInWithoutLock(invalidId) == 0);
    }
}

void TestNullOutputPointers() {
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, 32) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetRegisteredUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetJapaneseInputType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetTraditionalChineseInputType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetIsSpartacusEnabled(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (int invalidId : {0, -1, USER_SERVICE_USER_ID_INVALID, 123}) {
        Require(sceUserServiceGetUserNumber(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserColor(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(invalidId, nullptr, 32) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetKeyboardType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetJapaneseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetNpAccountId(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetIsSpartacusEnabled(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

} // namespace

int main() {
    TestInitialUserQueryBounds();
    TestForegroundUserQueryBounds();
    TestUserIdBounds();
    TestInvalidUserIdRejection();
    TestNullOutputPointers();
    TestLoginStateTransitions();
    return 0;
}

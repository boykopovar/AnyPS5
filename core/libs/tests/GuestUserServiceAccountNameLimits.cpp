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

void TestStorageAccountBound() {
    bool threw = false;
    try {
        sceUserServiceIsUserStorageAccountBound();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

void TestUserNameBoundaryLengths() {
    char nameBuffer[32];

    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + i;

        Require(sceUserServiceGetUserName(userId, nullptr, sizeof(nameBuffer)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(userId, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        for (std::size_t size = 0; size <= USER_SERVICE_MAX_USER_NAME_LENGTH; ++size) {
            std::memset(nameBuffer, 'Z', sizeof(nameBuffer));
            Require(sceUserServiceGetUserName(userId, nameBuffer, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
            for (std::size_t b = 0; b < size; ++b) {
                Require(nameBuffer[b] == 0);
            }
            for (std::size_t b = size; b < sizeof(nameBuffer); ++b) {
                Require(nameBuffer[b] == 'Z');
            }
        }

        std::memset(nameBuffer, 'Z', sizeof(nameBuffer));
        Require(sceUserServiceGetUserName(userId, nameBuffer, USER_SERVICE_MAX_USER_NAME_LENGTH + 1) == USER_SERVICE_OK);
        Require(std::strcmp(nameBuffer, USER_SERVICE_USER_NAMES[i]) == 0);

        std::memset(nameBuffer, 0, sizeof(nameBuffer));
        Require(sceUserServiceGetUserName(userId, nameBuffer, sizeof(nameBuffer)) == USER_SERVICE_OK);
        Require(std::strcmp(nameBuffer, USER_SERVICE_USER_NAMES[i]) == 0);
    }

    const int invalidIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_INITIAL_USER_ID + 10,
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        -1,
        0x20000000
    };
    for (int id : invalidIds) {
        std::memset(nameBuffer, 'Z', sizeof(nameBuffer));
        Require(sceUserServiceGetUserName(id, nameBuffer, sizeof(nameBuffer)) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(nameBuffer[0] == 'Z');
        Require(sceUserServiceGetUserName(id, nullptr, sizeof(nameBuffer)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

void TestFourUserLoginLimitsAndTransitions() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    UserServiceLoginUserIdList loginList{};
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 4; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    UserServiceRegisteredUserIdList regList{};
    Require(sceUserServiceGetRegisteredUserIdList(&regList) == USER_SERVICE_OK);
    Require(regList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(regList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, true);

        const int userId = USER_SERVICE_INITIAL_USER_ID + i;
        Require(sceUserServiceIsLoggedIn(userId) == 1);
        Require(sceUserServiceIsLoggedInWithoutLock(userId) == 1);

        uint64_t accountId = 0xDEADBEEF;
        Require(sceUserServiceGetNpAccountId(userId, &accountId) == USER_SERVICE_OK);
        Require(accountId == 0);
    }

    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
    }

    Require(sceUserServiceGetRegisteredUserIdList(&regList) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(regList.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
    }
    for (int i = USER_SERVICE_MAX_USERS; i < 16; ++i) {
        Require(regList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    UserServiceSetUserConnected_nid_postfix(4, true);
    UserServiceSetUserConnected_nid_postfix(-1, true);

    const int fifthUserId = USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS;
    Require(sceUserServiceIsLoggedIn(fifthUserId) == 0);
    Require(sceUserServiceIsLoggedInWithoutLock(fifthUserId) == 0);

    int number = -1;
    Require(sceUserServiceGetUserNumber(fifthUserId, &number) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    int color = -1;
    Require(sceUserServiceGetUserColor(fifthUserId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    char name[32]{};
    Require(sceUserServiceGetUserName(fifthUserId, name, sizeof(name)) == USER_SERVICE_ERROR_UNKNOWN_USER);
    uint64_t accountId = 0xDEADBEEF;
    Require(sceUserServiceGetNpAccountId(fifthUserId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(accountId == 0xDEADBEEF);

    for (int i = USER_SERVICE_MAX_USERS - 1; i >= 0; --i) {
        UserServiceSetUserConnected_nid_postfix(i, false);

        const int userId = USER_SERVICE_INITIAL_USER_ID + i;
        Require(sceUserServiceIsLoggedIn(userId) == 0);
        Require(sceUserServiceIsLoggedInWithoutLock(userId) == 0);

        accountId = 0xDEADBEEF;
        Require(sceUserServiceGetNpAccountId(userId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(accountId == 0xDEADBEEF);
    }

    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    for (int i = 0; i < 4; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    Require(sceUserServiceGetRegisteredUserIdList(&regList) == USER_SERVICE_OK);
    for (int i = 0; i < 16; ++i) {
        Require(regList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 1);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID) == 1);

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestNpAccountIdQueryBounds() {
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    const int invalidIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        -1,
        0x20000000
    };
    for (int id : invalidIds) {
        uint64_t accountId = 0xCAFEBABE;
        Require(sceUserServiceGetNpAccountId(id, &accountId) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        Require(accountId == 0xCAFEBABE);
        Require(sceUserServiceGetNpAccountId(id, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    }

    uint64_t validAccountId = 0xCAFEBABE;
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, &validAccountId) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(validAccountId == 0xCAFEBABE);
}

void TestNullPointerValidation() {
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
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetIsSpartacusEnabled(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (int invalidId : {0, -1, USER_SERVICE_USER_ID_INVALID, 123}) {
        Require(sceUserServiceGetUserNumber(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserColor(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(invalidId, nullptr, 32) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetKeyboardType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetJapaneseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetNpAccountId(invalidId, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        Require(sceUserServiceGetIsSpartacusEnabled(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

} // namespace

int main() {
    TestStorageAccountBound();
    TestUserNameBoundaryLengths();
    TestNpAccountIdQueryBounds();
    TestNullPointerValidation();
    TestFourUserLoginLimitsAndTransitions();
    return 0;
}

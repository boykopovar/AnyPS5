#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestUserColorEnumerationBounds() {
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + i;
        int color = -1;
        Require(sceUserServiceGetUserColor(userId, &color) == USER_SERVICE_OK);
        Require(color == i);
    }

    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    const int invalidIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_INITIAL_USER_ID + 10,
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        -1,
        0x20000000,
        0x7FFFFFFF,
        static_cast<int>(0x80000000u),
        static_cast<int>(0xFFFFFFFFu)
    };

    for (int invalidId : invalidIds) {
        int color = 0x5A5A;
        Require(sceUserServiceGetUserColor(invalidId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(color == 0x5A5A);
        Require(sceUserServiceGetUserColor(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

void TestKeyboardInputTypeMappings() {
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + i;

        int32_t jpType = -1;
        Require(sceUserServiceGetJapaneseInputType(userId, &jpType) == USER_SERVICE_OK);
        Require(jpType == 0);

        int32_t zhType = -1;
        Require(sceUserServiceGetTraditionalChineseInputType(userId, &zhType) == USER_SERVICE_OK);
        Require(zhType == 0);

        int32_t ktype = -1;
        Require(sceUserServiceGetKeyboardType(userId, &ktype) == USER_SERVICE_OK);
        Require(ktype > 0);
    }

    Require(sceUserServiceGetJapaneseInputType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetTraditionalChineseInputType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    const int invalidIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        -1,
        0x20000000,
        static_cast<int>(0xFFFFFFFFu)
    };

    for (int invalidId : invalidIds) {
        int32_t value = 0x5A5A;
        Require(sceUserServiceGetJapaneseInputType(invalidId, &value) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(value == 0x5A5A);
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, &value) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(value == 0x5A5A);
        Require(sceUserServiceGetKeyboardType(invalidId, &value) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(value == 0x5A5A);

        Require(sceUserServiceGetJapaneseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetKeyboardType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

void TestLoginUserIdListOffsetBounds() {
    UserServiceLoginUserIdList list{};
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 4; ++i) {
        Require(list.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, true);
    }

    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(list.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
    }

    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(list.user_id[1] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[2] == USER_SERVICE_INITIAL_USER_ID + 2);
    Require(list.user_id[3] == USER_SERVICE_INITIAL_USER_ID + 3);

    UserServiceSetUserConnected_nid_postfix(1, true);
    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceGetLoginUserIdList(&list) == USER_SERVICE_OK);
    Require(list.user_id[0] == USER_SERVICE_USER_ID_INVALID);
    Require(list.user_id[1] == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(list.user_id[2] == USER_SERVICE_INITIAL_USER_ID + 2);
    Require(list.user_id[3] == USER_SERVICE_INITIAL_USER_ID + 3);

    UserServiceSetUserConnected_nid_postfix(0, true);
    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, false);
    }

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestNullPointerValidation() {
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetRegisteredUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetIsSpartacusEnabled(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (int invalidId : {0, -1, USER_SERVICE_USER_ID_INVALID, 123}) {
        Require(sceUserServiceGetUserNumber(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetNpAccountId(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetIsSpartacusEnabled(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

} // namespace

int main() {
    TestUserColorEnumerationBounds();
    TestKeyboardInputTypeMappings();
    TestLoginUserIdListOffsetBounds();
    TestNullPointerValidation();
    return 0;
}

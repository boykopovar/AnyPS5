#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestGetInitialUserNullBounds() {
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    int initialUser = -1;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
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

void TestGetAccessibilityChatTranscriptionNullBounds() {
    Require(sceUserServiceGetAccessibilityChatTranscription(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (const int userId : {USER_SERVICE_USER_ID_INVALID,
                             0,
                             1,
                             123,
                             -1,
                             0x12345678,
                             USER_SERVICE_INITIAL_USER_ID + 4}) {
        std::int32_t chatTranscription = 0x5a5a;
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, &chatTranscription) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(chatTranscription == 0x5a5a);
    }

    std::int32_t chatTranscription = -1;
    Require(sceUserServiceGetAccessibilityChatTranscription(USER_SERVICE_INITIAL_USER_ID, &chatTranscription) == USER_SERVICE_OK);
    Require(chatTranscription == 0);
}

void TestGetAccessibilityPressAndHoldDelayNullBounds() {
    Require(sceUserServiceGetAccessibilityPressAndHoldDelay(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (const int userId : {USER_SERVICE_USER_ID_INVALID,
                             0,
                             1,
                             123,
                             -1,
                             0x12345678,
                             USER_SERVICE_INITIAL_USER_ID + 4}) {
        std::int32_t pressAndHoldDelay = 0x5a5a;
        Require(sceUserServiceGetAccessibilityPressAndHoldDelay(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityPressAndHoldDelay(userId, &pressAndHoldDelay) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(pressAndHoldDelay == 0x5a5a);
    }

    std::int32_t pressAndHoldDelay = -1;
    Require(sceUserServiceGetAccessibilityPressAndHoldDelay(USER_SERVICE_INITIAL_USER_ID, &pressAndHoldDelay) == USER_SERVICE_OK);
    Require(pressAndHoldDelay == 0);
}

}

int main() {
    TestGetInitialUserNullBounds();
    TestGetUserNameNullBounds();
    TestGetUserColorNullBounds();
    TestGetAccessibilityChatTranscriptionNullBounds();
    TestGetAccessibilityPressAndHoldDelayNullBounds();
    return 0;
}

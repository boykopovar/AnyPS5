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

void TestGetUserColorComprehensiveBounds() {
    for (const int validUserId : {USER_SERVICE_INITIAL_USER_ID,
                                  USER_SERVICE_INITIAL_USER_ID + 1,
                                  USER_SERVICE_INITIAL_USER_ID + 2,
                                  USER_SERVICE_INITIAL_USER_ID + 3}) {
        Require(sceUserServiceGetUserColor(validUserId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int color = -1;
        Require(sceUserServiceGetUserColor(validUserId, &color) == USER_SERVICE_OK);
        Require(color == (validUserId - USER_SERVICE_INITIAL_USER_ID));

        int repeatColor = -1;
        Require(sceUserServiceGetUserColor(validUserId, &repeatColor) == USER_SERVICE_OK);
        Require(repeatColor == color);
    }

    for (const int invalidId : {USER_SERVICE_INITIAL_USER_ID - 1,
                                USER_SERVICE_INITIAL_USER_ID + 4,
                                USER_SERVICE_INITIAL_USER_ID + 5,
                                USER_SERVICE_INITIAL_USER_ID + 100,
                                USER_SERVICE_USER_ID_INVALID,
                                0,
                                1,
                                2,
                                123,
                                -1,
                                -2,
                                -100,
                                0x12345678,
                                -2147483647 - 1,
                                2147483647}) {
        Require(sceUserServiceGetUserColor(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int color = 0x5a5a;
        Require(sceUserServiceGetUserColor(invalidId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(color == 0x5a5a);
    }
}

void TestTeamSettingsBounds() {
    bool threwGetTeamShow = false;
    try {
        sceUserServiceGetTeamShowAboutTeam();
    } catch (const std::runtime_error&) {
        threwGetTeamShow = true;
    }
    Require(threwGetTeamShow);

    bool threwSetTeamShow = false;
    try {
        sceUserServiceSetTeamShowAboutTeam();
    } catch (const std::runtime_error&) {
        threwSetTeamShow = true;
    }
    Require(threwSetTeamShow);

    bool threwGetEventFilter = false;
    try {
        sceUserServiceGetEventFilterTeamEvent();
    } catch (const std::runtime_error&) {
        threwGetEventFilter = true;
    }
    Require(threwGetEventFilter);

    bool threwSetEventFilter = false;
    try {
        sceUserServiceSetEventFilterTeamEvent();
    } catch (const std::runtime_error&) {
        threwSetEventFilter = true;
    }
    Require(threwSetEventFilter);
}

void TestThemeWaveColorBounds() {
    bool threwGetCommentColor = false;
    try {
        sceUserServiceGetGlsBroadcastersCommentColor();
    } catch (const std::runtime_error&) {
        threwGetCommentColor = true;
    }
    Require(threwGetCommentColor);

    bool threwSetCommentColor = false;
    try {
        sceUserServiceSetGlsBroadcastersCommentColor();
    } catch (const std::runtime_error&) {
        threwSetCommentColor = true;
    }
    Require(threwSetCommentColor);
}

}

int main() {
    TestGetInitialUserNullBounds();
    TestGetUserNameNullBounds();
    TestGetUserColorNullBounds();
    TestGetUserColorComprehensiveBounds();
    TestTeamSettingsBounds();
    TestThemeWaveColorBounds();
    return 0;
}

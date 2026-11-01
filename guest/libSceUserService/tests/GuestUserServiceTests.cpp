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

void TestGetAccessibilityChatTranscriptionNullBounds() {
    Require(sceUserServiceGetAccessibilityChatTranscription(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (const int userId : {USER_SERVICE_USER_ID_INVALID,
                             0,
                             1,
                             123,
                             -1,
                             -2,
                             -100,
                             0x12345678,
                             USER_SERVICE_INITIAL_USER_ID - 1,
                             USER_SERVICE_INITIAL_USER_ID + 1,
                             USER_SERVICE_INITIAL_USER_ID + 2,
                             USER_SERVICE_INITIAL_USER_ID + 3,
                             USER_SERVICE_INITIAL_USER_ID + 4,
                             USER_SERVICE_INITIAL_USER_ID + 100,
                             -2147483647 - 1,
                             2147483647}) {
        std::int32_t chatTranscription = 0x5a5a;
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, &chatTranscription) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(chatTranscription == 0x5a5a);
    }

    std::int32_t chatTranscription = -1;
    Require(sceUserServiceGetAccessibilityChatTranscription(USER_SERVICE_INITIAL_USER_ID, &chatTranscription) == USER_SERVICE_OK);
    Require(chatTranscription == 0);

    for (int userIndex = 1; userIndex < USER_SERVICE_MAX_USERS; ++userIndex) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + userIndex;
        UserServiceSetUserConnected_nid_postfix(userIndex, true);

        Require(sceUserServiceGetAccessibilityChatTranscription(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        std::int32_t activeTranscription = -1;
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, &activeTranscription) == USER_SERVICE_OK);
        Require(activeTranscription == 0);

        UserServiceSetUserConnected_nid_postfix(userIndex, false);

        std::int32_t unloggedTranscription = 0x5a5a;
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityChatTranscription(userId, &unloggedTranscription) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(unloggedTranscription == 0x5a5a);
    }
}

void TestPartyChatTranscriptionUnimplementedBounds() {
    bool threwSetLang = false;
    try {
        sceUserServiceSetPartyChatTranscriptionLanguageType();
    } catch (const std::runtime_error&) {
        threwSetLang = true;
    }
    Require(threwSetLang);

    bool threwSetFlag = false;
    try {
        sceUserServiceSetPartyChatTranscriptionFlag();
    } catch (const std::runtime_error&) {
        threwSetFlag = true;
    }
    Require(threwSetFlag);

    bool threwSetVoice = false;
    try {
        sceUserServiceSetPartyChatTranscriptionVoiceType();
    } catch (const std::runtime_error&) {
        threwSetVoice = true;
    }
    Require(threwSetVoice);

    bool threwGetLang = false;
    try {
        sceUserServiceGetPartyChatTranscriptionLanguageType();
    } catch (const std::runtime_error&) {
        threwGetLang = true;
    }
    Require(threwGetLang);

    bool threwGetFlag = false;
    try {
        sceUserServiceGetPartyChatTranscriptionFlag();
    } catch (const std::runtime_error&) {
        threwGetFlag = true;
    }
    Require(threwGetFlag);
}

}

int main() {
    TestGetInitialUserNullBounds();
    TestGetUserNameNullBounds();
    TestGetUserColorNullBounds();
    TestGetAccessibilityChatTranscriptionNullBounds();
    TestPartyChatTranscriptionUnimplementedBounds();
    return 0;
}

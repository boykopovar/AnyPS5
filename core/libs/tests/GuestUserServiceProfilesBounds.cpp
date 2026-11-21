#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <array>
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

bool MemoryMatches(const char* buffer, std::size_t offset, std::size_t count, char expected) {
    for (std::size_t i = 0; i < count; ++i) {
        if (buffer[offset + i] != expected) {
            return false;
        }
    }
    return true;
}

void TestUserIdRangeBounds() {
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

        char homePath[64]{};
        Require(sceUserServiceGetHomeDirectory(userId, homePath, sizeof(homePath)) == USER_SERVICE_OK);
        char expectedHome[64]{};
        std::snprintf(expectedHome, sizeof(expectedHome), "/user/home/%08x", userId);
        Require(std::strcmp(homePath, expectedHome) == 0);

        char secPath[64]{};
        Require(sceUserServiceGetSecureHomeDirectory(userId, secPath, sizeof(secPath)) == USER_SERVICE_OK);
        char expectedSec[64]{};
        std::snprintf(expectedSec, sizeof(expectedSec), "/priv/home/%08x", userId);
        Require(std::strstr(secPath, expectedSec) != nullptr);

        Require(sceUserServiceIsGuestUser(userId) == 0);
    }
}

void TestInvalidUserIdRejection() {
    const int invalidUserIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS + 1,
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

    for (int invalidId : invalidUserIds) {
        int number = -1;
        Require(sceUserServiceGetUserNumber(invalidId, &number) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(number == -1);
        Require(sceUserServiceGetUserNumber(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int color = -1;
        Require(sceUserServiceGetUserColor(invalidId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(color == -1);
        Require(sceUserServiceGetUserColor(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        char name[32]{};
        Require(sceUserServiceGetUserName(invalidId, name, sizeof(name)) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(sceUserServiceGetUserName(invalidId, nullptr, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(invalidId, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(invalidId, name, 5) == USER_SERVICE_ERROR_UNKNOWN_USER);

        int32_t ktype = -1;
        Require(sceUserServiceGetKeyboardType(invalidId, &ktype) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(ktype == -1);
        Require(sceUserServiceGetKeyboardType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int32_t jpType = -1;
        Require(sceUserServiceGetJapaneseInputType(invalidId, &jpType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(jpType == -1);
        Require(sceUserServiceGetJapaneseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int32_t zhType = -1;
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, &zhType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(zhType == -1);
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        uint32_t ageLevel = 0x5A5A;
        Require(sceUserServiceGetAgeLevel(invalidId, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(ageLevel == 0x5A5A);
        Require(sceUserServiceGetAgeLevel(invalidId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        int32_t privacy = 0x5A5A;
        Require(sceUserServiceGetPlatformPrivacyWs1(invalidId, &privacy) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(privacy == 0x5A5A);
        Require(sceUserServiceGetPlatformPrivacyWs1(invalidId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        uint64_t accountId = 0xDEADBEEF;
        Require(sceUserServiceGetNpAccountId(invalidId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(accountId == 0xDEADBEEF);
        Require(sceUserServiceGetNpAccountId(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        UserServiceGamePresets presets{};
        presets.difficulty = 0x5A;
        Require(sceUserServiceGetGamePresets(invalidId, &presets) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(presets.difficulty == 0x5A);
        Require(sceUserServiceGetGamePresets(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        using A11yFn = int (APS5_VABI *)(int, int32_t*);
        const A11yFn a11yFunctions[] = {
            sceUserServiceGetAccessibilityChatTranscription,
            sceUserServiceGetAccessibilityPressAndHoldDelay,
            sceUserServiceGetAccessibilityTriggerEffect,
            sceUserServiceGetAccessibilityVibration,
            sceUserServiceGetAccessibilityZoomEnabled,
            sceUserServiceGetAccessibilityZoomFollowFocus
        };
        for (auto fn : a11yFunctions) {
            int32_t val = 0x5A5A;
            Require(fn(invalidId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
            Require(val == 0x5A5A);
            Require(fn(invalidId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        }
    }
}

void TestUserNameBufferOverflowGuards() {
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + i;
        const char* expectedName = USER_SERVICE_USER_NAMES[i];
        const std::size_t nameLen = std::strlen(expectedName);

        Require(sceUserServiceGetUserName(userId, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(userId, nullptr, 16) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(userId, nullptr, 32) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        for (std::size_t size = 0; size <= USER_SERVICE_MAX_USER_NAME_LENGTH; ++size) {
            char buffer[64];
            std::memset(buffer, 'Z', sizeof(buffer));

            const int result = sceUserServiceGetUserName(userId, buffer, size);
            Require(result == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
            Require(MemoryMatches(buffer, 0, size, 0));
            Require(MemoryMatches(buffer, size, sizeof(buffer) - size, 'Z'));
        }

        for (std::size_t size : {USER_SERVICE_MAX_USER_NAME_LENGTH + 1, std::size_t{24}, std::size_t{32}, std::size_t{64}}) {
            char buffer[64];
            std::memset(buffer, 'Z', sizeof(buffer));

            const int result = sceUserServiceGetUserName(userId, buffer, size);
            Require(result == USER_SERVICE_OK);
            Require(std::strcmp(buffer, expectedName) == 0);
            Require(MemoryMatches(buffer, nameLen, 1, 0));
            Require(MemoryMatches(buffer, size, sizeof(buffer) - size, 'Z'));
        }
    }
}

void TestLoginStateTransitions() {
    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    UserServiceLoginUserIdList uninitList{};
    Require(sceUserServiceGetLoginUserIdList(&uninitList) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    UserServiceInitParam badParam{};
    badParam.priority = USER_SERVICE_THREAD_PRIORITY_MIN - 1;
    Require(sceUserServiceInitialize(&badParam) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    badParam.priority = USER_SERVICE_THREAD_PRIORITY_MAX + 1;
    Require(sceUserServiceInitialize(&badParam) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_ERROR_ALREADY_INITIALIZED);

    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == 1);
    Require(sceUserServiceIsLoggedInWithoutLock(USER_SERVICE_INITIAL_USER_ID) == 1);

    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + i;
        Require(sceUserServiceIsLoggedIn(userId) == 0);
        Require(sceUserServiceIsLoggedInWithoutLock(userId) == 0);

        uint32_t ageLevel = 0;
        Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        int32_t privacy = -1;
        Require(sceUserServiceGetPlatformPrivacyWs1(userId, &privacy) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        UserServiceGamePresets presets{};
        Require(sceUserServiceGetGamePresets(userId, &presets) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        uint64_t accountId = 0xDEAD;
        Require(sceUserServiceGetNpAccountId(userId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(accountId == 0xDEAD);
    }

    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    UserServiceLoginUserIdList loginList{};
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 4; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    UserServiceSetUserConnected_nid_postfix(-1, true);
    UserServiceSetUserConnected_nid_postfix(USER_SERVICE_MAX_USERS, true);

    for (int slot = 1; slot < USER_SERVICE_MAX_USERS; ++slot) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + slot;

        UserServiceSetUserConnected_nid_postfix(slot, true);

        Require(sceUserServiceIsLoggedIn(userId) == 1);
        Require(sceUserServiceIsLoggedInWithoutLock(userId) == 1);

        uint32_t ageLevel = 0;
        Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_OK);
        Require(ageLevel == 0xFFFFFFFFu);

        int32_t privacy = -1;
        Require(sceUserServiceGetPlatformPrivacyWs1(userId, &privacy) == USER_SERVICE_OK);
        Require(privacy == 0);

        UserServiceGamePresets presets{};
        presets.difficulty = 99;
        Require(sceUserServiceGetGamePresets(userId, &presets) == USER_SERVICE_OK);
        Require(presets.difficulty == 0);

        uint64_t accountId = 0xDEAD;
        Require(sceUserServiceGetNpAccountId(userId, &accountId) == USER_SERVICE_OK);
        Require(accountId == 0);

        int32_t triggerEffect = 0;
        Require(sceUserServiceGetAccessibilityTriggerEffect(userId, &triggerEffect) == USER_SERVICE_OK);
        Require(triggerEffect == 1);

        int32_t vibration = 0;
        Require(sceUserServiceGetAccessibilityVibration(userId, &vibration) == USER_SERVICE_OK);
        Require(vibration == 1);
    }

    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
    }

    UserServiceRegisteredUserIdList regList{};
    Require(sceUserServiceGetRegisteredUserIdList(&regList) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(regList.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
    }
    for (int i = USER_SERVICE_MAX_USERS; i < 16; ++i) {
        Require(regList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    for (int slot = 1; slot < USER_SERVICE_MAX_USERS; ++slot) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + slot;

        UserServiceSetUserConnected_nid_postfix(slot, false);

        Require(sceUserServiceIsLoggedIn(userId) == 0);
        Require(sceUserServiceIsLoggedInWithoutLock(userId) == 0);

        uint32_t ageLevel = 0;
        Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        int32_t privacy = -1;
        Require(sceUserServiceGetPlatformPrivacyWs1(userId, &privacy) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        UserServiceGamePresets presets{};
        Require(sceUserServiceGetGamePresets(userId, &presets) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        uint64_t accountId = 0xDEAD;
        Require(sceUserServiceGetNpAccountId(userId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
    }

    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    Require(loginList.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 4; ++i) {
        Require(loginList.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceIsLoggedIn(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

}

int main() {
    TestUserIdRangeBounds();
    TestInvalidUserIdRejection();
    TestUserNameBufferOverflowGuards();
    TestLoginStateTransitions();
    return 0;
}

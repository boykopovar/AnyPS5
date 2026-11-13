#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

constexpr int SCE_USER_SERVICE_ERROR_INVALID_USER_ID = USER_SERVICE_ERROR_UNKNOWN_USER;
constexpr int USER_SERVICE_ERROR_NO_SUCH_USER = USER_SERVICE_ERROR_UNKNOWN_USER;

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void SetLanguage(const char* value) {
#ifdef _WIN32
    Require(_putenv_s("ANYPS5_LANGUAGE", value != nullptr ? value : "") == 0);
#else
    Require((value != nullptr ? ::setenv("ANYPS5_LANGUAGE", value, 1) : ::unsetenv("ANYPS5_LANGUAGE")) == 0);
#endif
}

struct Setting {
    int (APS5_VABI *get)(int, std::int32_t*);
    std::int32_t value;
};

const Setting settings[] = {
    {sceUserServiceGetAccessibilityChatTranscription, 0},
    {sceUserServiceGetAccessibilityPressAndHoldDelay, 0},
    {sceUserServiceGetAccessibilityTriggerEffect, 1},
    {sceUserServiceGetAccessibilityVibration, 1},
    {sceUserServiceGetAccessibilityZoomEnabled, 0},
    {sceUserServiceGetAccessibilityZoomFollowFocus, 1},
};

void VerifyLoggedOutUser(int userId) {
    std::uint32_t ageLevel = 0x5a5a5a5a;
    Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(ageLevel == 0x5a5a5a5a);
    Require(sceUserServiceGetAgeLevel(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

    int32_t privacy = 0x5a5a;
    Require(sceUserServiceGetPlatformPrivacyWs1(userId, &privacy) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(privacy == 0x5a5a);
    Require(sceUserServiceGetPlatformPrivacyWs1(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

    std::uint64_t accountId = 0xdeadbeefu;
    Require(sceUserServiceGetNpAccountId(userId, &accountId) == USER_SERVICE_ERROR_NO_SUCH_USER);
    Require(accountId == 0xdeadbeefu);
    Require(sceUserServiceGetNpAccountId(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    UserServiceGamePresets presets{};
    presets.difficulty = 0x5a;
    Require(sceUserServiceGetGamePresets(userId, &presets) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(presets.difficulty == 0x5a);
    Require(sceUserServiceGetGamePresets(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (const Setting& setting : settings) {
        std::int32_t value = 0x5a5a5a5a;
        Require(setting.get(userId, &value) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(value == 0x5a5a5a5a);
        Require(setting.get(userId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }
}

void VerifyLoggedInUser(int userId) {
    std::uint32_t ageLevel = 0;
    Require(sceUserServiceGetAgeLevel(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_OK);
    Require(ageLevel == 0xffffffffu);

    int32_t privacy = -1;
    Require(sceUserServiceGetPlatformPrivacyWs1(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetPlatformPrivacyWs1(userId, &privacy) == USER_SERVICE_OK);
    Require(privacy == 0);

    std::uint64_t accountId = 0xdeadbeefu;
    Require(sceUserServiceGetNpAccountId(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(userId, &accountId) == USER_SERVICE_OK);
    Require(accountId == 0);

    UserServiceGamePresets presets{};
    presets.difficulty = 0x5a;
    Require(sceUserServiceGetGamePresets(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetGamePresets(userId, &presets) == USER_SERVICE_OK);
    Require(presets.difficulty == 0);

    for (const Setting& setting : settings) {
        std::int32_t value = 0x5a5a5a5a;
        Require(setting.get(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(setting.get(userId, &value) == USER_SERVICE_OK);
        Require(value == setting.value);
    }
}

}

int main() {
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    int foreground = -1;
    Require(sceUserServiceGetForegroundUser(&foreground) == USER_SERVICE_OK);
    Require(foreground == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    int initialUser = -1;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceGetRegisteredUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    UserServiceRegisteredUserIdList registered{};
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    std::int32_t homeIds[16]{};
    int homeCount = -1;
    Require(sceUserServiceGetRegisteredHomeUserIdList(nullptr, &homeCount) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetRegisteredHomeUserIdList(homeIds, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    std::memset(homeIds, 0x7B, sizeof(homeIds));
    Require(sceUserServiceGetRegisteredHomeUserIdList(homeIds, &homeCount) == USER_SERVICE_OK);
    Require(homeCount == 1 && homeIds[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(homeIds[i] == USER_SERVICE_USER_ID_INVALID);
    }

    char nameBuffer[32]{};
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + i;

        Require(sceUserServiceGetUserName(userId, nullptr, sizeof(nameBuffer)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        for (std::size_t size = 0; size <= USER_SERVICE_MAX_USER_NAME_LENGTH; ++size) {
            std::memset(nameBuffer, 'Z', sizeof(nameBuffer));
            Require(sceUserServiceGetUserName(userId, nameBuffer, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
            for (std::size_t b = 0; b < size; ++b) {
                Require(nameBuffer[b] == 0);
            }
            Require(nameBuffer[size] == 'Z');
        }

        std::memset(nameBuffer, 'Z', sizeof(nameBuffer));
        Require(sceUserServiceGetUserName(userId, nameBuffer, USER_SERVICE_MAX_USER_NAME_LENGTH + 1) == USER_SERVICE_OK);
        Require(std::strcmp(nameBuffer, USER_SERVICE_USER_NAMES[i]) == 0);
        Require(std::strlen(nameBuffer) <= USER_SERVICE_MAX_USER_NAME_LENGTH);
        Require(nameBuffer[std::strlen(USER_SERVICE_USER_NAMES[i])] == '\0');

        std::memset(nameBuffer, 0, sizeof(nameBuffer));
        Require(sceUserServiceGetUserName(userId, nameBuffer, sizeof(nameBuffer)) == USER_SERVICE_OK);
        Require(std::strcmp(nameBuffer, USER_SERVICE_USER_NAMES[i]) == 0);
        Require(std::strlen(nameBuffer) <= USER_SERVICE_MAX_USER_NAME_LENGTH);

        int32_t number = -1;
        Require(sceUserServiceGetUserNumber(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserNumber(userId, &number) == USER_SERVICE_OK);
        Require(number == i + 1);

        int color = -1;
        Require(sceUserServiceGetUserColor(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserColor(userId, &color) == USER_SERVICE_OK);
        Require(color == i);

        int32_t keyboardType = -1;
        Require(sceUserServiceGetKeyboardType(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetKeyboardType(userId, &keyboardType) == USER_SERVICE_OK);
        Require(keyboardType == 20);

        int32_t japaneseInput = -1;
        Require(sceUserServiceGetJapaneseInputType(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetJapaneseInputType(userId, &japaneseInput) == USER_SERVICE_OK);
        Require(japaneseInput == 0);

        int32_t chineseInput = -1;
        Require(sceUserServiceGetTraditionalChineseInputType(userId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetTraditionalChineseInputType(userId, &chineseInput) == USER_SERVICE_OK);
        Require(chineseInput == 0);
    }

    const int nonExistentUsers[] = {
        0,
        USER_SERVICE_USER_ID_INVALID,
        123,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS + 1,
        0x20000000,
        0x30000000,
        static_cast<int>(0xFFFFFFFFu),
    };
    for (int invalidId : nonExistentUsers) {
        Require(sceUserServiceGetUserName(invalidId, nullptr, sizeof(nameBuffer)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(invalidId, nameBuffer, sizeof(nameBuffer)) == USER_SERVICE_ERROR_NO_SUCH_USER);

        int32_t number = -1;
        Require(sceUserServiceGetUserNumber(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserNumber(invalidId, &number) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(number == -1);

        int color = -1;
        Require(sceUserServiceGetUserColor(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserColor(invalidId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(color == -1);

        int32_t keyboardType = -1;
        Require(sceUserServiceGetKeyboardType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetKeyboardType(invalidId, &keyboardType) == USER_SERVICE_ERROR_NO_SUCH_USER);
        Require(keyboardType == -1);

        int32_t japaneseInput = -1;
        Require(sceUserServiceGetJapaneseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetJapaneseInputType(invalidId, &japaneseInput) == USER_SERVICE_ERROR_NO_SUCH_USER);
        Require(japaneseInput == -1);

        int32_t chineseInput = -1;
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, &chineseInput) == USER_SERVICE_ERROR_NO_SUCH_USER);
        Require(chineseInput == -1);

        VerifyLoggedOutUser(invalidId);
    }

    char homePath[64]{};
    Require(sceUserServiceGetHomeDirectory(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(homePath)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetHomeDirectory(USER_SERVICE_INITIAL_USER_ID, homePath, 1) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    Require(sceUserServiceGetHomeDirectory(USER_SERVICE_INITIAL_USER_ID, homePath, sizeof(homePath)) == USER_SERVICE_OK);
    Require(std::strcmp(homePath, "/user/home/10000000") == 0);
    Require(sceUserServiceGetSecureHomeDirectory(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(homePath)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetSecureHomeDirectory(USER_SERVICE_INITIAL_USER_ID, homePath, 1) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    Require(sceUserServiceGetSecureHomeDirectory(USER_SERVICE_INITIAL_USER_ID, homePath, sizeof(homePath)) == USER_SERVICE_OK);
    Require(std::strstr(homePath, "/priv/home/10000000") != nullptr);
    Require(libSceUserServiceUnknown_ErTMkmSho4E_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, nullptr, 37) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(libSceUserServiceUnknown_ErTMkmSho4E_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, homePath, 36) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    Require(libSceUserServiceUnknown_ErTMkmSho4E_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, homePath, 37) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    Require(libSceUserServiceUnknown_ErTMkmSho4E_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, homePath, 38) == USER_SERVICE_OK);
    Require(std::strlen(homePath) == 36);
    Require(sceUserServiceGetCurrentUserGroupIndex(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    errno = 71;
    Require(sceUserServiceGetCurrentUserGroupIndex(reinterpret_cast<std::int32_t*>(0x10)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(errno == 71);
    Require(sceUserServiceSetUserGroupIndex(-2) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceSetUserGroupIndex(129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupName(0, nullptr, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupName(0, homePath, 128) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    char oversizedGroupName[256]{};
    Require(sceUserServiceGetUserGroupName(0, oversizedGroupName, sizeof(oversizedGroupName)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    SetLanguage(nullptr);
    using KeyboardSetting = decltype(&sceUserServiceGetKeyboardType);
    const std::pair<KeyboardSetting, std::int32_t> keyboardSettings[] = {
        {sceUserServiceGetKeyboardType, 20},
        {sceUserServiceGetJapaneseInputType, 0},
        {sceUserServiceGetTraditionalChineseInputType, 0},
    };
    for (const auto& [setting, expected] : keyboardSettings) {
        std::int32_t value = 0x5A5A;
        Require(setting(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(setting(123, &value) == USER_SERVICE_ERROR_NO_SUCH_USER && value == 0x5A5A);
        Require(setting(USER_SERVICE_USER_ID_INVALID, &value) == USER_SERVICE_ERROR_NO_SUCH_USER && value == 0x5A5A);
        Require(setting(USER_SERVICE_INITIAL_USER_ID, &value) == USER_SERVICE_OK && value == expected);
    }
    constexpr std::int32_t keyboardTypeByLanguage[] = {
        37, 20, 26, 21, 17, 28, 29, 33, 34, 38, 39, 40, 23, 35, 16,
        30, 31, 32, 19, 36, 22, 41, 25, 44, 48, 43, 47, 42, 46, 45,
    };
    for (int language = 0; language < 30; ++language) {
        SetLanguage(std::to_string(language).c_str());
        std::int32_t value = 0x5A5A;
        Require(sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, &value) == USER_SERVICE_OK && value == keyboardTypeByLanguage[language]);
        Require(sceUserServiceGetKeyboardType(123, &value) == USER_SERVICE_ERROR_NO_SUCH_USER && value == keyboardTypeByLanguage[language]);
        for (const auto variant : {sceUserServiceGetJapaneseInputType, sceUserServiceGetTraditionalChineseInputType}) {
            value = 0x5A5A;
            Require(variant(USER_SERVICE_INITIAL_USER_ID, &value) == USER_SERVICE_OK && value == 0);
        }
    }
    SetLanguage("30");
    std::int32_t fallback = 0x5A5A;
    Require(sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, &fallback) == USER_SERVICE_OK && fallback == 20);
    for (const char* malformed : {"arabic", "-1", "31", "1x", ""}) {
        SetLanguage(malformed);
        std::int32_t untouched = 0x5A5A;
        bool rejected = false;
        try {
            sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, &untouched);
        } catch (const std::runtime_error& error) {
            rejected = std::string(error.what()).find("ANYPS5_LANGUAGE") != std::string::npos;
        }
        const bool configured = malformed[0] != '\0';
        Require(rejected == configured && untouched == (configured ? 0x5A5A : 20));
        untouched = 0x5A5A;
        Require(sceUserServiceGetKeyboardType(123, &untouched) == USER_SERVICE_ERROR_NO_SUCH_USER && untouched == 0x5A5A);
        Require(sceUserServiceGetJapaneseInputType(USER_SERVICE_INITIAL_USER_ID, &untouched) == USER_SERVICE_OK && untouched == 0);
    }
#if defined(__linux__)
    for (const char* name : {"LC_MESSAGES", "LANG", "LANGUAGE"}) Require(::unsetenv(name) == 0);
    Require(::setenv("LC_ALL", "de_DE.UTF-8", 1) == 0);
    SetLanguage("auto");
    std::int32_t german = 0x5A5A;
    Require(sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, &german) == USER_SERVICE_OK && german == keyboardTypeByLanguage[4]);
    Require(::setenv("LC_ALL", "ja_JP.UTF-8", 1) == 0);
    german = 0x5A5A;
    Require(sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, &german) == USER_SERVICE_OK && german == keyboardTypeByLanguage[4]);
#endif
    SetLanguage(nullptr);

    const int absent_users[] = {0, USER_SERVICE_USER_ID_INVALID, 0xff, 0x12345678};
    using UnmeasuredSetting = int (APS5_VABI *)(std::int32_t, std::int32_t*);
    const UnmeasuredSetting unmeasuredSettings[] = {
        sceUserServiceGetAccessibilityKeyremapEnable,
        sceUserServiceGetAccessibilityZoom,
    };
    for (const UnmeasuredSetting& get : unmeasuredSettings) {
        std::int32_t value = 0x5a5a5a5a;
        Require(get(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        for (int user_id : absent_users) {
            Require(get(user_id, &value) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
            Require(value == 0x5a5a5a5a);
            Require(get(user_id, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        }
        bool threw = false;
        try {
            get(USER_SERVICE_INITIAL_USER_ID, &value);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }

    VerifyLoggedInUser(USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        VerifyLoggedOutUser(USER_SERVICE_INITIAL_USER_ID + i);
    }

    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        const int userId = USER_SERVICE_INITIAL_USER_ID + i;
        UserServiceSetUserConnected_nid_postfix(i, true);
        Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
        Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
        Require(registered.user_id[i] == userId);
        VerifyLoggedInUser(userId);

        UserServiceSetUserConnected_nid_postfix(i, false);
        Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
        VerifyLoggedOutUser(userId);
    }

    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, true);
    }
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_INITIAL_USER_ID + i);
        VerifyLoggedInUser(USER_SERVICE_INITIAL_USER_ID + i);
    }
    for (int i = USER_SERVICE_MAX_USERS; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    UserServiceSetUserConnected_nid_postfix(2, false);
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(registered.user_id[1] == USER_SERVICE_INITIAL_USER_ID + 1);
    Require(registered.user_id[2] == USER_SERVICE_USER_ID_INVALID);
    Require(registered.user_id[3] == USER_SERVICE_INITIAL_USER_ID + 3);
    VerifyLoggedInUser(USER_SERVICE_INITIAL_USER_ID);
    VerifyLoggedInUser(USER_SERVICE_INITIAL_USER_ID + 1);
    VerifyLoggedOutUser(USER_SERVICE_INITIAL_USER_ID + 2);
    VerifyLoggedInUser(USER_SERVICE_INITIAL_USER_ID + 3);

    UserServiceSetUserConnected_nid_postfix(1, false);
    UserServiceSetUserConnected_nid_postfix(3, false);
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }
    VerifyLoggedInUser(USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        VerifyLoggedOutUser(USER_SERVICE_INITIAL_USER_ID + i);
    }

    UserServiceSetUserConnected_nid_postfix(0, false);
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    for (int i = 0; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        VerifyLoggedOutUser(USER_SERVICE_INITIAL_USER_ID + i);
    }

    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK && initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetForegroundUser(&foreground) == USER_SERVICE_OK && foreground == USER_SERVICE_INITIAL_USER_ID);

    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }
    VerifyLoggedInUser(USER_SERVICE_INITIAL_USER_ID);

    const int outOfBoundsIndices[] = {-10, -1, USER_SERVICE_MAX_USERS, USER_SERVICE_MAX_USERS + 1, 100};
    for (int index : outOfBoundsIndices) {
        UserServiceSetUserConnected_nid_postfix(index, true);
        UserServiceSetUserConnected_nid_postfix(index, false);
    }
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }
    VerifyLoggedInUser(USER_SERVICE_INITIAL_USER_ID);

    return 0;
}

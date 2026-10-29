#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void SetLanguage(const char* value) {
#ifdef _WIN32
    Require(_putenv_s("ANYPS5_LANGUAGE", value != nullptr ? value : "") == 0);
#else
    Require((value != nullptr ? ::setenv("ANYPS5_LANGUAGE", value, 1) : ::unsetenv("ANYPS5_LANGUAGE")) == 0);
#endif
}

}

int main() {
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    int32_t userNumber = -1;
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserNumber(123, &userNumber) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, &userNumber) == USER_SERVICE_OK);
    Require(userNumber == 1);

    int32_t privacy = -1;
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID, &privacy) == USER_SERVICE_OK);
    Require(privacy == 0);
    privacy = -1;
    Require(sceUserServiceGetPlatformPrivacyWs1(123, &privacy) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(privacy == -1);
    Require(sceUserServiceGetPlatformPrivacyWs1(123, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

    int foreground = -1;
    Require(sceUserServiceGetForegroundUser(&foreground) == USER_SERVICE_OK);
    Require(foreground == USER_SERVICE_INITIAL_USER_ID);

    Require(sceUserServiceGetRegisteredUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    UserServiceRegisteredUserIdList registered{};
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }

    int color = -1;
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserColor(123, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, &color) == USER_SERVICE_OK);

    SetLanguage(nullptr);
    using KeyboardSetting = decltype(&sceUserServiceGetKeyboardType);
    const std::pair<KeyboardSetting, std::int32_t> keyboardSettings[] = {
        {sceUserServiceGetKeyboardType, 20}, {sceUserServiceGetJapaneseInputType, 0}, {sceUserServiceGetTraditionalChineseInputType, 0}};
    for (const auto& [setting, expected] : keyboardSettings) {
        std::int32_t value = 0x5A5A;
        Require(setting(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(setting(123, &value) == USER_SERVICE_ERROR_UNKNOWN_USER && value == 0x5A5A);
        Require(setting(USER_SERVICE_USER_ID_INVALID, &value) == USER_SERVICE_ERROR_UNKNOWN_USER && value == 0x5A5A);
        Require(setting(USER_SERVICE_INITIAL_USER_ID, &value) == USER_SERVICE_OK && value == expected);
    }
    constexpr std::int32_t keyboardTypeByLanguage[] = {37, 20, 26, 21, 17, 28, 29, 33, 34, 38, 39, 40, 23, 35, 16, 30, 31, 32, 19, 36, 22, 41, 25, 44, 48, 43, 47, 42, 46, 45};
    for (int language = 0; language < 30; ++language) {
        SetLanguage(std::to_string(language).c_str());
        std::int32_t value = 0x5A5A;
        Require(sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, &value) == USER_SERVICE_OK && value == keyboardTypeByLanguage[language]);
        Require(sceUserServiceGetKeyboardType(123, &value) == USER_SERVICE_ERROR_UNKNOWN_USER && value == keyboardTypeByLanguage[language]);
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
        Require(sceUserServiceGetKeyboardType(123, &untouched) == USER_SERVICE_ERROR_UNKNOWN_USER && untouched == 0x5A5A);
        Require(sceUserServiceGetJapaneseInputType(USER_SERVICE_INITIAL_USER_ID, &untouched) == USER_SERVICE_OK && untouched == 0);
    }
    SetLanguage(nullptr);
    Require(color == 0);

    std::uint64_t account_id = 0xdeadbeefu;
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(123, &account_id) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, &account_id) == USER_SERVICE_OK);
    Require(account_id == 0);

    std::uint32_t age_level = 0x5a5a5a5au;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    const int absent_users[] = {0, USER_SERVICE_USER_ID_INVALID, 0xff, 0x12345678};
    for (int user_id : absent_users) {
        Require(sceUserServiceGetAgeLevel(user_id, &age_level) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(age_level == 0x5a5a5a5au);
        Require(sceUserServiceGetAgeLevel(user_id, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        privacy = 0x5a5a;
        Require(sceUserServiceGetPlatformPrivacyWs1(user_id, &privacy) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(privacy == 0x5a5a);
        Require(sceUserServiceGetPlatformPrivacyWs1(user_id, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &age_level) == USER_SERVICE_OK);
    Require(age_level == 0xffffffffu);

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
    for (const Setting& setting : settings) {
        std::int32_t value = 0x5a5a5a5a;
        Require(setting.get(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        for (int user_id : absent_users) {
            Require(setting.get(user_id, &value) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
            Require(value == 0x5a5a5a5a);
            Require(setting.get(user_id, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        }
        Require(setting.get(USER_SERVICE_INITIAL_USER_ID, &value) == USER_SERVICE_OK);
        Require(value == setting.value);
    }
    int initialUser = -1;
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK && initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceGetGamePresets(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    UserServiceGamePresets presets{};
    presets.difficulty = 0x5a;
    Require(sceUserServiceGetGamePresets(0, &presets) == USER_SERVICE_ERROR_INVALID_ARGUMENT && presets.difficulty == 0x5a);
    Require(sceUserServiceGetGamePresets(USER_SERVICE_INITIAL_USER_ID, &presets) == USER_SERVICE_OK && presets.difficulty == 0);
    std::int32_t accessibilityValue = 0;
    bool threwKeyremap = false;
    try {
        sceUserServiceGetAccessibilityKeyremapEnable(USER_SERVICE_INITIAL_USER_ID, &accessibilityValue);
    } catch (const std::runtime_error&) {
        threwKeyremap = true;
    }
    Require(threwKeyremap);

    bool threwZoom = false;
    try {
        sceUserServiceGetAccessibilityZoom(USER_SERVICE_INITIAL_USER_ID, &accessibilityValue);
    } catch (const std::runtime_error&) {
        threwZoom = true;
    }
    Require(threwZoom);

    char name[32]{};
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(123, name, sizeof(name)) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, 5) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "Player") == 0);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID + 1, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "Player2") == 0);

    std::int32_t number = -1;
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserNumber(123, &number) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, &number) == USER_SERVICE_OK);
    Require(number == 1);
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID + 1, &number) == USER_SERVICE_OK);
    Require(number == 2);

    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID + 1, &color) == USER_SERVICE_OK);
    Require(color == 1);

    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID + 1, &age_level) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID + 1, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    privacy = 0x5a5a;
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID + 1, &privacy) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(privacy == 0x5a5a);
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID + 1, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    for (const Setting& setting : settings) {
        std::int32_t value = 0x5a5a5a5a;
        Require(setting.get(USER_SERVICE_INITIAL_USER_ID + 1, &value) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(value == 0x5a5a5a5a);
        Require(setting.get(USER_SERVICE_INITIAL_USER_ID + 1, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }

    UserServiceSetUserConnected_nid_postfix(1, true);
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    Require(registered.user_id[1] == USER_SERVICE_INITIAL_USER_ID + 1);
    for (int i = 2; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID + 1, &age_level) == USER_SERVICE_OK);
    Require(age_level == 0xffffffffu);
    privacy = 0x5a5a;
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID + 1, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID + 1, &privacy) == USER_SERVICE_OK);
    Require(privacy == 0);
    for (const Setting& setting : settings) {
        std::int32_t value = 0x5a5a5a5a;
        Require(setting.get(USER_SERVICE_INITIAL_USER_ID + 1, &value) == USER_SERVICE_OK);
        Require(value == setting.value);
    }

    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceGetRegisteredUserIdList(&registered) == USER_SERVICE_OK);
    Require(registered.user_id[0] == USER_SERVICE_INITIAL_USER_ID);
    for (int i = 1; i < 16; ++i) {
        Require(registered.user_id[i] == USER_SERVICE_USER_ID_INVALID);
    }
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID + 1, &age_level) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID + 1, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    privacy = 0x5a5a;
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID + 1, &privacy) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(privacy == 0x5a5a);
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID + 1, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    for (const Setting& setting : settings) {
        std::int32_t value = 0x5a5a5a5a;
        Require(setting.get(USER_SERVICE_INITIAL_USER_ID + 1, &value) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(value == 0x5a5a5a5a);
        Require(setting.get(USER_SERVICE_INITIAL_USER_ID + 1, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }
}

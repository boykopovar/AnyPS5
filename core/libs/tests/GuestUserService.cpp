#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

extern "C" {
int APS5_VABI sceUserServiceGetUserNumber(int user_id, int32_t* number);
int APS5_VABI sceUserServiceGetPlatformPrivacyWs1(int32_t user_id, int32_t* value);
int APS5_VABI sceUserServiceGetForegroundUser(int* user_id);
int APS5_VABI sceUserServiceGetRegisteredUserIdList(UserServiceRegisteredUserIdList* user_id_list);
int APS5_VABI sceUserServiceGetUserColor(int user_id, int* color);
int APS5_VABI sceUserServiceGetKeyboardType(int user_id, std::int32_t* type);
int APS5_VABI sceUserServiceGetJapaneseInputType(int user_id, std::int32_t* type);
int APS5_VABI sceUserServiceGetTraditionalChineseInputType(int user_id, std::int32_t* type);
int APS5_VABI sceUserServiceGetNpAccountId(int user_id, std::uint64_t* account_id);
int APS5_VABI sceUserServiceGetAgeLevel(int user_id, std::uint32_t* age_level);
int APS5_VABI sceUserServiceGetAccessibilityChatTranscription(int user_id, std::int32_t* value);
int APS5_VABI sceUserServiceGetAccessibilityKeyremapEnable();
int APS5_VABI sceUserServiceGetAccessibilityPressAndHoldDelay(int user_id, std::int32_t* value);
int APS5_VABI sceUserServiceGetAccessibilityTriggerEffect(int user_id, std::int32_t* value);
int APS5_VABI sceUserServiceGetAccessibilityVibration(int user_id, std::int32_t* value);
int APS5_VABI sceUserServiceGetAccessibilityZoom();
int APS5_VABI sceUserServiceGetAccessibilityZoomEnabled(int user_id, std::int32_t* value);
int APS5_VABI sceUserServiceGetAccessibilityZoomFollowFocus(int user_id, std::int32_t* value);
}

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
    bool threwKeyremap = false;
    try {
        sceUserServiceGetAccessibilityKeyremapEnable();
    } catch (const std::runtime_error&) {
        threwKeyremap = true;
    }
    Require(threwKeyremap);

    bool threwZoom = false;
    try {
        sceUserServiceGetAccessibilityZoom();
    } catch (const std::runtime_error&) {
        threwZoom = true;
    }
    Require(threwZoom);
}

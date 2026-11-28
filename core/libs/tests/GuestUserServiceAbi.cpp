#include "prx/libSceUserService/UserService.hpp"
#include "prx/libSceUserService/include/Exports.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <type_traits>

extern "C" {

int APS5_VABI sceUserServiceGetNotificationSettings_4_nid_no_patch(std::int32_t userId, std::uint32_t* settings);
int APS5_VABI sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(std::int32_t userId, char* history, std::size_t size);
int APS5_VABI sceUserServiceSetA11yControllerInitialSetupProgress_nid_no_patch(std::int32_t userId, std::int32_t progress);
int APS5_VABI sceUserServiceSetA11yControllerLedBrightness_nid_no_patch(std::int32_t userId, std::int32_t brightness);
int APS5_VABI sceUserServiceSetHmd2HideCameraImage_nid_no_patch(std::int32_t userId, std::int32_t hide);
int APS5_VABI sceUserServiceSetNotificationSettings_4_nid_no_patch(std::int32_t userId, std::uint32_t settings);
int APS5_VABI sceUserServiceSetPartyGameIconInstallHistory_nid_no_patch(std::int32_t userId, const char* history);
int APS5_VABI sceUserServiceSetProControllerBtAddress_nid_no_patch(std::int32_t userId, std::uint64_t btAddress);

}

using LoginSignature = int (APS5_VABI *)(int, std::uint64_t);
using LogoutSignature = int (APS5_VABI *)(int, int);
using DestroyUserSignature = int (APS5_VABI *)(int);
using SetInt32SettingSignature = int (APS5_VABI *)(std::int32_t, std::int32_t);
using SetUserHearingAssistSignature = int (APS5_VABI *)(std::int32_t);

static_assert(std::is_same_v<decltype(&sceUserServiceLogin), LoginSignature>);
static_assert(std::is_same_v<decltype(&sceUserServiceLogout), LogoutSignature>);
static_assert(std::is_same_v<decltype(&sceUserServiceDestroyUser), DestroyUserSignature>);
static_assert(std::is_same_v<decltype(&sceUserServiceSetAccessibilityKeyremapEnableAston), SetInt32SettingSignature>);
static_assert(std::is_same_v<decltype(&sceUserServiceSetHmd2CinematicAudioRotation), SetInt32SettingSignature>);
static_assert(std::is_same_v<decltype(&sceUserServiceSetIsPersonalizedHrtfEnabled), SetInt32SettingSignature>);
static_assert(std::is_same_v<decltype(&sceUserServiceSetUserWithHearingAssist), SetUserHearingAssistSignature>);

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

void CheckPreInitBehavior() {
    std::uint32_t notificationSettings = 0xA5A5A5A5u;
    Require(sceUserServiceGetNotificationSettings_4_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNotificationSettings_4_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, &notificationSettings) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(notificationSettings == 0xA5A5A5A5u);
    Require(sceUserServiceSetNotificationSettings_4_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 1u) == USER_SERVICE_ERROR_NOT_INITIALIZED);

    char history[16];
    std::memset(history, 0xA5, sizeof(history));
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(history)) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, history, sizeof(history)) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(static_cast<unsigned char>(history[0]) == 0xA5u);
    Require(sceUserServiceSetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceSetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, "item") == USER_SERVICE_ERROR_NOT_INITIALIZED);

    Require(sceUserServiceSetA11yControllerInitialSetupProgress_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 1) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceSetA11yControllerLedBrightness_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 2) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceSetAccessibilityKeyremapEnableAston(USER_SERVICE_INITIAL_USER_ID, 1) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceSetHmd2CinematicAudioRotation(USER_SERVICE_INITIAL_USER_ID, 1) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceSetHmd2HideCameraImage_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 1) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceSetIsPersonalizedHrtfEnabled(USER_SERVICE_INITIAL_USER_ID, 1) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceSetProControllerBtAddress_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 0x112233445566ull) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceSetUserWithHearingAssist(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    std::int32_t hearingAssistUser = 0x12345678;
    Require(sceUserServiceGetUserWithHearingAssist(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetUserWithHearingAssist(&hearingAssistUser) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(hearingAssistUser == 0x12345678);
}

void CheckPostInitBehavior() {
    std::uint32_t notificationSettings = 0xA5A5A5A5u;
    Require(sceUserServiceGetNotificationSettings_4_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNotificationSettings_4_nid_no_patch(0, &notificationSettings) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(notificationSettings == 0xA5A5A5A5u);
    errno = 71;
    Require(sceUserServiceGetNotificationSettings_4_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, reinterpret_cast<std::uint32_t*>(0x10)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(errno == 71);
    Require(sceUserServiceGetNotificationSettings_4_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, &notificationSettings) == USER_SERVICE_OK);
    Require(notificationSettings == 0u);
    Require(sceUserServiceSetNotificationSettings_4_nid_no_patch(0, 0x12345678u) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetNotificationSettings_4_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 0x12345678u) == USER_SERVICE_OK);
    Require(sceUserServiceGetNotificationSettings_4_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, &notificationSettings) == USER_SERVICE_OK);
    Require(notificationSettings == 0x12345678u);

    char history[32];
    std::memset(history, 0xA5, sizeof(history));
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, nullptr, sizeof(history)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(0, history, sizeof(history)) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(static_cast<unsigned char>(history[0]) == 0xA5u);
    errno = 71;
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, reinterpret_cast<char*>(0x10), sizeof(history)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(errno == 71);
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, history, 0) == USER_SERVICE_OK);
    Require(static_cast<unsigned char>(history[0]) == 0xA5u);
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, history, sizeof(history)) == USER_SERVICE_OK);
    Require(history[0] == '\0');

    Require(sceUserServiceSetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    errno = 71;
    Require(sceUserServiceSetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, reinterpret_cast<const char*>(0x10)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(errno == 71);
    Require(sceUserServiceSetPartyGameIconInstallHistory_nid_no_patch(0, "title-icon") == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, "title-icon") == USER_SERVICE_OK);
    std::memset(history, 0xA5, sizeof(history));
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, history, sizeof(history)) == USER_SERVICE_OK);
    Require(std::strcmp(history, "title-icon") == 0);
    std::memset(history, 0xA5, sizeof(history));
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, history, 6) == USER_SERVICE_OK);
    Require(std::strcmp(history, "title") == 0);

    Require(sceUserServiceSetA11yControllerInitialSetupProgress_nid_no_patch(0, 1) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetA11yControllerInitialSetupProgress_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 3) == USER_SERVICE_OK);
    Require(sceUserServiceSetA11yControllerLedBrightness_nid_no_patch(0, 1) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetA11yControllerLedBrightness_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 2) == USER_SERVICE_OK);
    Require(sceUserServiceSetAccessibilityKeyremapEnableAston(0, 1) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetAccessibilityKeyremapEnableAston(USER_SERVICE_INITIAL_USER_ID, 1) == USER_SERVICE_OK);
    Require(sceUserServiceSetHmd2CinematicAudioRotation(0, 1) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetHmd2CinematicAudioRotation(USER_SERVICE_INITIAL_USER_ID, 1) == USER_SERVICE_OK);
    Require(sceUserServiceSetHmd2HideCameraImage_nid_no_patch(0, 1) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetHmd2HideCameraImage_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 1) == USER_SERVICE_OK);
    Require(sceUserServiceSetIsPersonalizedHrtfEnabled(0, 1) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetIsPersonalizedHrtfEnabled(USER_SERVICE_INITIAL_USER_ID, 1) == USER_SERVICE_OK);
    Require(sceUserServiceSetProControllerBtAddress_nid_no_patch(0, 0x112233445566ull) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetProControllerBtAddress_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, 0x112233445566ull) == USER_SERVICE_OK);
    std::int32_t hearingAssistUser = 0x12345678;
    Require(sceUserServiceGetUserWithHearingAssist(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    errno = 71;
    Require(sceUserServiceGetUserWithHearingAssist(reinterpret_cast<std::int32_t*>(0x10)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(errno == 71);
    Require(sceUserServiceGetUserWithHearingAssist(&hearingAssistUser) == USER_SERVICE_OK);
    Require(hearingAssistUser == USER_SERVICE_USER_ID_INVALID);

    Require(sceUserServiceSetUserWithHearingAssist(0) == USER_SERVICE_ERROR_UNKNOWN_USER);
    Require(sceUserServiceSetUserWithHearingAssist(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_OK);
    Require(sceUserServiceGetUserWithHearingAssist(&hearingAssistUser) == USER_SERVICE_OK);
    Require(hearingAssistUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceSetUserWithHearingAssist(USER_SERVICE_USER_ID_INVALID) == USER_SERVICE_OK);
    Require(sceUserServiceGetUserWithHearingAssist(&hearingAssistUser) == USER_SERVICE_OK);
    Require(hearingAssistUser == USER_SERVICE_USER_ID_INVALID);
    Require(sceUserServiceSetUserWithHearingAssist(USER_SERVICE_INITIAL_USER_ID) == USER_SERVICE_OK);
}

void CheckLifecycleReset() {
    std::int32_t hearingAssistUser = 0x12345678;
    Require(sceUserServiceGetUserWithHearingAssist(&hearingAssistUser) == USER_SERVICE_OK);
    Require(hearingAssistUser == USER_SERVICE_USER_ID_INVALID);

    std::uint32_t notificationSettings = 0xA5A5A5A5u;
    Require(sceUserServiceGetNotificationSettings_4_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, &notificationSettings) == USER_SERVICE_OK);
    Require(notificationSettings == 0u);

    char history[32];
    std::memset(history, 0xA5, sizeof(history));
    Require(sceUserServiceGetPartyGameIconInstallHistory_nid_no_patch(USER_SERVICE_INITIAL_USER_ID, history, sizeof(history)) == USER_SERVICE_OK);
    Require(history[0] == '\0');
}

} // namespace

int main() {
    CheckPreInitBehavior();
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    CheckPostInitBehavior();
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    CheckPreInitBehavior();
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    CheckLifecycleReset();
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    return 0;
}

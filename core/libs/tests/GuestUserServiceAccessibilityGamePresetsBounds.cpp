#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
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

void TestGamePresetsPrecedenceAndDefaults() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    Require(sceUserServiceGetGamePresets(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    const int unloggedIds[] = {
        USER_SERVICE_INITIAL_USER_ID + 1,
        USER_SERVICE_INITIAL_USER_ID + 2,
        USER_SERVICE_INITIAL_USER_ID + 3,
        0,
        1,
        -1,
        USER_SERVICE_USER_ID_INVALID,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        0x20000000,
        static_cast<int>(0xFFFFFFFFu)
    };

    for (int unloggedId : unloggedIds) {
        Require(sceUserServiceGetGamePresets(unloggedId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        UserServiceGamePresets presets{};
        std::memset(&presets, 0x5A, sizeof(presets));
        Require(sceUserServiceGetGamePresets(unloggedId, &presets) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }

    UserServiceGamePresets presets{};
    std::memset(&presets, 0x5A, sizeof(presets));
    Require(sceUserServiceGetGamePresets(USER_SERVICE_INITIAL_USER_ID, &presets) == USER_SERVICE_OK);
    Require(presets.this_size == 0);
    Require(presets.difficulty == 0);
    Require(presets.priority == 0);
    Require(presets.invert_vertical_view_for_1st_person_view == 0);
    Require(presets.invert_horizontal_view_for_1st_person_view == 0);
    Require(presets.invert_vertical_view_for_3rd_person_view == 0);
    Require(presets.invert_horizontal_view_for_3rd_person_view == 0);
    Require(presets.display_sub_titles == 0);
    Require(presets.audio_language == 0);

    UserServiceSetUserConnected_nid_postfix(1, true);
    std::memset(&presets, 0x5A, sizeof(presets));
    Require(sceUserServiceGetGamePresets(USER_SERVICE_INITIAL_USER_ID + 1, &presets) == USER_SERVICE_OK);
    Require(presets.this_size == 0);
    Require(presets.difficulty == 0);
    Require(presets.priority == 0);
    Require(presets.invert_vertical_view_for_1st_person_view == 0);
    Require(presets.invert_horizontal_view_for_1st_person_view == 0);
    Require(presets.invert_vertical_view_for_3rd_person_view == 0);
    Require(presets.invert_horizontal_view_for_3rd_person_view == 0);
    Require(presets.display_sub_titles == 0);
    Require(presets.audio_language == 0);

    UserServiceSetUserConnected_nid_postfix(1, false);
    Require(sceUserServiceGetGamePresets(USER_SERVICE_INITIAL_USER_ID + 1, &presets) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
}

void TestAccessibilitySettingsPrecedenceAndDefaults() {
    const int unloggedIds[] = {
        USER_SERVICE_INITIAL_USER_ID + 1,
        USER_SERVICE_INITIAL_USER_ID + 2,
        USER_SERVICE_INITIAL_USER_ID + 3,
        0,
        1,
        -1,
        USER_SERVICE_USER_ID_INVALID,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        0x20000000,
        static_cast<int>(0xFFFFFFFFu)
    };

    for (int unloggedId : unloggedIds) {
        Require(sceUserServiceGetAccessibilityChatTranscription(unloggedId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityPressAndHoldDelay(unloggedId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityTriggerEffect(unloggedId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityVibration(unloggedId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityZoomEnabled(unloggedId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityZoomFollowFocus(unloggedId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetPlatformPrivacyWs1(unloggedId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityKeyremapEnable(unloggedId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityZoom(unloggedId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        int32_t val = 0x5A5A;
        Require(sceUserServiceGetAccessibilityChatTranscription(unloggedId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(val == 0x5A5A);
        Require(sceUserServiceGetAccessibilityPressAndHoldDelay(unloggedId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(val == 0x5A5A);
        Require(sceUserServiceGetAccessibilityTriggerEffect(unloggedId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(val == 0x5A5A);
        Require(sceUserServiceGetAccessibilityVibration(unloggedId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(val == 0x5A5A);
        Require(sceUserServiceGetAccessibilityZoomEnabled(unloggedId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(val == 0x5A5A);
        Require(sceUserServiceGetAccessibilityZoomFollowFocus(unloggedId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(val == 0x5A5A);
        Require(sceUserServiceGetPlatformPrivacyWs1(unloggedId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(val == 0x5A5A);
        Require(sceUserServiceGetAccessibilityKeyremapEnable(unloggedId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(val == 0x5A5A);
        Require(sceUserServiceGetAccessibilityZoom(unloggedId, &val) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(val == 0x5A5A);
    }

    Require(sceUserServiceGetAccessibilityChatTranscription(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAccessibilityPressAndHoldDelay(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAccessibilityTriggerEffect(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAccessibilityVibration(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAccessibilityZoomEnabled(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAccessibilityZoomFollowFocus(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAccessibilityKeyremapEnable(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetAccessibilityZoom(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    int32_t chat = -1;
    Require(sceUserServiceGetAccessibilityChatTranscription(USER_SERVICE_INITIAL_USER_ID, &chat) == USER_SERVICE_OK);
    Require(chat == 0);

    int32_t delay = -1;
    Require(sceUserServiceGetAccessibilityPressAndHoldDelay(USER_SERVICE_INITIAL_USER_ID, &delay) == USER_SERVICE_OK);
    Require(delay == 0);

    int32_t trigger = -1;
    Require(sceUserServiceGetAccessibilityTriggerEffect(USER_SERVICE_INITIAL_USER_ID, &trigger) == USER_SERVICE_OK);
    Require(trigger == 1);

    int32_t vib = -1;
    Require(sceUserServiceGetAccessibilityVibration(USER_SERVICE_INITIAL_USER_ID, &vib) == USER_SERVICE_OK);
    Require(vib == 1);

    int32_t zoomEnabled = -1;
    Require(sceUserServiceGetAccessibilityZoomEnabled(USER_SERVICE_INITIAL_USER_ID, &zoomEnabled) == USER_SERVICE_OK);
    Require(zoomEnabled == 0);

    int32_t zoomFocus = -1;
    Require(sceUserServiceGetAccessibilityZoomFollowFocus(USER_SERVICE_INITIAL_USER_ID, &zoomFocus) == USER_SERVICE_OK);
    Require(zoomFocus == 1);

    int32_t privacy = -1;
    Require(sceUserServiceGetPlatformPrivacyWs1(USER_SERVICE_INITIAL_USER_ID, &privacy) == USER_SERVICE_OK);
    Require(privacy == 0);
}

void TestCleanTermination() {
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

} // namespace

int main() {
    TestGamePresetsPrecedenceAndDefaults();
    TestAccessibilitySettingsPrecedenceAndDefaults();
    TestCleanTermination();
    return 0;
}

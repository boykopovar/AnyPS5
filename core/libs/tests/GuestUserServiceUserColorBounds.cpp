#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libSceUserService/include/LegacyExports.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename Func>
void RequireThrows(Func func) {
    bool caught = false;
    try {
        func();
    } catch (const std::runtime_error&) {
        caught = true;
    }
    Require(caught);
}

void TestNullPointerBounds() {
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID + i, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    const int invalidIds[] = {
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        2,
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS + 1,
        0x20000000,
        0x30000000,
        INT_MIN,
        INT_MAX
    };

    for (int id : invalidIds) {
        Require(sceUserServiceGetUserColor(id, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

void TestValidUserColorMapping() {
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        int color = -1;
        Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID + i, &color) == USER_SERVICE_OK);
        Require(color == i);

        color = 0x5A5A5A5A;
        Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID + i, &color) == USER_SERVICE_OK);
        Require(color == i);
    }
}

void TestInvalidUserIdBounds() {
    const int invalidIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID - 100,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS + 1,
        USER_SERVICE_INITIAL_USER_ID + 1000,
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        2,
        -2,
        -100,
        INT_MIN,
        INT_MAX,
        0x20000000,
        0x20000001,
        0x2FFFFFFF,
        0x30000000,
        0x30000001,
        0x3FFFFFFF,
        0x40000000,
        static_cast<int>(0x80000000u),
        static_cast<int>(0xFFFFFFFEu)
    };

    for (int id : invalidIds) {
        int color = 0x5A5A5A5A;
        Require(sceUserServiceGetUserColor(id, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(color == 0x5A5A5A5A);
    }
}

void TestLoginStateAndConnectionDynamics() {
    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, false);
        int color = -1;
        Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID + i, &color) == USER_SERVICE_OK);
        Require(color == i);

        UserServiceSetUserConnected_nid_postfix(i, true);
        color = -1;
        Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID + i, &color) == USER_SERVICE_OK);
        Require(color == i);
    }

    UserServiceSetUserConnected_nid_postfix(0, true);
    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, false);
    }
}

void TestLifecycleInvariance() {
    int color = -1;
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, &color) == USER_SERVICE_OK);
    Require(color == 0);

    UserServiceInitParam params{};
    params.priority = USER_SERVICE_THREAD_PRIORITY_MIN;
    Require(sceUserServiceInitialize(&params) == USER_SERVICE_OK);

    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        color = -1;
        Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID + i, &color) == USER_SERVICE_OK);
        Require(color == i);
    }
    Require(sceUserServiceGetUserColor(USER_SERVICE_USER_ID_INVALID, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);

    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        color = -1;
        Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID + i, &color) == USER_SERVICE_OK);
        Require(color == i);
    }
    Require(sceUserServiceGetUserColor(USER_SERVICE_USER_ID_INVALID, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, &color) == USER_SERVICE_OK);
    Require(color == 0);
    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
}

void TestUnimplementedColorAndThemeExports() {
    RequireThrows([] { static_cast<void>(sceUserServiceSetGlsBroadcastersCommentColor()); });
    RequireThrows([] { static_cast<void>(sceUserServiceGetGlsBroadcastersCommentColor()); });
    RequireThrows([] { static_cast<void>(sceUserServiceSetLightBarBaseBrightness()); });
    RequireThrows([] { static_cast<void>(sceUserServiceGetLightBarBaseBrightness()); });
    RequireThrows([] { static_cast<void>(sceUserServiceGetThemeBgImageWaveColor()); });
    RequireThrows([] { static_cast<void>(sceUserServiceGetThemeWaveColor()); });
    RequireThrows([] { static_cast<void>(sceUserServiceSetThemeBgImageWaveColor()); });
    RequireThrows([] { static_cast<void>(sceUserServiceSetThemeWaveColor()); });
}

} // namespace

int main() {
    TestNullPointerBounds();
    TestValidUserColorMapping();
    TestInvalidUserIdBounds();
    TestLoginStateAndConnectionDynamics();
    TestLifecycleInvariance();
    TestUnimplementedColorAndThemeExports();
    return 0;
}

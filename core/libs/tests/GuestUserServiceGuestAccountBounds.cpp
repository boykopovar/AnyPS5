#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

std::int32_t MakeId(std::uint32_t value) {
    std::int32_t id = 0;
    std::memcpy(&id, &value, sizeof(id));
    return id;
}

void TestGuestUserClassification() {
    const std::uint32_t guestIds[] = {
        0x20000000u,
        0x20000001u,
        0x20000002u,
        0x20000003u,
        0x20001234u,
        0x2abcdef0u,
        0x2fffffffu,
        0x30000000u,
        0x30000001u,
        0x3abcdef0u,
        0x3fffffffu
    };
    for (const std::uint32_t id : guestIds) {
        Require(sceUserServiceIsGuestUser(MakeId(id)) == 1);
    }

    const std::uint32_t nonGuestIds[] = {
        0u,
        1u,
        2u,
        100u,
        0x0000ffffu,
        0x0fffffffu,
        static_cast<std::uint32_t>(USER_SERVICE_INITIAL_USER_ID),
        0x10000001u,
        0x10000002u,
        0x10000003u,
        0x10000004u,
        0x15555555u,
        0x1ffffffeu,
        0x1fffffffu,
        0x40000000u,
        0x40000001u,
        0x50000000u,
        0x60000000u,
        0x70000000u,
        0x80000000u,
        0x90000000u,
        0xa0000000u,
        0xb0000000u,
        0xc0000000u,
        0xd0000000u,
        0xe0000000u,
        0xf0000000u,
        static_cast<std::uint32_t>(USER_SERVICE_USER_ID_INVALID)
    };
    for (const std::uint32_t id : nonGuestIds) {
        Require(sceUserServiceIsGuestUser(MakeId(id)) == 0);
    }

    Require(sceUserServiceIsGuestUser(USER_SERVICE_INITIAL_USER_ID) == 0);
    Require(sceUserServiceIsGuestUser(USER_SERVICE_USER_ID_INVALID) == 0);
}

void TestSharePlayClientSubsetBounds() {
    const std::uint32_t sharePlayClients[] = {
        0x30000000u,
        0x30000001u,
        0x3abcdef0u,
        0x3fffffffu
    };
    for (const std::uint32_t id : sharePlayClients) {
        Require(sceUserServiceIsSharePlayClientUser(MakeId(id)) == 1);
        Require(sceUserServiceIsGuestUser(MakeId(id)) == 1);
    }

    const std::uint32_t nonSharePlayGuests[] = {
        0x20000000u,
        0x20000001u,
        0x20000002u,
        0x2abcdef0u,
        0x2fffffffu
    };
    for (const std::uint32_t id : nonSharePlayGuests) {
        Require(sceUserServiceIsGuestUser(MakeId(id)) == 1);
        Require(sceUserServiceIsSharePlayClientUser(MakeId(id)) == 0);
    }

    const std::uint32_t neither[] = {
        0u,
        static_cast<std::uint32_t>(USER_SERVICE_INITIAL_USER_ID),
        0x1fffffffu,
        0x40000000u,
        static_cast<std::uint32_t>(USER_SERVICE_USER_ID_INVALID)
    };
    for (const std::uint32_t id : neither) {
        Require(sceUserServiceIsGuestUser(MakeId(id)) == 0);
        Require(sceUserServiceIsSharePlayClientUser(MakeId(id)) == 0);
    }

    Require(sceUserServiceIsSharePlayClientUser(USER_SERVICE_INITIAL_USER_ID) == 0);
    Require(sceUserServiceIsSharePlayClientUser(USER_SERVICE_USER_ID_INVALID) == 0);
}

void TestGuestLoginLifecycleAndListExclusion() {
    const std::int32_t testGuests[] = {
        MakeId(0x20000000u),
        MakeId(0x20000001u),
        MakeId(0x2fffffffu),
        MakeId(0x30000000u),
        MakeId(0x3fffffffu)
    };

    for (const std::int32_t guestId : testGuests) {
        Require(sceUserServiceIsLoggedIn(guestId) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        Require(sceUserServiceIsLoggedInWithoutLock(guestId) == 0);
    }

    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    for (const std::int32_t guestId : testGuests) {
        Require(sceUserServiceIsLoggedIn(guestId) == 0);
        Require(sceUserServiceIsLoggedInWithoutLock(guestId) == 0);
    }

    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, true);
    }

    for (const std::int32_t guestId : testGuests) {
        Require(sceUserServiceIsLoggedIn(guestId) == 0);
        Require(sceUserServiceIsLoggedInWithoutLock(guestId) == 0);
    }

    UserServiceLoginUserIdList loginList{};
    Require(sceUserServiceGetLoginUserIdList(&loginList) == USER_SERVICE_OK);
    for (int i = 0; i < 4; ++i) {
        Require(sceUserServiceIsGuestUser(loginList.user_id[i]) == 0);
    }

    UserServiceRegisteredUserIdList regList{};
    Require(sceUserServiceGetRegisteredUserIdList(&regList) == USER_SERVICE_OK);
    for (int i = 0; i < 16; ++i) {
        Require(sceUserServiceIsGuestUser(regList.user_id[i]) == 0);
    }

    UserServiceSetUserConnected_nid_postfix(0, true);
    for (int i = 1; i < USER_SERVICE_MAX_USERS; ++i) {
        UserServiceSetUserConnected_nid_postfix(i, false);
    }

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);

    for (const std::int32_t guestId : testGuests) {
        Require(sceUserServiceIsLoggedIn(guestId) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        Require(sceUserServiceIsLoggedInWithoutLock(guestId) == 0);
    }
}

void TestGuestAccountUnknownUserQueries() {
    const std::int32_t testGuests[] = {
        MakeId(0x20000000u),
        MakeId(0x20000001u),
        MakeId(0x2fffffffu),
        MakeId(0x30000000u),
        MakeId(0x3fffffffu)
    };

    char nameBuffer[64]{};
    for (const std::int32_t guestId : testGuests) {
        Require(sceUserServiceGetUserName(guestId, nameBuffer, sizeof(nameBuffer)) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(sceUserServiceGetUserName(guestId, nullptr, sizeof(nameBuffer)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(guestId, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int32_t number = -1;
        Require(sceUserServiceGetUserNumber(guestId, &number) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserNumber(guestId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int color = -1;
        Require(sceUserServiceGetUserColor(guestId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserColor(guestId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        uint64_t accountId = 0xCAFEBABE12345678ULL;
        Require(sceUserServiceGetNpAccountId(guestId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(accountId == 0xCAFEBABE12345678ULL);
        Require(sceUserServiceGetNpAccountId(guestId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int32_t keyboardType = -1;
        Require(sceUserServiceGetKeyboardType(guestId, &keyboardType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(sceUserServiceGetKeyboardType(guestId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int32_t japaneseInputType = -1;
        Require(sceUserServiceGetJapaneseInputType(guestId, &japaneseInputType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(sceUserServiceGetJapaneseInputType(guestId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        int32_t traditionalChineseInputType = -1;
        Require(sceUserServiceGetTraditionalChineseInputType(guestId, &traditionalChineseInputType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(sceUserServiceGetTraditionalChineseInputType(guestId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

void TestGuestAccountNotLoggedInGate() {
    const std::int32_t testGuests[] = {
        MakeId(0x20000000u),
        MakeId(0x20000001u),
        MakeId(0x30000000u)
    };

    for (const std::int32_t guestId : testGuests) {
        UserServiceGamePresets presets{};
        Require(sceUserServiceGetGamePresets(guestId, &presets) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetGamePresets(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        uint32_t ageLevel = 0;
        Require(sceUserServiceGetAgeLevel(guestId, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAgeLevel(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        int32_t accessibilityValue = 0;
        Require(sceUserServiceGetAccessibilityKeyremapEnable(guestId, &accessibilityValue) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityKeyremapEnable(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        Require(sceUserServiceGetAccessibilityVibration(guestId, &accessibilityValue) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityVibration(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        Require(sceUserServiceGetAccessibilityTriggerEffect(guestId, &accessibilityValue) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityTriggerEffect(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        Require(sceUserServiceGetAccessibilityZoom(guestId, &accessibilityValue) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityZoom(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        Require(sceUserServiceGetAccessibilityZoomEnabled(guestId, &accessibilityValue) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityZoomEnabled(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        Require(sceUserServiceGetAccessibilityZoomFollowFocus(guestId, &accessibilityValue) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityZoomFollowFocus(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        Require(sceUserServiceGetAccessibilityPressAndHoldDelay(guestId, &accessibilityValue) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityPressAndHoldDelay(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        Require(sceUserServiceGetAccessibilityChatTranscription(guestId, &accessibilityValue) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetAccessibilityChatTranscription(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

        Require(sceUserServiceGetPlatformPrivacyWs1(guestId, &accessibilityValue) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
        Require(sceUserServiceGetPlatformPrivacyWs1(guestId, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    }
}

void TestGuestAccountSpartacusAndKratos() {
    const std::int32_t testGuests[] = {
        MakeId(0x20000000u),
        MakeId(0x20000001u),
        MakeId(0x30000000u)
    };

    for (const std::int32_t guestId : testGuests) {
        Require(sceUserServiceIsKratosPrimaryUser(guestId) == false);

        Require(sceUserServiceGetIsSpartacusEnabled(guestId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

        bool enabled = false;
        Require(sceUserServiceGetIsSpartacusEnabled(guestId, &enabled) == USER_SERVICE_OK);
        Require(enabled == true);
    }
}

void TestGuestAccountHomeDirectoryBounds() {
    const std::uint32_t guestIds[] = {
        0x20000000u,
        0x20000001u,
        0x30000000u
    };

    char path[128]{};
    for (const std::uint32_t guestId : guestIds) {
        Require(sceUserServiceGetHomeDirectory(guestId, nullptr, sizeof(path)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        for (std::size_t size = 0; size <= 19; ++size) {
            Require(sceUserServiceGetHomeDirectory(guestId, path, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        }

        std::memset(path, 0, sizeof(path));
        Require(sceUserServiceGetHomeDirectory(guestId, path, sizeof(path)) == USER_SERVICE_OK);
        char expectedHome[64]{};
        std::snprintf(expectedHome, sizeof(expectedHome), "/user/home/%08x", guestId);
        Require(std::strcmp(path, expectedHome) == 0);

        char securePath[512]{};
        Require(sceUserServiceGetSecureHomeDirectory(guestId, nullptr, sizeof(securePath)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetSecureHomeDirectory(guestId, securePath, 10) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        Require(sceUserServiceGetSecureHomeDirectory(guestId, securePath, sizeof(securePath)) == USER_SERVICE_OK);
        char expectedPrivHome[64]{};
        std::snprintf(expectedPrivHome, sizeof(expectedPrivHome), "/priv/home/%08x", guestId);
        Require(std::strstr(securePath, expectedPrivHome) != nullptr);

        char eapPath[128]{};
        Require(libSceUserServiceUnknown_ErTMkmSho4E_nid_no_patch(guestId, nullptr, sizeof(eapPath)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(libSceUserServiceUnknown_ErTMkmSho4E_nid_no_patch(guestId, eapPath, 0x25) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        Require(libSceUserServiceUnknown_ErTMkmSho4E_nid_no_patch(guestId, eapPath, sizeof(eapPath)) == USER_SERVICE_OK);
        char expectedEapHome[64]{};
        std::snprintf(expectedEapHome, sizeof(expectedEapHome), "/eap/rodata/home/%08x", guestId);
        Require(std::strstr(eapPath, expectedEapHome) != nullptr);
    }
}

void TestSystemPrimaryUserInvariants() {
    int initialUser = -1;
    Require(sceUserServiceGetInitialUser(&initialUser) == USER_SERVICE_OK);
    Require(initialUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceIsGuestUser(initialUser) == 0);
    Require(sceUserServiceIsSharePlayClientUser(initialUser) == 0);

    int foregroundUser = -1;
    Require(sceUserServiceGetForegroundUser(&foregroundUser) == USER_SERVICE_OK);
    Require(foregroundUser == USER_SERVICE_INITIAL_USER_ID);
    Require(sceUserServiceIsGuestUser(foregroundUser) == 0);
    Require(sceUserServiceIsSharePlayClientUser(foregroundUser) == 0);
}

} // namespace

int main() {
    TestGuestUserClassification();
    TestSharePlayClientSubsetBounds();
    TestGuestLoginLifecycleAndListExclusion();
    TestGuestAccountUnknownUserQueries();
    TestGuestAccountNotLoggedInGate();
    TestGuestAccountSpartacusAndKratos();
    TestGuestAccountHomeDirectoryBounds();
    TestSystemPrimaryUserInvariants();
    return 0;
}

#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestAgeLevelLoggedOutAndUnregisteredRejection() {
    const int absentUsers[] = {
        0,
        USER_SERVICE_USER_ID_INVALID,
        1,
        0xFF,
        0x12345678,
        USER_SERVICE_INITIAL_USER_ID + 1,
        USER_SERVICE_INITIAL_USER_ID + 2,
        USER_SERVICE_INITIAL_USER_ID + 3,
        static_cast<int>(0x20000000u),
        static_cast<int>(0x30000000u),
        static_cast<int>(0xFFFFFFFFu)
    };

    for (int userId : absentUsers) {
        std::uint32_t ageLevel = 0x5A5A5A5Au;
        Require(sceUserServiceGetAgeLevel(userId, &ageLevel) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        Require(ageLevel == 0x5A5A5A5Au);
        Require(sceUserServiceGetAgeLevel(userId, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    }
}

void TestAgeLevelLoggedInAndNullBounds() {
    Require(sceUserServiceInitialize(nullptr) == USER_SERVICE_OK);

    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    std::uint32_t ageLevel = 0;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &ageLevel) == USER_SERVICE_OK);
    Require(ageLevel == 0xFFFFFFFFu);

    UserServiceSetUserConnected_nid_postfix(1, true);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID + 1, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    ageLevel = 0;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID + 1, &ageLevel) == USER_SERVICE_OK);
    Require(ageLevel == 0xFFFFFFFFu);

    UserServiceSetUserConnected_nid_postfix(1, false);
    ageLevel = 0x5A5A5A5Au;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID + 1, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(ageLevel == 0x5A5A5A5Au);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID + 1, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

    UserServiceSetUserConnected_nid_postfix(0, false);
    ageLevel = 0x5A5A5A5Au;
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &ageLevel) == USER_SERVICE_ERROR_NOT_LOGGED_IN);
    Require(ageLevel == 0x5A5A5A5Au);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_NOT_LOGGED_IN);

    UserServiceSetUserConnected_nid_postfix(0, true);
    Require(sceUserServiceGetAgeLevel(USER_SERVICE_INITIAL_USER_ID, &ageLevel) == USER_SERVICE_OK);
    Require(ageLevel == 0xFFFFFFFFu);

    Require(sceUserServiceTerminate() == USER_SERVICE_OK);
    Require(sceUserServiceTerminate() == USER_SERVICE_ERROR_NOT_INITIALIZED);
}

void TestGuestUserBitmaskEdgeCases() {
    const std::uint32_t guests[] = {
        0x20000000u,
        0x20000001u,
        0x25000000u,
        0x2FFFFFFFu,
        0x30000000u,
        0x35000000u,
        0x3FFFFFFFu
    };
    for (std::uint32_t guestId : guests) {
        Require(sceUserServiceIsGuestUser(static_cast<std::int32_t>(guestId)) == 1);
    }

    const std::uint32_t nonGuests[] = {
        0u,
        1u,
        0x1FFFFFFFu,
        0x40000000u,
        0x40000001u,
        0x7FFFFFFFu,
        0x80000000u,
        0xFFFFFFFFu
    };
    for (std::uint32_t nonGuestId : nonGuests) {
        Require(sceUserServiceIsGuestUser(static_cast<std::int32_t>(nonGuestId)) == 0);
    }

    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(sceUserServiceIsGuestUser(USER_SERVICE_INITIAL_USER_ID + i) == 0);
    }
    Require(sceUserServiceIsGuestUser(USER_SERVICE_USER_ID_INVALID) == 0);
}

void TestSharePlayClientUserBitmaskEdgeCases() {
    const std::uint32_t sharePlayClients[] = {
        0x30000000u,
        0x30000001u,
        0x35000000u,
        0x3FFFFFFFu
    };
    for (std::uint32_t spId : sharePlayClients) {
        Require(sceUserServiceIsSharePlayClientUser(static_cast<std::int32_t>(spId)) == 1);
        Require(sceUserServiceIsGuestUser(static_cast<std::int32_t>(spId)) == 1);
    }

    const std::uint32_t nonSharePlayClients[] = {
        0u,
        1u,
        0x1FFFFFFFu,
        0x20000000u,
        0x20000001u,
        0x25000000u,
        0x2FFFFFFFu,
        0x40000000u,
        0x40000001u,
        0x7FFFFFFFu,
        0x80000000u,
        0xFFFFFFFFu
    };
    for (std::uint32_t nonSpId : nonSharePlayClients) {
        Require(sceUserServiceIsSharePlayClientUser(static_cast<std::int32_t>(nonSpId)) == 0);
    }

    for (int i = 0; i < USER_SERVICE_MAX_USERS; ++i) {
        Require(sceUserServiceIsSharePlayClientUser(USER_SERVICE_INITIAL_USER_ID + i) == 0);
    }
    Require(sceUserServiceIsSharePlayClientUser(USER_SERVICE_USER_ID_INVALID) == 0);
}

void TestUserQueryBoundsAndNulls() {
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, 32) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetRegisteredUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetEvent(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetJapaneseInputType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetTraditionalChineseInputType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetIsSpartacusEnabled(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    char name[32]{};
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, 16) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, name, 17) == USER_SERVICE_OK);
    Require(std::strcmp(name, USER_SERVICE_INITIAL_USER_NAME) == 0);

    const int invalidIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
        USER_SERVICE_USER_ID_INVALID,
        0,
        1,
        -1,
        static_cast<int>(0x20000000u),
        static_cast<int>(0x30000000u),
        static_cast<int>(0xFFFFFFFFu)
    };
    for (int invalidId : invalidIds) {
        int number = -1;
        Require(sceUserServiceGetUserNumber(invalidId, &number) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        Require(number == -1);

        int color = -1;
        Require(sceUserServiceGetUserColor(invalidId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(color == -1);

        Require(sceUserServiceGetUserName(invalidId, name, sizeof(name)) == USER_SERVICE_ERROR_UNKNOWN_USER);
        int32_t ktype = -1;
        Require(sceUserServiceGetKeyboardType(invalidId, &ktype) == USER_SERVICE_ERROR_NOT_INITIALIZED);
        int32_t jpType = -1;
        Require(sceUserServiceGetJapaneseInputType(invalidId, &jpType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        int32_t zhType = -1;
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, &zhType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        uint64_t accountId = 0xDEADBEEF;
        Require(sceUserServiceGetNpAccountId(invalidId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(accountId == 0xDEADBEEF);
    }
}

} // namespace

int main() {
    TestAgeLevelLoggedOutAndUnregisteredRejection();
    TestAgeLevelLoggedInAndNullBounds();
    TestGuestUserBitmaskEdgeCases();
    TestSharePlayClientUserBitmaskEdgeCases();
    TestUserQueryBoundsAndNulls();
    return 0;
}

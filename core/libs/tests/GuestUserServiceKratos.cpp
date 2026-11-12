#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestKratosUnsupportedState() {
    bool threw = false;
    try {
        sceUserServiceIsKratosUser();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        int userId = -1;
        sceUserServiceGetKratosPrimaryUser(&userId);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceUserServiceGetKratosPrimaryUser(nullptr);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    const int userIds[] = {
        USER_SERVICE_INITIAL_USER_ID,
        USER_SERVICE_INITIAL_USER_ID + 1,
        USER_SERVICE_INITIAL_USER_ID + 2,
        USER_SERVICE_INITIAL_USER_ID + 3,
        USER_SERVICE_USER_ID_INVALID,
        0,
        -1,
        0x20000000,
        0x30000000
    };
    for (int id : userIds) {
        threw = false;
        try {
            sceUserServiceIsKratosPrimaryUser(id);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }
}

void TestInvalidUserIdRejection() {
    const int invalidIds[] = {
        USER_SERVICE_INITIAL_USER_ID - 1,
        USER_SERVICE_INITIAL_USER_ID + USER_SERVICE_MAX_USERS,
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

    for (int invalidId : invalidIds) {
        int number = -1;
        Require(sceUserServiceGetUserNumber(invalidId, &number) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(number == -1);

        int color = -1;
        Require(sceUserServiceGetUserColor(invalidId, &color) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(color == -1);

        char name[32]{};
        Require(sceUserServiceGetUserName(invalidId, name, sizeof(name)) == USER_SERVICE_ERROR_UNKNOWN_USER);

        int32_t ktype = -1;
        Require(sceUserServiceGetKeyboardType(invalidId, &ktype) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(ktype == -1);

        int32_t jpType = -1;
        Require(sceUserServiceGetJapaneseInputType(invalidId, &jpType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(jpType == -1);

        int32_t zhType = -1;
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, &zhType) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(zhType == -1);

        uint64_t accountId = 0xDEADBEEF;
        Require(sceUserServiceGetNpAccountId(invalidId, &accountId) == USER_SERVICE_ERROR_UNKNOWN_USER);
        Require(accountId == 0xDEADBEEF);
    }
}

void TestNullOutputPointers() {
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetForegroundUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserNumber(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserColor(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, 32) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserName(USER_SERVICE_INITIAL_USER_ID, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetRegisteredUserIdList(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetLoginUserIdList(nullptr) == USER_SERVICE_ERROR_NOT_INITIALIZED);
    Require(sceUserServiceGetKeyboardType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetJapaneseInputType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetTraditionalChineseInputType(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetNpAccountId(USER_SERVICE_INITIAL_USER_ID, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (int invalidId : {0, -1, USER_SERVICE_USER_ID_INVALID, 123}) {
        Require(sceUserServiceGetUserNumber(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserColor(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserName(invalidId, nullptr, 32) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetKeyboardType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetJapaneseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetTraditionalChineseInputType(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetNpAccountId(invalidId, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

void TestStandardUserProfileBounds() {
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

        char shortBuf[16]{};
        for (std::size_t size = 0; size <= USER_SERVICE_MAX_USER_NAME_LENGTH; ++size) {
            Require(sceUserServiceGetUserName(userId, shortBuf, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        }

        Require(sceUserServiceIsGuestUser(userId) == 0);
        Require(sceUserServiceIsSharePlayClientUser(userId) == 0);
    }

    const std::uint32_t guests[] = {0x20000000u, 0x20000001u, 0x2abcdef0u, 0x2fffffffu, 0x30000000u, 0x3fffffffu};
    for (std::uint32_t guestId : guests) {
        Require(sceUserServiceIsGuestUser(static_cast<std::int32_t>(guestId)) == 1);
    }

    const std::uint32_t nonGuests[] = {0u, 0x10000000u, 0x10000003u, 0x1fffffffu, 0x40000000u, 0x60000000u, 0xffffffffu};
    for (std::uint32_t nonGuestId : nonGuests) {
        Require(sceUserServiceIsGuestUser(static_cast<std::int32_t>(nonGuestId)) == 0);
    }

    const std::uint32_t sharePlayClients[] = {0x30000000u, 0x30000001u, 0x3abcdef0u, 0x3fffffffu};
    for (std::uint32_t spId : sharePlayClients) {
        Require(sceUserServiceIsSharePlayClientUser(static_cast<std::int32_t>(spId)) == 1);
    }
}

} // namespace

int main() {
    TestKratosUnsupportedState();
    TestInvalidUserIdRejection();
    TestNullOutputPointers();
    TestStandardUserProfileBounds();
    return 0;
}

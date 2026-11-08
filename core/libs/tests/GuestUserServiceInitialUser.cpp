#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT = USER_SERVICE_ERROR_INVALID_ARGUMENT;

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestNullPointer() {
    Require(sceUserServiceGetInitialUser(nullptr) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetInitialUser(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
}

void TestValidInitialUser() {
    int userId = -1;
    Require(sceUserServiceGetInitialUser(&userId) == USER_SERVICE_OK);
    Require(userId >= 0x10000000 && userId <= 0x10000003);
    Require(userId == USER_SERVICE_INITIAL_USER_ID);
}

void TestIdempotency() {
    int baselineId = 0;
    Require(sceUserServiceGetInitialUser(&baselineId) == USER_SERVICE_OK);
    Require(baselineId >= 0x10000000 && baselineId <= 0x10000003);

    for (int i = 0; i < 64; ++i) {
        int tempId = 0x5a5a5a5a + i;
        Require(sceUserServiceGetInitialUser(&tempId) == USER_SERVICE_OK);
        Require(tempId == baselineId);
        Require(tempId >= 0x10000000 && tempId <= 0x10000003);
        Require(sceUserServiceGetInitialUser(nullptr) == SCE_USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

void TestPreSetValues() {
    const int sentinels[] = {
        0,
        -1,
        0x10000000,
        0x10000001,
        0x10000002,
        0x10000003,
        0x7fffffff,
        static_cast<int>(0x80960001u),
        static_cast<int>(0x80960005u),
        static_cast<int>(0xdeadbeefu),
    };
    for (int sentinel : sentinels) {
        int userId = sentinel;
        Require(sceUserServiceGetInitialUser(&userId) == USER_SERVICE_OK);
        Require(userId == USER_SERVICE_INITIAL_USER_ID);
        Require(userId >= 0x10000000 && userId <= 0x10000003);
    }
}

} // namespace

int main() {
    TestNullPointer();
    TestValidInitialUser();
    TestIdempotency();
    TestPreSetValues();
    return 0;
}

#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <climits>
#include <cstddef>
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

void TestUserGroupNameBounds() {
    Require(sceUserServiceGetUserGroupName(0, nullptr, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupName(1, nullptr, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupName(-1, nullptr, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupName(129, nullptr, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    char buffer[256];
    for (std::size_t size : {0ull, 1ull, 16ull, 64ull, 128ull}) {
        Require(sceUserServiceGetUserGroupName(0, buffer, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        Require(sceUserServiceGetUserGroupName(1, buffer, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        Require(sceUserServiceGetUserGroupName(-1, buffer, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        Require(sceUserServiceGetUserGroupName(129, buffer, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    }

    for (std::size_t size : {130ull, 150ull, 256ull}) {
        Require(sceUserServiceGetUserGroupName(0, buffer, size) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserGroupName(1, buffer, size) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserGroupName(-1, buffer, size) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserGroupName(129, buffer, size) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    const int invalidIndices[] = {INT_MIN, -100, -2, -1, 129, 130, 1000, INT_MAX};
    for (int index : invalidIndices) {
        Require(sceUserServiceGetUserGroupName(index, buffer, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserGroupName(index, nullptr, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserGroupName(index, buffer, 64) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    }

    Require(sceUserServiceGetUserGroupName(0, buffer, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupName(64, buffer, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupName(128, buffer, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
}

void TestSetUserGroupNameBounds() {
    Require(sceUserServiceSetUserGroupName(0, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceSetUserGroupName(1, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceSetUserGroupName(-1, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    const int invalidIndices[] = {INT_MIN, -100, -2, -1, 1, 2, 64, 128, 129, INT_MAX};
    for (int index : invalidIndices) {
        Require(sceUserServiceSetUserGroupName(index, "Group") == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceSetUserGroupName(index, "") == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceSetUserGroupName(index, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }
}

void TestCurrentUserGroupIndexBounds() {
    Require(sceUserServiceGetCurrentUserGroupIndex(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    std::int32_t groupIndex = -999;
    const int result = sceUserServiceGetCurrentUserGroupIndex(&groupIndex);
    Require(result < 0);
    Require(groupIndex == 0);
}

void TestSetUserGroupIndexBounds() {
    const int invalidIndices[] = {INT_MIN, -100, -5, -2, 129, 130, 500, INT_MAX};
    for (int index : invalidIndices) {
        Require(sceUserServiceSetUserGroupIndex(index) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    for (int index = 0; index <= 128; ++index) {
        Require(sceUserServiceSetUserGroupIndex(index) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    Require(sceUserServiceSetUserGroupIndex(-1) < 0);
}

void TestDefaultNewUserGroupNameBounds() {
    Require(sceUserServiceGetDefaultNewUserGroupName(nullptr, 129) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetDefaultNewUserGroupName(nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetDefaultNewUserGroupName(nullptr, 256) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    char defaultName[256];
    for (std::size_t size : {0ull, 1ull, 32ull, 64ull, 128ull}) {
        Require(sceUserServiceGetDefaultNewUserGroupName(defaultName, size) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    }

    std::memset(defaultName, 'Z', sizeof(defaultName));
    Require(sceUserServiceGetDefaultNewUserGroupName(defaultName, 129) == USER_SERVICE_OK);
    Require(std::strcmp(defaultName, "User Group 1") == 0);
    Require(defaultName[12] == '\0');
    Require(defaultName[129] == 'Z');

    std::memset(defaultName, 'Z', sizeof(defaultName));
    Require(sceUserServiceGetDefaultNewUserGroupName(defaultName, sizeof(defaultName)) == USER_SERVICE_OK);
    Require(std::strcmp(defaultName, "User Group 1") == 0);
}

void TestUserGroupNameListBounds() {
    char nameList[0x110]{};
    std::int32_t count = -1;

    Require(sceUserServiceGetUserGroupNameList(0, nullptr, sizeof(nameList), &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupNameList(0, nameList, sizeof(nameList), nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupNameList(0, nullptr, sizeof(nameList), nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupNameList(-1, nullptr, sizeof(nameList), &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupNameList(-1, nameList, sizeof(nameList), nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (std::size_t size : {0ull, 1ull, 64ull, 128ull, 256ull, 0x10Full}) {
        Require(sceUserServiceGetUserGroupNameList(0, nameList, size, &count) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
        Require(sceUserServiceGetUserGroupNameList(-1, nameList, size, &count) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    }

    for (std::size_t size : {0x111ull, 0x120ull, 0x200ull}) {
        Require(sceUserServiceGetUserGroupNameList(0, nameList, size, &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserGroupNameList(-1, nameList, size, &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    const int invalidIndices[] = {INT_MIN, -100, -5, -2, 129, 130, 1000, INT_MAX};
    for (int index : invalidIndices) {
        Require(sceUserServiceGetUserGroupNameList(index, nameList, 0x110, &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserGroupNameList(index, nullptr, 0x110, &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceGetUserGroupNameList(index, nameList, 0x110, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    for (int index = 0; index <= 128; ++index) {
        Require(sceUserServiceGetUserGroupNameList(index, nameList, 0x110, &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    count = -1;
    Require(sceUserServiceGetUserGroupNameList(-1, nameList, 0x110, &count) < 0);
    Require(count == -1);
}

void TestUserGroupNumBounds() {
    Require(sceUserServiceGetUserGroupNum(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    std::int32_t groupNum = -1;
    Require(sceUserServiceGetUserGroupNum(&groupNum) == USER_SERVICE_OK);
    Require(groupNum == 0);
}

void TestRegisteredHomeUserIdListBounds() {
    std::int32_t userIds[16]{};
    std::int32_t homeCount = -1;

    Require(sceUserServiceGetRegisteredHomeUserIdList(nullptr, &homeCount) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetRegisteredHomeUserIdList(userIds, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetRegisteredHomeUserIdList(nullptr, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (int i = 0; i < 16; ++i) {
        userIds[i] = -999;
    }
    Require(sceUserServiceGetRegisteredHomeUserIdList(userIds, &homeCount) == USER_SERVICE_OK);
    Require(homeCount >= 1);
    Require(userIds[0] == USER_SERVICE_INITIAL_USER_ID);
}

} // namespace

int main() {
    TestUserGroupNameBounds();
    TestSetUserGroupNameBounds();
    TestCurrentUserGroupIndexBounds();
    TestSetUserGroupIndexBounds();
    TestDefaultNewUserGroupNameBounds();
    TestUserGroupNameListBounds();
    TestUserGroupNumBounds();
    TestRegisteredHomeUserIdListBounds();
    return 0;
}

#include "SceTypes.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

int openResult = SCE_KERNEL_ERROR_ENOENT;
int statResult = SCE_KERNEL_ERROR_ENOENT;
int existingSlots = 0;

void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "%s:%u: Require failed\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

void TestCurrentUserGroupIndexBounds() {
    Require(sceUserServiceGetCurrentUserGroupIndex(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    std::int32_t groupIndex = -999;
    const int result = sceUserServiceGetCurrentUserGroupIndex(&groupIndex);
    Require(result == SCE_KERNEL_ERROR_ENOENT);
    Require(groupIndex == 0);
}

void TestSetUserGroupIndexBounds() {
    const int invalidIndices[] = {INT_MIN, -100, -5, -2, 129, 130, 500, INT_MAX};
    for (int index : invalidIndices) {
        Require(sceUserServiceSetUserGroupIndex(index) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    Require(sceUserServiceSetUserGroupIndex(-1) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceUserServiceSetUserGroupIndex(0) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceUserServiceSetUserGroupIndex(64) == SCE_KERNEL_ERROR_ENOENT);
    Require(sceUserServiceSetUserGroupIndex(128) == SCE_KERNEL_ERROR_ENOENT);
}

void TestUserGroupNumSlotLimits() {
    Require(sceUserServiceGetUserGroupNum(nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    existingSlots = 0;
    std::int32_t groupNum = -1;
    Require(sceUserServiceGetUserGroupNum(&groupNum) == USER_SERVICE_OK);
    Require(groupNum == 0);

    existingSlots = 5;
    groupNum = -1;
    Require(sceUserServiceGetUserGroupNum(&groupNum) == USER_SERVICE_OK);
    Require(groupNum == 5);

    existingSlots = 128;
    groupNum = -1;
    Require(sceUserServiceGetUserGroupNum(&groupNum) == USER_SERVICE_OK);
    Require(groupNum == 128);

    existingSlots = 0;
}

void TestUserGroupNameListBounds() {
    char nameList[0x110]{};
    std::int32_t count = -1;

    Require(sceUserServiceGetUserGroupNameList(0, nullptr, sizeof(nameList), &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupNameList(0, nameList, sizeof(nameList), nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupNameList(-1, nullptr, sizeof(nameList), &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupNameList(-1, nameList, sizeof(nameList), nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    for (std::size_t size : {0ull, 1ull, 64ull, 128ull, 256ull, 0x10Full}) {
        Require(sceUserServiceGetUserGroupNameList(-1, nameList, size, &count) == USER_SERVICE_ERROR_BUFFER_TOO_SHORT);
    }

    for (std::size_t size : {0x111ull, 0x120ull, 0x200ull}) {
        Require(sceUserServiceGetUserGroupNameList(-1, nameList, size, &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    const int invalidIndices[] = {INT_MIN, -100, -5, -2, 129, 130, 1000, INT_MAX};
    for (int index : invalidIndices) {
        Require(sceUserServiceGetUserGroupNameList(index, nameList, 0x110, &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    for (int index = 0; index <= 128; ++index) {
        Require(sceUserServiceGetUserGroupNameList(index, nameList, 0x110, &count) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    count = -1;
    Require(sceUserServiceGetUserGroupNameList(-1, nameList, 0x110, &count) == SCE_KERNEL_ERROR_ENOENT);
    Require(count == -1);
}

void TestSetUserGroupNameSlotBounds() {
    Require(sceUserServiceSetUserGroupName(0, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceSetUserGroupName(1, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceSetUserGroupName(-1, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);

    const int invalidIndices[] = {INT_MIN, -100, -2, -1, 129, INT_MAX};
    for (int index : invalidIndices) {
        Require(sceUserServiceSetUserGroupName(index, "Group") == USER_SERVICE_ERROR_INVALID_ARGUMENT);
        Require(sceUserServiceSetUserGroupName(index, "") == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    }

    existingSlots = 0;
    Require(sceUserServiceSetUserGroupName(1, "Group") == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceSetUserGroupName(128, "Group") == USER_SERVICE_ERROR_INVALID_ARGUMENT);
}

} // namespace

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t) {
    return openResult;
}

int APS5_VABI sceKernelClose(int) {
    return 0;
}

int APS5_VABI sceKernelStat(const char* path, FileStat*) {
    if (existingSlots > 0 && path != nullptr) {
        int slot = -1;
        if (std::sscanf(path, "/system_data/priv/home/user_list.dat.%d", &slot) == 1) {
            if (slot >= 0 && slot < existingSlots) {
                return 0;
            }
        }
    }
    return statResult;
}

std::int64_t APS5_VABI sceKernelLseek(int, std::int64_t, int) {
    return -1;
}

std::int64_t APS5_VABI sceKernelRead(int, void*, std::size_t) {
    return -1;
}

std::int64_t APS5_VABI sceKernelWrite(int, const void*, std::size_t) {
    return -1;
}
}

int main() {
    TestCurrentUserGroupIndexBounds();
    TestSetUserGroupIndexBounds();
    TestUserGroupNumSlotLimits();
    TestUserGroupNameListBounds();
    TestSetUserGroupNameSlotBounds();
    return 0;
}

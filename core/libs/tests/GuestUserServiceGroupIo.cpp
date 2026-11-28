#include "prx/libSceUserService/UserService.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {
int openResult = 7;
std::int64_t readResult = 4;
std::int64_t writeResult = 4;
int opens{};
int closes{};
std::string written;
void Require(bool value) { if (!value) std::abort(); }
}

extern "C" {
int APS5_VABI sceKernelOpen(const char*, int, std::uint16_t) { ++opens; return openResult; }
int APS5_VABI sceKernelClose(int) { ++closes; return 0; }
int APS5_VABI sceKernelStat(const char*, FileStat*) { return 0; }
std::int64_t APS5_VABI sceKernelRead(int, void* buffer, std::size_t size) {
    if (readResult > 0) std::memcpy(buffer, "Team", std::min(size, static_cast<std::size_t>(readResult)));
    return readResult;
}
std::int64_t APS5_VABI sceKernelWrite(int, const void* buffer, std::size_t size) {
    written.assign(static_cast<const char*>(buffer), size);
    return writeResult;
}
}

int main() {
    std::int32_t groupCount = -1;
    Require(sceUserServiceGetUserGroupNum(&groupCount) == USER_SERVICE_OK && groupCount == 128);
    errno = 73;
    Require(sceUserServiceGetUserGroupNum(reinterpret_cast<std::int32_t*>(0x10)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(errno == 73);
    unsigned char smallName[32];
    std::memset(smallName, 0xa5, sizeof(smallName));
    const int opensBeforeSizeChecks = opens;
    const int closesBeforeSizeChecks = closes;
    for (const std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{16}, std::size_t{128}, std::size_t{130}, std::size_t{SIZE_MAX}}) {
        errno = EDOM;
        const int expected = size < 129 ? USER_SERVICE_ERROR_BUFFER_TOO_SHORT : USER_SERVICE_ERROR_INVALID_ARGUMENT;
        Require(sceUserServiceGetUserGroupName(0, reinterpret_cast<char*>(smallName), size) == expected);
        Require(errno == EDOM && opens == opensBeforeSizeChecks && closes == closesBeforeSizeChecks);
        for (const unsigned char byte : smallName) Require(byte == 0xa5);
    }
    Require(sceUserServiceGetUserGroupName(0, nullptr, 0) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    char name[129]{};
    Require(sceUserServiceGetUserGroupName(INT_MAX, name, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupName(INT_MIN, name, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(opens == 0);
    Require(sceUserServiceGetUserGroupName(0, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "Team") == 0 && closes == 1);
    errno = EDOM;
    Require(sceUserServiceGetUserGroupName(0, reinterpret_cast<char*>(0x10), sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(errno == EDOM && opens == closes);
    static const char readOnlyName[129]{};
    Require(sceUserServiceGetUserGroupName(0, const_cast<char*>(readOnlyName), sizeof(readOnlyName)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(errno == EDOM && opens == closes);
    readResult = 0;
    Require(sceUserServiceGetUserGroupName(128, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "User Group 129") == 0);
    readResult = SCE_KERNEL_ERROR_EIO;
    Require(sceUserServiceGetUserGroupName(0, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "User Group 1") == 0);
    openResult = SCE_KERNEL_ERROR_EIO;
    Require(sceUserServiceGetUserGroupName(0, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "User Group 1") == 0);
    errno = EDOM;
    Require(sceUserServiceGetUserGroupName(0, reinterpret_cast<char*>(0x10), sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(errno == EDOM);
    openResult = 7;
    const int opensBeforeInvalidName = opens;
    const int closesBeforeInvalidName = closes;
    const std::string writtenBeforeInvalidName = written;
    Require(sceUserServiceSetUserGroupName(0, reinterpret_cast<const char*>(static_cast<std::uintptr_t>(0x10))) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(opens == opensBeforeInvalidName && closes == closesBeforeInvalidName);
    Require(written == writtenBeforeInvalidName);
    Require(sceUserServiceSetUserGroupName(0, nullptr) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    const std::string longestName(128, 'N');
    writeResult = 128;
    Require(sceUserServiceSetUserGroupName(0, longestName.c_str()) == USER_SERVICE_OK);
    Require(written == longestName);
    Require(sceUserServiceSetUserGroupName(0, (longestName + 'N').c_str()) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    writeResult = 4;
    Require(sceUserServiceSetUserGroupName(0, "Team") == USER_SERVICE_OK);
    Require(written == "Team");
    writeResult = 2;
    Require(sceUserServiceSetUserGroupName(0, "Team") == SCE_KERNEL_ERROR_EIO);
    writeResult = SCE_KERNEL_ERROR_EIO;
    Require(sceUserServiceSetUserGroupName(0, "Team") == SCE_KERNEL_ERROR_EIO);
    writeResult = 0;
    Require(sceUserServiceSetUserGroupName(0, "") == USER_SERVICE_OK);
}

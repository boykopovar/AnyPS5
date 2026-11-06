#include "prx/libSceUserService/UserService.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <climits>
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
    char name[129]{};
    Require(sceUserServiceGetUserGroupName(INT_MAX, name, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(sceUserServiceGetUserGroupName(INT_MIN, name, sizeof(name)) == USER_SERVICE_ERROR_INVALID_ARGUMENT);
    Require(opens == 0);
    Require(sceUserServiceGetUserGroupName(0, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "Team") == 0 && closes == 1);
    readResult = 0;
    Require(sceUserServiceGetUserGroupName(128, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "User Group 129") == 0);
    readResult = SCE_KERNEL_ERROR_EIO;
    Require(sceUserServiceGetUserGroupName(0, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "User Group 1") == 0);
    openResult = SCE_KERNEL_ERROR_EIO;
    Require(sceUserServiceGetUserGroupName(0, name, sizeof(name)) == USER_SERVICE_OK);
    Require(std::strcmp(name, "User Group 1") == 0);
    openResult = 7;
    Require(sceUserServiceSetUserGroupName(0, "Team") == USER_SERVICE_OK);
    Require(written == "Team");
    writeResult = 2;
    Require(sceUserServiceSetUserGroupName(0, "Team") == SCE_KERNEL_ERROR_EIO);
    writeResult = SCE_KERNEL_ERROR_EIO;
    Require(sceUserServiceSetUserGroupName(0, "Team") == SCE_KERNEL_ERROR_EIO);
    writeResult = 0;
    Require(sceUserServiceSetUserGroupName(0, "") == USER_SERVICE_OK);
}

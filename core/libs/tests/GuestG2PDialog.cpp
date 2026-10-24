#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include <cstdlib>

extern "C" {
int APS5_VABI sceUserServiceInitialize(const UserServiceInitParam* param);
int APS5_VABI sceUserServiceTerminate();
int APS5_VABI sceG2PDialogInitialize(void);
int APS5_VABI sceG2PDialogOpen(const void* param);
int APS5_VABI sceG2PDialogUpdateStatus(void);
int APS5_VABI sceG2PDialogGetStatus(void);
int APS5_VABI sceG2PDialogGetResult(void* result);
int APS5_VABI sceG2PDialogClose(void);
int APS5_VABI sceG2PDialogTerminate(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005);
constexpr int kErrArgNull = static_cast<int>(0x80B8000D);
constexpr int kResultUserCanceled = 1;

}

int main() {
    Require(sceUserServiceInitialize(nullptr) == 0);
    G2PDialogParam param{};
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = sizeof(param);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    std::int32_t result[4] = {-1, -1, -1, -1};

    Require(sceG2PDialogTerminate() == kErrNotInitialized);
    Require(sceG2PDialogOpen(&param) == kErrNotInitialized);
    Require(sceG2PDialogClose() == kErrNotInitialized);
    Require(sceG2PDialogGetResult(result) == kErrNotInitialized);
    Require(sceG2PDialogOpen(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceG2PDialogGetResult(nullptr) == static_cast<int>(0x80B8000Du));
    Require(sceG2PDialogGetStatus() == kStatusNone);
    Require(sceG2PDialogUpdateStatus() == kStatusNone);

    Require(sceG2PDialogInitialize() == 0);
    Require(sceG2PDialogInitialize() == kErrAlreadyInitialized);
    Require(sceG2PDialogGetStatus() == kStatusInitialized);
    Require(sceG2PDialogUpdateStatus() == kStatusInitialized);

    Require(sceG2PDialogOpen(nullptr) == kErrArgNull);
    Require(sceG2PDialogGetResult(result) == kErrNotFinished);

    param.base.size = 0;
    Require(sceG2PDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.base.size = sizeof(param.base);
    param.base.magic = 0;
    Require(sceG2PDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = 0;
    Require(sceG2PDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.size = sizeof(param);
    param.reserved[63] = 1;
    Require(sceG2PDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.reserved[63] = 0;
    param.userId = -1;
    Require(sceG2PDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.userId = 42;
    Require(sceG2PDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    Require(sceUserServiceTerminate() == 0);
    Require(sceG2PDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    Require(sceUserServiceInitialize(nullptr) == 0);
    Require(sceG2PDialogOpen(&param) == 0);
    Require(sceG2PDialogGetStatus() == kStatusFinished);
    Require(sceG2PDialogUpdateStatus() == kStatusFinished);

    Require(sceG2PDialogGetResult(nullptr) == kErrArgNull);
    Require(sceG2PDialogGetResult(result) == 0);
    Require(result[0] == -1 && result[1] == -1 && result[3] == -1);
    Require(result[2] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceG2PDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceG2PDialogTerminate() == 0);
    Require(sceG2PDialogTerminate() == kErrNotInitialized);
    Require(sceG2PDialogGetStatus() == kStatusNone);

    Require(sceG2PDialogInitialize() == 0);
    Require(sceG2PDialogGetStatus() == kStatusInitialized);
    Require(sceG2PDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceG2PDialogGetStatus() == kStatusInitialized);
    result[2] = -1;
    Require(sceG2PDialogGetResult(result) == kErrNotFinished);
    Require(result[0] == -1 && result[1] == -1 && result[3] == -1);
    Require(result[2] == -1);
    Require(sceG2PDialogTerminate() == 0);
    Require(sceUserServiceTerminate() == 0);
}

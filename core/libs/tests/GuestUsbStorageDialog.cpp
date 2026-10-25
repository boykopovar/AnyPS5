#include "prx/libSceUsbStorageDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <array>
#include <string>
#include "ProcessParametersFixture.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libSceUserService/UserService.hpp"

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusRunning = 2;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005);
constexpr int kErrBusy = static_cast<int>(0x80B80007);
constexpr int kErrNotRunning = static_cast<int>(0x80B8000B);
constexpr int kErrArgNull = static_cast<int>(0x80B8000D);
constexpr int kResultUserCanceled = 1;

}

int main() {
    static std::array<std::byte, 64> process{};
    Require(ProcessParametersFixture::Install(process.data(), process.size()));
    std::uint32_t sdkVersion = 0x04500000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(sceUserServiceInitialize(nullptr) == 0);
    UsbStorageDialogParam param{};
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = sizeof(param);
    param.userId = USER_SERVICE_INITIAL_USER_ID;
    std::strcpy(param.path, "test");
    UsbStorageDialogResult result;
    std::memset(&result, 0x5a, sizeof(result));
    result.size = sizeof(result);
    result.result = kErrBusy;

    Require(sceUsbStorageDialogTerminate() == kErrNotInitialized);
    Require(sceUsbStorageDialogOpen(&param) == kErrNotInitialized);
    Require(sceUsbStorageDialogClose() == kErrNotInitialized);
    Require(sceUsbStorageDialogGetResult(&result) == kErrNotInitialized);
    Require(sceUsbStorageDialogGetStatus() == kStatusNone);
    Require(sceUsbStorageDialogUpdateStatus() == kStatusNone);

    Require(sceUsbStorageDialogInitialize() == 0);
    Require(sceUsbStorageDialogInitialize() == kErrAlreadyInitialized);
    Require(sceUsbStorageDialogGetStatus() == kStatusInitialized);

    Require(sceUsbStorageDialogOpen(nullptr) == kErrArgNull);
    Require(sceUsbStorageDialogGetResult(&result) == kErrNotFinished);

    param.path[1023] = 'a';
    Require(sceUsbStorageDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.path[1023] = 0;
    std::strcpy(param.path, "a..b");
    Require(sceUsbStorageDialogOpen(&param) == static_cast<int>(0x812D0003u));
    for (const char invalid : std::string(",*?\"<>|")) {
        param.path[0] = invalid;
        param.path[1] = 0;
        Require(sceUsbStorageDialogOpen(&param) == static_cast<int>(0x812D0003u));
    }
    std::strcpy(param.path, "test");
    Require(sceUsbStorageDialogOpen(&param) == 0);
    Require(sceUsbStorageDialogOpen(&param) == kErrBusy);
    Require(sceUsbStorageDialogGetStatus() == kStatusRunning);
    Require(sceUsbStorageDialogGetResult(&result) == kErrNotFinished);

    Require(sceUsbStorageDialogUpdateStatus() == kStatusFinished);
    Require(sceUsbStorageDialogGetStatus() == kStatusFinished);

    Require(sceUsbStorageDialogGetResult(nullptr) == kErrArgNull);
    result.size = 79;
    Require(sceUsbStorageDialogGetResult(&result) == static_cast<int>(0x80B8000Au));
    result.size = sizeof(result);
    result.result = 0;
    Require(sceUsbStorageDialogGetResult(&result) == static_cast<int>(0x80B8000Au));
    result.result = kErrBusy;
    Require(sceUsbStorageDialogGetResult(&result) == kResultUserCanceled);
    Require(result.result == kResultUserCanceled);
    Require(result.size == sizeof(result) && result.status == 0x5a5a5a5a);
    for (const auto value : result.reserved) Require(value == 0x5a);

    Require(sceUsbStorageDialogClose() == kErrNotRunning);
    Require(sceUsbStorageDialogTerminate() == 0);
    Require(sceUsbStorageDialogTerminate() == kErrNotInitialized);
    Require(sceUsbStorageDialogGetStatus() == kStatusNone);

    Require(sceUsbStorageDialogInitialize() == 0);
    Require(sceUsbStorageDialogOpen(&param) == 0);
    Require(sceUsbStorageDialogGetStatus() == kStatusRunning);
    Require(sceUsbStorageDialogClose() == 0);
    Require(sceUsbStorageDialogGetStatus() == kStatusFinished);
    result.result = 0;
    Require(sceUsbStorageDialogGetResult(&result) == 0);
    Require(result.result == kErrNotFinished && result.status == 0);
    Require(sceUsbStorageDialogTerminate() == 0);
    sdkVersion = 0x04400000;
    std::memcpy(process.data() + 16, &sdkVersion, sizeof(sdkVersion));
    Require(sceUsbStorageDialogInitialize() == 0);
    std::strcpy(param.path, "a..b");
    Require(sceUsbStorageDialogOpen(&param) == 0);
    Require(sceUsbStorageDialogUpdateStatus() == kStatusFinished);
    result.size = 0;
    result.result = 0;
    Require(sceUsbStorageDialogGetResult(&result) == kResultUserCanceled);
    Require(result.size == 0 && result.result == kResultUserCanceled);
    Require(sceUsbStorageDialogTerminate() == 0);
    Require(sceUsbStorageDialogOpen(nullptr) == kErrArgNull);
    Require(sceUsbStorageDialogGetResult(nullptr) == kErrArgNull);
    Require(sceUserServiceTerminate() == 0);
    return 0;
}

#include "prx/libSceNetCtlApDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include "prx/libSceCommonDialog/CommonDialog.hpp"

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

void RequireCanceled(const std::uint8_t (&result)[44]) {
    std::int32_t code = 0;
    std::memcpy(&code, result, sizeof(code));
    Require(code == kResultUserCanceled);
    for (int i = 4; i < 40; ++i) Require(result[i] == 0);
    for (int i = 40; i < 44; ++i) Require(result[i] == 0x5a);
}

}

int main() {
    NetCtlApDialogParam param{};
    param.base.size = sizeof(param.base);
    param.base.magic = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&param)) - 1060003575u;
    param.size = sizeof(param);
    alignas(NetCtlApDialogResult) std::uint8_t result[44];
    std::memset(result, 0x5a, sizeof(result));

    Require(sceNetCtlApDialogTerminate() == kErrNotInitialized);
    Require(sceNetCtlApDialogOpen(&param) == kErrNotInitialized);
    Require(sceNetCtlApDialogClose() == kErrNotInitialized);
    Require(sceNetCtlApDialogGetResult(reinterpret_cast<NetCtlApDialogResult*>(result)) == kErrNotInitialized);
    Require(sceNetCtlApDialogGetStatus() == kStatusNone);
    Require(sceNetCtlApDialogUpdateStatus() == kStatusNone);

    Require(sceNetCtlApDialogInitialize() == 0);
    Require(sceNetCtlApDialogInitialize() == kErrAlreadyInitialized);
    Require(sceNetCtlApDialogGetStatus() == kStatusInitialized);
    Require(sceNetCtlApDialogUpdateStatus() == kStatusInitialized);

    Require(sceNetCtlApDialogOpen(nullptr) == kErrArgNull);
    Require(sceNetCtlApDialogGetStatus() == kStatusInitialized);
    Require(sceNetCtlApDialogGetResult(reinterpret_cast<NetCtlApDialogResult*>(result)) == static_cast<int>(0x80B8000Au));
    std::memset(result + 4, 0, 36);
    Require(sceNetCtlApDialogGetResult(reinterpret_cast<NetCtlApDialogResult*>(result)) == kErrNotFinished);

    param.size = 103;
    Require(sceNetCtlApDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.size = sizeof(param);
    param.reserved60 = 1;
    Require(sceNetCtlApDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.reserved60 = 0;
    std::memset(param.option, 'a', sizeof(param.option));
    Require(sceNetCtlApDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.option[8] = 0;
    param.reserved[30] = 1;
    Require(sceNetCtlApDialogOpen(&param) == static_cast<int>(0x80B8000Au));
    param.reserved[30] = 0;
    Require(sceNetCtlApDialogOpen(&param) == 0);
    Require(sceNetCtlApDialogGetStatus() == kStatusFinished);
    Require(sceNetCtlApDialogUpdateStatus() == kStatusFinished);

    Require(sceNetCtlApDialogGetResult(nullptr) == kErrArgNull);
    Require(sceNetCtlApDialogGetResult(reinterpret_cast<NetCtlApDialogResult*>(result)) == kResultUserCanceled);
    RequireCanceled(result);

    Require(sceNetCtlApDialogOpen(&param) == 0);
    Require(sceNetCtlApDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceNetCtlApDialogTerminate() == 0);
    Require(sceNetCtlApDialogTerminate() == kErrNotInitialized);
    Require(sceNetCtlApDialogGetStatus() == kStatusNone);

    Require(sceNetCtlApDialogInitialize() == 0);
    Require(sceNetCtlApDialogClose() == static_cast<int>(0x80B8000Bu));
    Require(sceNetCtlApDialogGetStatus() == kStatusInitialized);
    Require(sceNetCtlApDialogGetResult(reinterpret_cast<NetCtlApDialogResult*>(result)) == kErrNotFinished);
    Require(sceNetCtlApDialogTerminate() == 0);
}

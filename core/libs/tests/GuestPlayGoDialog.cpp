#include "prx/libScePlayGoDialog/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr std::int32_t notInitialized = static_cast<std::int32_t>(0x80B80003);
    constexpr std::int32_t alreadyInitialized = static_cast<std::int32_t>(0x80B80004);
    constexpr std::int32_t paramInvalid = static_cast<std::int32_t>(0x80B8000D);
    constexpr std::int32_t notFinished = static_cast<std::int32_t>(0x80B80005u);

    Require(scePlayGoDialogGetStatus() == 0);
    Require(scePlayGoDialogTerminate() == notInitialized);
    Require(scePlayGoDialogClose() == notInitialized);
    Require(scePlayGoDialogOpen(nullptr) == paramInvalid);
    Require(scePlayGoDialogGetResult(nullptr) == paramInvalid);

    const int openParam = 1;
    Require(scePlayGoDialogOpen(&openParam) == notInitialized);

    PlayGoDialogResult early{};
    Require(scePlayGoDialogGetResult(&early) == notInitialized);
    early.reserved[31] = 1;
    Require(scePlayGoDialogGetResult(&early) == static_cast<std::int32_t>(0x80B8000Au));
    Require(scePlayGoDialogInitialize() == 0);
    Require(scePlayGoDialogGetStatus() == 1);
    Require(scePlayGoDialogInitialize() == alreadyInitialized);
    Require(scePlayGoDialogOpen(nullptr) == paramInvalid);
    Require(scePlayGoDialogUpdateStatus() == 1);

    PlayGoDialogResult unfinishedResult{};
    Require(scePlayGoDialogGetResult(&unfinishedResult) == notFinished);

    Require(scePlayGoDialogOpen(&openParam) == 0);
    Require(scePlayGoDialogGetStatus() == 3);
    Require(scePlayGoDialogUpdateStatus() == 3);
    Require(scePlayGoDialogOpen(nullptr) == paramInvalid);
    Require(scePlayGoDialogOpen(&openParam) == 0);

    PlayGoDialogResult result{};
    Require(scePlayGoDialogGetResult(&result) == 0);
    Require(result.result == 0 && result.errorCode == 0);
    result.errorCode = 7;
    result.result = 9;
    result.reserved[0] = 1;
    Require(scePlayGoDialogGetResult(&result) == static_cast<std::int32_t>(0x80B8000Au));
    Require(result.errorCode == 7 && result.result == 9 && result.reserved[0] == 1);
    Require(scePlayGoDialogClose() == static_cast<std::int32_t>(0x80B8000Bu));
    Require(scePlayGoDialogTerminate() == 0);
    Require(scePlayGoDialogGetStatus() == 0);
    Require(scePlayGoDialogTerminate() == notInitialized);
    Require(scePlayGoDialogClose() == notInitialized);
    Require(scePlayGoDialogOpen(nullptr) == paramInvalid);
    Require(scePlayGoDialogOpen(&openParam) == notInitialized);
    Require(scePlayGoDialogGetResult(nullptr) == paramInvalid);
    Require(scePlayGoDialogGetResult(&result) == static_cast<std::int32_t>(0x80B8000Au));
    result.reserved[0] = 0;
    Require(scePlayGoDialogGetResult(&result) == notInitialized);
    Require(scePlayGoDialogUpdateStatus() == 0);
    return 0;
}

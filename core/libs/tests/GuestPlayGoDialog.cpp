#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI scePlayGoDialogInitialize(void);
int APS5_VABI scePlayGoDialogTerminate(void);
int APS5_VABI scePlayGoDialogOpen(const void* param);
int APS5_VABI scePlayGoDialogClose(void);
int APS5_VABI scePlayGoDialogUpdateStatus(void);
int APS5_VABI scePlayGoDialogGetStatus(void);
int APS5_VABI scePlayGoDialogGetResult(void* result);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr std::int32_t notInitialized = static_cast<std::int32_t>(0x80ED0001);
    constexpr std::int32_t alreadyInitialized = static_cast<std::int32_t>(0x80ED0002);
    constexpr std::int32_t paramInvalid = static_cast<std::int32_t>(0x80ED0003);
    constexpr std::int32_t invalidState = static_cast<std::int32_t>(0x80ED0005);

    Require(scePlayGoDialogGetStatus() == 0);
    Require(scePlayGoDialogTerminate() == notInitialized);
    Require(scePlayGoDialogOpen(nullptr) == paramInvalid);
    Require(scePlayGoDialogGetResult(nullptr) == notInitialized);

    const int openParam = 1;
    Require(scePlayGoDialogOpen(&openParam) == invalidState);

    Require(scePlayGoDialogInitialize() == 0);
    Require(scePlayGoDialogGetStatus() == 1);
    Require(scePlayGoDialogInitialize() == alreadyInitialized);
    Require(scePlayGoDialogOpen(nullptr) == paramInvalid);
    Require(scePlayGoDialogUpdateStatus() == 1);

    Require(scePlayGoDialogOpen(&openParam) == 0);
    Require(scePlayGoDialogGetStatus() == 3);
    Require(scePlayGoDialogUpdateStatus() == 3);

    PlayGoDialogResult result{};
    Require(scePlayGoDialogGetResult(&result) == 0);
    Require(result.result == 0);
    Require(scePlayGoDialogClose() == 0);
    Require(scePlayGoDialogTerminate() == 0);
    Require(scePlayGoDialogGetStatus() == 0);
    Require(scePlayGoDialogTerminate() == notInitialized);
    return 0;
}

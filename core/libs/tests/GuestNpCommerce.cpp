#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

extern "C" {
int APS5_VABI sceNpCommerceDialogInitialize();
int APS5_VABI sceNpCommerceDialogOpen(const void* param);
int APS5_VABI sceNpCommerceDialogOpen2(const void* param);
int APS5_VABI sceNpCommerceDialogUpdateStatus(void);
int APS5_VABI sceNpCommerceDialogGetStatus(void);
int APS5_VABI sceNpCommerceDialogGetResult(void* result);
int APS5_VABI sceNpCommerceDialogClose(void);
int APS5_VABI sceNpCommerceDialogTerminate();
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    constexpr int notInitialized = static_cast<int>(0x80B80003u);
    constexpr int notFinished = static_cast<int>(0x80B80006u);
    constexpr int argNull = static_cast<int>(0x80B80009u);
    constexpr int statusInitialized = 1;
    constexpr int statusFinished = 3;
    constexpr int userCanceled = 1;
    const std::uint8_t param[0x80] = {};

    Require(sceNpCommerceDialogOpen2(param) == notInitialized);
    Require(sceNpCommerceDialogInitialize() == 0);
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);
    std::uint8_t result[0x20];
    Require(sceNpCommerceDialogGetResult(result) == notFinished);
    Require(sceNpCommerceDialogOpen2(nullptr) == argNull);
    Require(sceNpCommerceDialogGetStatus() == statusInitialized);

    Require(sceNpCommerceDialogOpen2(param) == 0);
    Require(sceNpCommerceDialogUpdateStatus() == statusFinished);
    Require(sceNpCommerceDialogGetStatus() == statusFinished);
    std::memset(result, 0xcd, sizeof(result));
    Require(sceNpCommerceDialogGetResult(result) == 0);
    std::int32_t value = 0;
    std::memcpy(&value, result, sizeof(value));
    Require(value == userCanceled && result[4] == 0);
    Require(sceNpCommerceDialogClose() == 0);
    Require(sceNpCommerceDialogTerminate() == 0);

    Require(sceNpCommerceDialogInitialize() == 0);
    Require(sceNpCommerceDialogOpen(param) == 0);
    Require(sceNpCommerceDialogGetStatus() == statusFinished);
    Require(sceNpCommerceDialogTerminate() == 0);
    Require(sceNpCommerceDialogOpen2(param) == notInitialized);
    return 0;
}

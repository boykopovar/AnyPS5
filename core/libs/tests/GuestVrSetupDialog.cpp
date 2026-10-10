#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceVrSetupDialogInitialize(void);
int APS5_VABI sceVrSetupDialogOpen(const void* param);
int APS5_VABI sceVrSetupDialogUpdateStatus(void);
int APS5_VABI sceVrSetupDialogGetResult(void* result);
int APS5_VABI sceVrSetupDialogClose(void);
int APS5_VABI sceVrSetupDialogTerminate(void);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005);
constexpr int kErrArgNull = static_cast<int>(0x80B8000D);
constexpr int kResultUserCanceled = 1;

}

int main() {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(sceVrSetupDialogTerminate() == kErrNotInitialized);
    Require(sceVrSetupDialogOpen(&param) == kErrNotInitialized);
    Require(sceVrSetupDialogGetResult(result) == kErrNotInitialized);

    Require(sceVrSetupDialogInitialize() == 0);
    Require(sceVrSetupDialogInitialize() == kErrAlreadyInitialized);

    Require(sceVrSetupDialogOpen(nullptr) == kErrArgNull);
    Require(sceVrSetupDialogGetResult(result) == kErrNotFinished);

    Require(sceVrSetupDialogOpen(&param) == 0);
    Require(sceVrSetupDialogUpdateStatus() == kStatusFinished);

    Require(sceVrSetupDialogGetResult(nullptr) == kErrArgNull);
    Require(sceVrSetupDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceVrSetupDialogClose() == 0);
    Require(sceVrSetupDialogTerminate() == 0);
    Require(sceVrSetupDialogTerminate() == kErrNotInitialized);
}

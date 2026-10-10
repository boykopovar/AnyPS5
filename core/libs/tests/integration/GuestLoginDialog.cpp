#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceLoginDialogInitialize(void);
int APS5_VABI sceLoginDialogOpen(const void* param);
int APS5_VABI sceLoginDialogUpdateStatus(void);
int APS5_VABI sceLoginDialogGetStatus(void);
int APS5_VABI sceLoginDialogGetResult(void* result);
int APS5_VABI sceLoginDialogClose(void);
int APS5_VABI sceLoginDialogTerminate(void);
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

    Require(sceLoginDialogTerminate() == kErrNotInitialized);
    Require(sceLoginDialogOpen(&param) == kErrNotInitialized);
    Require(sceLoginDialogGetResult(result) == kErrNotInitialized);

    Require(sceLoginDialogInitialize() == 0);
    Require(sceLoginDialogInitialize() == kErrAlreadyInitialized);

    Require(sceLoginDialogOpen(nullptr) == kErrArgNull);
    Require(sceLoginDialogGetResult(result) == kErrNotFinished);

    Require(sceLoginDialogOpen(&param) == 0);
    Require(sceLoginDialogGetStatus() == kStatusFinished);
    Require(sceLoginDialogUpdateStatus() == kStatusFinished);

    Require(sceLoginDialogGetResult(nullptr) == kErrArgNull);
    Require(sceLoginDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceLoginDialogClose() == 0);
    Require(sceLoginDialogTerminate() == 0);
    Require(sceLoginDialogTerminate() == kErrNotInitialized);
}

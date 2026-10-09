#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceSigninDialogInitialize(void);
int APS5_VABI sceSigninDialogOpen(const void* param);
int APS5_VABI sceSigninDialogUpdateStatus(void);
int APS5_VABI sceSigninDialogGetStatus(void);
int APS5_VABI sceSigninDialogGetResult(void* result);
int APS5_VABI sceSigninDialogClose(void);
int APS5_VABI sceSigninDialogTerminate(void);
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

    Require(sceSigninDialogTerminate() == kErrNotInitialized);
    Require(sceSigninDialogOpen(&param) == kErrNotInitialized);
    Require(sceSigninDialogGetResult(result) == kErrNotInitialized);

    Require(sceSigninDialogInitialize() == 0);
    Require(sceSigninDialogInitialize() == kErrAlreadyInitialized);

    Require(sceSigninDialogOpen(nullptr) == kErrArgNull);
    Require(sceSigninDialogGetResult(result) == kErrNotFinished);

    Require(sceSigninDialogOpen(&param) == 0);
    Require(sceSigninDialogGetStatus() == kStatusFinished);
    Require(sceSigninDialogUpdateStatus() == kStatusFinished);

    Require(sceSigninDialogGetResult(nullptr) == kErrArgNull);
    Require(sceSigninDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceSigninDialogClose() == 0);
    Require(sceSigninDialogTerminate() == 0);
    Require(sceSigninDialogTerminate() == kErrNotInitialized);
}

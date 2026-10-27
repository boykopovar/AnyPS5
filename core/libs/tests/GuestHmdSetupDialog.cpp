#include "prx/libSceHmdSetupDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

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
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};

    Require(sceHmdSetupDialogTerminate() == kErrNotInitialized);
    Require(sceHmdSetupDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceHmdSetupDialogOpen(&param) == kErrNotInitialized);
    Require(sceHmdSetupDialogClose() == kErrNotInitialized);
    Require(sceHmdSetupDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceHmdSetupDialogGetResult(result) == kErrNotInitialized);
    Require(sceHmdSetupDialogGetStatus() == kStatusNone);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusNone);

    Require(sceHmdSetupDialogInitialize() == 0);
    Require(sceHmdSetupDialogInitialize() == kErrAlreadyInitialized);
    Require(sceHmdSetupDialogGetStatus() == kStatusInitialized);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusInitialized);

    Require(sceHmdSetupDialogOpen(nullptr) == kErrArgNull);
    Require(sceHmdSetupDialogGetResult(result) == kErrNotFinished);

    Require(sceHmdSetupDialogOpen(&param) == 0);
    Require(sceHmdSetupDialogGetStatus() == kStatusFinished);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusFinished);

    Require(sceHmdSetupDialogGetResult(nullptr) == kErrArgNull);
    Require(sceHmdSetupDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == -1);

    Require(sceHmdSetupDialogClose() == 0);
    Require(sceHmdSetupDialogTerminate() == 0);
    Require(sceHmdSetupDialogTerminate() == kErrNotInitialized);
    Require(sceHmdSetupDialogGetStatus() == kStatusNone);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusNone);

    Require(sceHmdSetupDialogInitialize() == 0);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusInitialized);
    Require(sceHmdSetupDialogClose() == 0);
    Require(sceHmdSetupDialogUpdateStatus() == kStatusFinished);
    result[0] = -1;
    Require(sceHmdSetupDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(sceHmdSetupDialogTerminate() == 0);
}

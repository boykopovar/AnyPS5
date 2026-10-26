#include "prx/libSceSigninDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

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
    std::int32_t result[4];
    std::memset(result, 0x5a, sizeof(result));

    Require(sceSigninDialogTerminate() == kErrNotInitialized);
    Require(sceSigninDialogOpen(nullptr) == kErrNotInitialized);
    Require(sceSigninDialogOpen(&param) == kErrNotInitialized);
    Require(sceSigninDialogClose() == kErrNotInitialized);
    Require(sceSigninDialogGetResult(nullptr) == kErrNotInitialized);
    Require(sceSigninDialogGetResult(result) == kErrNotInitialized);
    Require(sceSigninDialogGetStatus() == kStatusNone);
    Require(sceSigninDialogUpdateStatus() == kStatusNone);

    Require(sceSigninDialogInitialize() == 0);
    Require(sceSigninDialogInitialize() == kErrAlreadyInitialized);
    Require(sceSigninDialogGetStatus() == kStatusInitialized);

    Require(sceSigninDialogOpen(nullptr) == kErrArgNull);
    Require(sceSigninDialogGetResult(nullptr) == kErrNotFinished);
    Require(sceSigninDialogGetResult(result) == kErrNotFinished);

    Require(sceSigninDialogOpen(&param) == 0);
    Require(sceSigninDialogGetStatus() == kStatusFinished);
    Require(sceSigninDialogUpdateStatus() == kStatusFinished);

    Require(sceSigninDialogGetResult(nullptr) == kErrArgNull);
    Require(sceSigninDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == 0);
    Require(result[2] == 0);
    Require(result[3] == 0);

    Require(sceSigninDialogClose() == 0);
    Require(sceSigninDialogTerminate() == 0);
    Require(sceSigninDialogTerminate() == kErrNotInitialized);
    Require(sceSigninDialogGetStatus() == kStatusNone);

    Require(sceSigninDialogInitialize() == 0);
    Require(sceSigninDialogGetStatus() == kStatusInitialized);
    Require(sceSigninDialogClose() == 0);
    Require(sceSigninDialogGetStatus() == kStatusFinished);
    std::memset(result, 0xa5, sizeof(result));
    Require(sceSigninDialogGetResult(result) == 0);
    Require(result[0] == kResultUserCanceled);
    Require(result[1] == 0);
    Require(result[2] == 0);
    Require(result[3] == 0);
    Require(sceSigninDialogTerminate() == 0);
}

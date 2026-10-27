#include "prx/libSceLoginDialog/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include "prx/libSceCommonDialog/CommonDialog.hpp"

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x81340001);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x81340002);
constexpr int kErrNotFinished = static_cast<int>(0x81340005);
constexpr int kErrArgNull = static_cast<int>(0x81340003);
constexpr int kResultUserCanceled = 1;

}

int main() {
    LoginDialogParam param{};
    param.size = sizeof(param);
    std::fill(std::begin(param.userIds), std::end(param.userIds), -1);
    LoginDialogResult result{-1, -1, {11, 22, 33, 44, 55, 66, 77, 88}};

    Require(sceLoginDialogTerminate() == kErrNotInitialized);
    Require(sceLoginDialogOpen(nullptr) == kErrArgNull);
    Require(sceLoginDialogOpen(&param) == kErrNotInitialized);
    Require(sceLoginDialogClose() == kErrNotInitialized);
    Require(sceLoginDialogGetResult(nullptr) == kErrArgNull);
    Require(sceLoginDialogGetResult(&result) == kErrNotInitialized);
    Require(sceLoginDialogGetStatus() == kStatusNone);
    Require(sceLoginDialogUpdateStatus() == kStatusNone);

    Require(sceLoginDialogInitialize() == 0);
    Require(sceLoginDialogInitialize() == kErrAlreadyInitialized);
    Require(sceLoginDialogGetStatus() == kStatusInitialized);
    Require(sceLoginDialogUpdateStatus() == kStatusInitialized);

    Require(sceLoginDialogOpen(nullptr) == kErrArgNull);
    Require(sceLoginDialogGetResult(nullptr) == kErrArgNull);
    Require(sceLoginDialogGetResult(&result) == kErrNotFinished);
    Require(result.result == 1 && result.errorCode == static_cast<int>(0x81340007u));

    param.size = 63;
    Require(sceLoginDialogOpen(&param) == kErrArgNull);
    param.size = sizeof(param);
    param.mode = 2;
    Require(sceLoginDialogOpen(&param) == kErrArgNull);
    param.mode = 1;
    param.reserved[4] = 1;
    Require(sceLoginDialogOpen(&param) == kErrArgNull);
    param.reserved[4] = 0;
    for (auto& userId : param.userIds) {
        for (const auto invalid : {0, 0x0fffffff, 0x40000000, -2}) {
            userId = invalid;
            Require(sceLoginDialogOpen(&param) == static_cast<int>(0x81340007u));
        }
        userId = -1;
    }
    param.userIds[0] = 0x10000000;
    param.userIds[8] = 0x3fffffff;
    Require(sceLoginDialogOpen(&param) == 0);
    Require(sceLoginDialogGetStatus() == kStatusFinished);
    Require(sceLoginDialogUpdateStatus() == kStatusFinished);

    Require(sceLoginDialogGetResult(nullptr) == kErrArgNull);
    Require(sceLoginDialogGetResult(&result) == 0);
    Require(result.result == kResultUserCanceled);
    Require(result.errorCode == static_cast<int>(0x81340007u));
    Require(result.data[0] == 11 && result.data[7] == 88);

    Require(sceLoginDialogClose() == 0);
    Require(sceLoginDialogTerminate() == 0);
    Require(sceLoginDialogTerminate() == kErrNotInitialized);
    Require(sceLoginDialogOpen(nullptr) == kErrArgNull);
    Require(sceLoginDialogOpen(&param) == kErrNotInitialized);
    Require(sceLoginDialogGetResult(nullptr) == kErrArgNull);
    Require(sceLoginDialogGetResult(&result) == kErrNotInitialized);
    Require(sceLoginDialogGetStatus() == kStatusNone);
    Require(sceLoginDialogUpdateStatus() == kStatusNone);

    Require(sceLoginDialogInitialize() == 0);
    Require(sceLoginDialogGetStatus() == kStatusInitialized);
    Require(sceLoginDialogClose() == 0);
    Require(sceLoginDialogGetStatus() == kStatusFinished);
    result.result = -1;
    Require(sceLoginDialogGetResult(&result) == 0);
    Require(result.result == kResultUserCanceled);
    Require(sceLoginDialogTerminate() == 0);
    Require(sceLoginDialogTerminate() == kErrNotInitialized);
    Require(sceLoginDialogOpen(nullptr) == kErrArgNull);
    Require(sceLoginDialogOpen(&param) == kErrNotInitialized);
    Require(sceLoginDialogGetResult(nullptr) == kErrArgNull);
    Require(sceLoginDialogGetResult(&result) == kErrNotInitialized);
    Require(sceLoginDialogGetStatus() == kStatusNone);
    Require(sceLoginDialogUpdateStatus() == kStatusNone);
}

#include "prx/libSceNpEulaDialog/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;

constexpr int kErrNotInitialized = static_cast<int>(0x80B80003u);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004u);

}

int main() {
    Require(sceNpEulaDialogTerminate() == kErrNotInitialized);
    Require(sceNpEulaDialogOpen() == kErrNotInitialized);
    Require(sceNpEulaDialogClose() == kErrNotInitialized);
    Require(sceNpEulaDialogGetResult() == kErrNotInitialized);
    Require(sceNpEulaDialogCheckVersion() == kErrNotInitialized);
    Require(sceNpEulaDialogCheckVersionAbort() == kErrNotInitialized);
    Require(sceNpEulaDialogGetStatus() == kStatusNone);
    Require(sceNpEulaDialogUpdateStatus() == kStatusNone);

    Require(sceNpEulaDialogInitialize() == 0);
    Require(sceNpEulaDialogGetStatus() == kStatusInitialized);
    Require(sceNpEulaDialogUpdateStatus() == kStatusInitialized);
    Require(sceNpEulaDialogInitialize() == kErrAlreadyInitialized);
    Require(sceNpEulaDialogGetStatus() == kStatusInitialized);
    Require(sceNpEulaDialogUpdateStatus() == kStatusInitialized);

    Require(sceNpEulaDialogTerminate() == 0);
    Require(sceNpEulaDialogGetStatus() == kStatusNone);
    Require(sceNpEulaDialogUpdateStatus() == kStatusNone);
    Require(sceNpEulaDialogTerminate() == kErrNotInitialized);
    Require(sceNpEulaDialogOpen() == kErrNotInitialized);
    Require(sceNpEulaDialogClose() == kErrNotInitialized);
    Require(sceNpEulaDialogGetResult() == kErrNotInitialized);
    Require(sceNpEulaDialogCheckVersion() == kErrNotInitialized);
    Require(sceNpEulaDialogCheckVersionAbort() == kErrNotInitialized);

    Require(sceNpEulaDialogInitialize() == 0);
    Require(sceNpEulaDialogGetStatus() == kStatusInitialized);
    Require(sceNpEulaDialogTerminate() == 0);
    Require(sceNpEulaDialogGetStatus() == kStatusNone);
    Require(sceNpEulaDialogOpen() == kErrNotInitialized);
    Require(sceNpEulaDialogClose() == kErrNotInitialized);
    Require(sceNpEulaDialogGetResult() == kErrNotInitialized);
    Require(sceNpEulaDialogCheckVersion() == kErrNotInitialized);
    Require(sceNpEulaDialogCheckVersionAbort() == kErrNotInitialized);

    return 0;
}

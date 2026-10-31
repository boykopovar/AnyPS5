#include "prx/libSceConnectAccountDialog/include/InventoryExports.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003u);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004u);

}

int main() {
    Require(sceConnectAccountDialogGetStatus() == kStatusNone);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusNone);
    Require(sceConnectAccountDialogTerminate() == kErrNotInitialized);
    Require(sceConnectAccountDialogClose() == kErrNotInitialized);

    Require(sceConnectAccountDialogInitialize() == 0);
    Require(sceConnectAccountDialogInitialize() == kErrAlreadyInitialized);
    Require(sceConnectAccountDialogGetStatus() == kStatusInitialized);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusInitialized);

    Require(sceConnectAccountDialogClose() == 0);
    Require(sceConnectAccountDialogGetStatus() == kStatusFinished);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusFinished);
    Require(sceConnectAccountDialogClose() == 0);
    Require(sceConnectAccountDialogGetStatus() == kStatusFinished);

    Require(sceConnectAccountDialogTerminate() == 0);
    Require(sceConnectAccountDialogTerminate() == kErrNotInitialized);
    Require(sceConnectAccountDialogClose() == kErrNotInitialized);
    Require(sceConnectAccountDialogGetStatus() == kStatusNone);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusNone);

    Require(sceConnectAccountDialogInitialize() == 0);
    Require(sceConnectAccountDialogGetStatus() == kStatusInitialized);
    Require(sceConnectAccountDialogClose() == 0);
    Require(sceConnectAccountDialogGetStatus() == kStatusFinished);
    Require(sceConnectAccountDialogTerminate() == 0);
    Require(sceConnectAccountDialogGetStatus() == kStatusNone);

    return 0;
}

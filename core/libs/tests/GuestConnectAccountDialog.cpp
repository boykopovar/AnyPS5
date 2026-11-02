#include "prx/libSceConnectAccountDialog/libSceConnectAccountDialog.hpp"
#include <cstdlib>

extern "C" [[gnu::weak, noreturn]] void NotImplemented_nid_no_patch(const char*) {
    std::abort();
}

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

constexpr int kStatusNone = 0;
constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003);
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004);

}

int main() {
    Require(sceConnectAccountDialogTerminate() == kErrNotInitialized);
    Require(sceConnectAccountDialogClose() == kErrNotInitialized);
    Require(sceConnectAccountDialogGetStatus() == kStatusNone);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusNone);

    Require(sceConnectAccountDialogInitialize() == 0);
    Require(sceConnectAccountDialogInitialize() == kErrAlreadyInitialized);
    Require(sceConnectAccountDialogGetStatus() == kStatusInitialized);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusInitialized);

    Require(sceConnectAccountDialogClose() == 0);
    Require(sceConnectAccountDialogGetStatus() == kStatusFinished);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusFinished);

    Require(sceConnectAccountDialogTerminate() == 0);
    Require(sceConnectAccountDialogGetStatus() == kStatusNone);
    Require(sceConnectAccountDialogUpdateStatus() == kStatusNone);
    Require(sceConnectAccountDialogTerminate() == kErrNotInitialized);
    Require(sceConnectAccountDialogClose() == kErrNotInitialized);

    return 0;
}

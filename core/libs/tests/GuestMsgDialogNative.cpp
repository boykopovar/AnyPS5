#include "prx/libSceMsgDialog.native/libSceMsgDialog.native.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
extern int SceMsgDialogNativeModuleLoaded_nid_no_patch;

}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrAlreadyInitialized = static_cast<int>(0x80B80004u);
constexpr int kErrNotInitialized = static_cast<int>(0x80B80003u);
constexpr int kErrNotFinished = static_cast<int>(0x80B80005u);
constexpr int kErrNotRunning = static_cast<int>(0x80B8000Bu);
constexpr int kErrArgNull = static_cast<int>(0x80B8000Du);
constexpr int kButtonIdOk = 1;

}

int main() {
    Require(SceMsgDialogNativeModuleLoaded_nid_no_patch == 1);
    std::uint8_t param[0x60]{};
    MsgDialogResult result{};

    Require(sceMsgDialogTerminate() == kErrNotInitialized);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogOpen(param) == kErrNotInitialized);
    Require(sceMsgDialogGetResult(&result) == kErrNotInitialized);
    Require(sceMsgDialogUpdateStatus() == 0);
    Require(sceMsgDialogGetStatus() == 0);

    Require(sceMsgDialogInitialize() == 0);
    Require(sceMsgDialogInitialize() == kErrAlreadyInitialized);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogGetResult(&result) == kErrNotFinished);
    Require(sceMsgDialogGetResult(nullptr) == kErrArgNull);
    Require(sceMsgDialogOpen(nullptr) == kErrArgNull);
    Require(sceMsgDialogUpdateStatus() == kStatusInitialized);
    Require(sceMsgDialogGetStatus() == kStatusInitialized);

    Require(sceMsgDialogProgressBarSetValue(0, 50) == kErrNotRunning);
    Require(sceMsgDialogOpen(param) == 0);
    Require(sceMsgDialogUpdateStatus() == kStatusFinished);
    Require(sceMsgDialogGetStatus() == kStatusFinished);
    Require(sceMsgDialogProgressBarInc(0, 10) == kErrNotRunning);
    Require(sceMsgDialogProgressBarSetMsg(0, "progress") == kErrNotRunning);
    Require(sceMsgDialogProgressBarSetValue(0, 100) == kErrNotRunning);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogUpdateStatus() == kStatusFinished);
    Require(sceMsgDialogGetStatus() == kStatusFinished);

    result = MsgDialogResult{};
    result.button_id = -1;
    Require(sceMsgDialogGetResult(&result) == 0);
    Require(result.result == 0);
    Require(result.button_id == kButtonIdOk);

    Require(sceMsgDialogTerminate() == 0);
    Require(sceMsgDialogTerminate() == kErrNotInitialized);
    Require(sceMsgDialogOpen(param) == kErrNotInitialized);
    Require(sceMsgDialogGetResult(&result) == kErrNotInitialized);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogGetStatus() == 0);
}

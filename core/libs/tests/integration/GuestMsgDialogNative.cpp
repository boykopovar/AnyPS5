#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI sceMsgDialogInitialize(void);
int APS5_VABI sceMsgDialogOpen(const void* param);
int APS5_VABI sceMsgDialogGetStatus(void);
int APS5_VABI sceMsgDialogUpdateStatus(void);
int APS5_VABI sceMsgDialogGetResult(MsgDialogResult* result);
int APS5_VABI sceMsgDialogClose(void);
int APS5_VABI sceMsgDialogTerminate(void);
int APS5_VABI sceMsgDialogProgressBarInc(int target, std::uint32_t delta);
int APS5_VABI sceMsgDialogProgressBarSetMsg(int target, const char* msg);
int APS5_VABI sceMsgDialogProgressBarSetValue(int target, std::uint32_t rate);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kStatusInitialized = 1;
constexpr int kStatusFinished = 3;
constexpr int kErrNotRunning = static_cast<int>(0x80B8000Bu);
constexpr int kButtonIdOk = 1;

}

int main() {
    std::uint8_t param[0x60]{};

    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogUpdateStatus() == 0);
    Require(sceMsgDialogGetStatus() == 0);

    Require(sceMsgDialogInitialize() == 0);
    Require(sceMsgDialogClose() == kErrNotRunning);
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

    MsgDialogResult result{};
    result.button_id = -1;
    Require(sceMsgDialogGetResult(&result) == 0);
    Require(result.result == 0);
    Require(result.button_id == kButtonIdOk);

    Require(sceMsgDialogTerminate() == 0);
    Require(sceMsgDialogClose() == kErrNotRunning);
    Require(sceMsgDialogGetStatus() == 0);
}

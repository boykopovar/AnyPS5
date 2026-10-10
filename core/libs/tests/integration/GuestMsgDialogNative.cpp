#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <exception>

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

using Testing::Case;
using Testing::RequireEqual;

constexpr int statusNone = 0;
constexpr int statusInitialized = 1;
constexpr int statusFinished = 3;
constexpr int errNotRunning = static_cast<int>(0x80B8000Bu);
constexpr int buttonIdOk = 1;

class InitializedDialog {
public:
    InitializedDialog() {
        RequireEqual(sceMsgDialogInitialize(), 0, "initialize");
    }

    ~InitializedDialog() {
        if (sceMsgDialogGetStatus() == statusNone) return;
        try {
            sceMsgDialogTerminate();
        } catch (const std::exception&) {
        }
    }

    InitializedDialog(const InitializedDialog&) = delete;
    InitializedDialog& operator=(const InitializedDialog&) = delete;
};

void Open() {
    std::uint8_t param[0x60]{};
    RequireEqual(sceMsgDialogOpen(param), 0, "open");
}

const Case beforeInit{"Calls_BeforeInitialize_ReportNoneAndNotRunning", [] {
    RequireEqual(sceMsgDialogClose(), errNotRunning, "close");
    RequireEqual(sceMsgDialogUpdateStatus(), statusNone, "updated status");
    RequireEqual(sceMsgDialogGetStatus(), statusNone, "status");
}};

const Case initialize{"Initialize_FromNone_ReportsInitializedAndRejectsClose", [] {
    const InitializedDialog dialog;
    RequireEqual(sceMsgDialogClose(), errNotRunning, "close");
    RequireEqual(sceMsgDialogUpdateStatus(), statusInitialized, "updated status");
    RequireEqual(sceMsgDialogGetStatus(), statusInitialized, "status");
}};

const Case progressBeforeOpen{"ProgressBarSetValue_BeforeOpen_ReturnsNotRunning", [] {
    const InitializedDialog dialog;
    RequireEqual(sceMsgDialogProgressBarSetValue(0, 50), errNotRunning, "set progress value");
}};

const Case open{"Open_AfterInitialize_FinishesImmediately", [] {
    const InitializedDialog dialog;
    Open();
    RequireEqual(sceMsgDialogUpdateStatus(), statusFinished, "updated status");
    RequireEqual(sceMsgDialogGetStatus(), statusFinished, "status");
}};

const Case progressAfterOpen{"ProgressBarCalls_AfterOpen_ReturnNotRunning", [] {
    const InitializedDialog dialog;
    Open();
    RequireEqual(sceMsgDialogProgressBarInc(0, 10), errNotRunning, "increment progress");
    RequireEqual(sceMsgDialogProgressBarSetMsg(0, "progress"), errNotRunning, "set progress message");
    RequireEqual(sceMsgDialogProgressBarSetValue(0, 100), errNotRunning, "set progress value");
}};

const Case closeAfterOpen{"Close_AfterOpen_ReturnsNotRunningAndStaysFinished", [] {
    const InitializedDialog dialog;
    Open();
    RequireEqual(sceMsgDialogClose(), errNotRunning, "close");
    RequireEqual(sceMsgDialogUpdateStatus(), statusFinished, "updated status");
    RequireEqual(sceMsgDialogGetStatus(), statusFinished, "status");
}};

const Case result{"GetResult_AfterOpen_ReportsOkButton", [] {
    const InitializedDialog dialog;
    Open();
    MsgDialogResult dialogResult{};
    dialogResult.button_id = -1;
    RequireEqual(sceMsgDialogGetResult(&dialogResult), 0, "get result");
    RequireEqual(dialogResult.result, 0, "result");
    RequireEqual(dialogResult.button_id, buttonIdOk, "button id");
}};

const Case terminate{"Terminate_AfterOpen_ReturnsToNone", [] {
    RequireEqual(sceMsgDialogInitialize(), 0, "initialize");
    Open();
    RequireEqual(sceMsgDialogTerminate(), 0, "terminate");
    RequireEqual(sceMsgDialogClose(), errNotRunning, "close after terminate");
    RequireEqual(sceMsgDialogGetStatus(), statusNone, "status after terminate");
}};

} // namespace

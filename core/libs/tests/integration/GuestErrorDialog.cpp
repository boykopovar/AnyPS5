#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>

extern "C" {
int APS5_VABI sceErrorDialogInitialize(void);
int APS5_VABI sceErrorDialogOpen(const void* param);
int APS5_VABI sceErrorDialogUpdateStatus(void);
int APS5_VABI sceErrorDialogGetStatus(void);
int APS5_VABI sceErrorDialogClose(void);
int APS5_VABI sceErrorDialogTerminate(void);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int statusRunning = 2;
constexpr int statusFinished = 3;
constexpr int errNotInitialized = static_cast<int>(0x80ED0001);
constexpr int errAlreadyInitialized = static_cast<int>(0x80ED0002);
constexpr int errParam = static_cast<int>(0x80ED0003);
constexpr int errInvalidState = static_cast<int>(0x80ED0005);

struct ErrorDialogParam {
    std::int32_t size;
    std::int32_t error_code;
    std::int32_t user_id;
    std::int32_t reserved;
};

constexpr ErrorDialogParam validParam{16, static_cast<std::int32_t>(0x80020001), 0, 0};

class InitializedDialog {
public:
    InitializedDialog() {
        RequireEqual(sceErrorDialogInitialize(), 0, "initialize");
    }

    ~InitializedDialog() {
        sceErrorDialogTerminate();
    }

    InitializedDialog(const InitializedDialog&) = delete;
    InitializedDialog& operator=(const InitializedDialog&) = delete;
};

const Case terminateBeforeInit{"Terminate_BeforeInitialize_ReturnsNotInitialized", [] {
    RequireEqual(sceErrorDialogTerminate(), errNotInitialized, "terminate");
}};

const Case openBeforeInit{"Open_BeforeInitialize_ReturnsInvalidState", [] {
    RequireEqual(sceErrorDialogOpen(&validParam), errInvalidState, "open");
}};

const Case initializeTwice{"Initialize_AlreadyInitialized_ReturnsAlreadyInitialized", [] {
    const InitializedDialog dialog;
    RequireEqual(sceErrorDialogInitialize(), errAlreadyInitialized, "second initialize");
}};

const Case closeNotRunning{"Close_NotRunning_ReturnsInvalidState", [] {
    const InitializedDialog dialog;
    RequireEqual(sceErrorDialogClose(), errInvalidState, "close");
}};

const Case openInvalidParam{"Open_NullOrUndersizedParam_ReturnsParamError", [] {
    const InitializedDialog dialog;
    const ErrorDialogParam undersized{8, 0, 0, 0};
    RequireEqual(sceErrorDialogOpen(nullptr), errParam, "open with a null param");
    RequireEqual(sceErrorDialogOpen(&undersized), errParam, "open with an undersized param");
}};

const Case openRunning{"Open_AfterInitialize_RunsAndRejectsSecondOpen", [] {
    const InitializedDialog dialog;
    RequireEqual(sceErrorDialogOpen(&validParam), 0, "open");
    RequireEqual(sceErrorDialogGetStatus(), statusRunning, "status");
    RequireEqual(sceErrorDialogOpen(&validParam), errInvalidState, "second open");
}};

const Case updateFinishes{"UpdateStatus_WhileRunning_Finishes", [] {
    const InitializedDialog dialog;
    RequireEqual(sceErrorDialogOpen(&validParam), 0, "open");
    RequireEqual(sceErrorDialogUpdateStatus(), statusFinished, "updated status");
    RequireEqual(sceErrorDialogGetStatus(), statusFinished, "status");
}};

const Case reopenAndClose{"Close_AfterReopenFromFinished_Finishes", [] {
    const InitializedDialog dialog;
    RequireEqual(sceErrorDialogOpen(&validParam), 0, "first open");
    RequireEqual(sceErrorDialogUpdateStatus(), statusFinished, "updated status");
    RequireEqual(sceErrorDialogOpen(&validParam), 0, "open from finished");
    RequireEqual(sceErrorDialogGetStatus(), statusRunning, "status after reopen");
    RequireEqual(sceErrorDialogClose(), 0, "close");
    RequireEqual(sceErrorDialogGetStatus(), statusFinished, "status after close");
}};

const Case terminate{"Terminate_AfterClose_SucceedsOnce", [] {
    RequireEqual(sceErrorDialogInitialize(), 0, "initialize");
    RequireEqual(sceErrorDialogOpen(&validParam), 0, "open");
    RequireEqual(sceErrorDialogClose(), 0, "close");
    RequireEqual(sceErrorDialogTerminate(), 0, "terminate");
    RequireEqual(sceErrorDialogTerminate(), errNotInitialized, "second terminate");
}};

} // namespace

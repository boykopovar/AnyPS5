#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>

extern "C" {
int APS5_VABI sceWebBrowserDialogInitialize(void);
int APS5_VABI sceWebBrowserDialogTerminate(void);
int APS5_VABI sceWebBrowserDialogOpen(const void* param);
int APS5_VABI sceWebBrowserDialogGetStatus(void);
int APS5_VABI sceWebBrowserDialogUpdateStatus(void);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int statusNone = 0;
constexpr int statusInitialized = 1;
constexpr int statusFinished = 3;

class InitializedDialog {
public:
    InitializedDialog() {
        RequireEqual(sceWebBrowserDialogInitialize(), 0, "initialize");
    }

    ~InitializedDialog() {
        if (sceWebBrowserDialogGetStatus() == statusInitialized) sceWebBrowserDialogTerminate();
    }

    InitializedDialog(const InitializedDialog&) = delete;
    InitializedDialog& operator=(const InitializedDialog&) = delete;
};

const Case statusBeforeInit{"GetStatus_BeforeInitialize_ReturnsNone", [] {
    RequireEqual(sceWebBrowserDialogGetStatus(), statusNone, "status before initialize");
}};

const Case initialize{"Initialize_FromNone_ReportsInitialized", [] {
    const InitializedDialog dialog;
    RequireEqual(sceWebBrowserDialogGetStatus(), statusInitialized, "status after initialize");
}};

const Case terminate{"Terminate_AfterInitialize_ReturnsToNone", [] {
    RequireEqual(sceWebBrowserDialogInitialize(), 0, "initialize");
    RequireEqual(sceWebBrowserDialogTerminate(), 0, "terminate");
    RequireEqual(sceWebBrowserDialogGetStatus(), statusNone, "status after terminate");
}};

const Case open{"Open_AfterInitialize_FinishesImmediately", [] {
    const InitializedDialog dialog;
    std::uint8_t param[64] = {};
    RequireEqual(sceWebBrowserDialogOpen(param), 0, "open");
    RequireEqual(sceWebBrowserDialogGetStatus(), statusFinished, "status after open");
    RequireEqual(sceWebBrowserDialogUpdateStatus(), statusFinished, "updated status after open");
}};

} // namespace

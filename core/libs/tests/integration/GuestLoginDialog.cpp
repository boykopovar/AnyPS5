#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>

extern "C" {
int APS5_VABI sceLoginDialogInitialize(void);
int APS5_VABI sceLoginDialogOpen(const void* param);
int APS5_VABI sceLoginDialogUpdateStatus(void);
int APS5_VABI sceLoginDialogGetStatus(void);
int APS5_VABI sceLoginDialogGetResult(void* result);
int APS5_VABI sceLoginDialogClose(void);
int APS5_VABI sceLoginDialogTerminate(void);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int statusFinished = 3;
constexpr int errNotInitialized = static_cast<int>(0x80B80003);
constexpr int errAlreadyInitialized = static_cast<int>(0x80B80004);
constexpr int errNotFinished = static_cast<int>(0x80B80005);
constexpr int errArgNull = static_cast<int>(0x80B8000D);
constexpr int resultUserCanceled = 1;

class InitializedDialog {
public:
    InitializedDialog() {
        RequireEqual(sceLoginDialogInitialize(), 0, "initialize");
    }

    ~InitializedDialog() {
        sceLoginDialogTerminate();
    }

    InitializedDialog(const InitializedDialog&) = delete;
    InitializedDialog& operator=(const InitializedDialog&) = delete;
};

const Case beforeInit{"Calls_BeforeInitialize_ReturnNotInitialized", [] {
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};
    RequireEqual(sceLoginDialogTerminate(), errNotInitialized, "terminate");
    RequireEqual(sceLoginDialogOpen(&param), errNotInitialized, "open");
    RequireEqual(sceLoginDialogGetResult(result), errNotInitialized, "get result");
}};

const Case initializeTwice{"Initialize_AlreadyInitialized_ReturnsAlreadyInitialized", [] {
    const InitializedDialog dialog;
    RequireEqual(sceLoginDialogInitialize(), errAlreadyInitialized, "second initialize");
}};

const Case openNull{"Open_NullParam_ReturnsArgNull", [] {
    const InitializedDialog dialog;
    RequireEqual(sceLoginDialogOpen(nullptr), errArgNull, "open with a null param");
}};

const Case resultBeforeOpen{"GetResult_BeforeOpen_ReturnsNotFinished", [] {
    const InitializedDialog dialog;
    std::int32_t result[2] = {-1, -1};
    RequireEqual(sceLoginDialogGetResult(result), errNotFinished, "get result");
}};

const Case open{"Open_AfterInitialize_FinishesImmediately", [] {
    const InitializedDialog dialog;
    std::int64_t param = 0;
    RequireEqual(sceLoginDialogOpen(&param), 0, "open");
    RequireEqual(sceLoginDialogGetStatus(), statusFinished, "status");
    RequireEqual(sceLoginDialogUpdateStatus(), statusFinished, "updated status");
}};

const Case resultNull{"GetResult_NullOutputAfterOpen_ReturnsArgNull", [] {
    const InitializedDialog dialog;
    std::int64_t param = 0;
    RequireEqual(sceLoginDialogOpen(&param), 0, "open");
    RequireEqual(sceLoginDialogGetResult(nullptr), errArgNull, "get result with a null output");
}};

const Case resultAfterOpen{"GetResult_AfterOpen_ReportsUserCanceledInFirstWordOnly", [] {
    const InitializedDialog dialog;
    std::int64_t param = 0;
    std::int32_t result[2] = {-1, -1};
    RequireEqual(sceLoginDialogOpen(&param), 0, "open");
    RequireEqual(sceLoginDialogGetResult(result), 0, "get result");
    RequireEqual(result[0], resultUserCanceled, "result code");
    RequireEqual(result[1], -1, "word after the result code");
}};

const Case closeAndTerminate{"CloseAndTerminate_AfterOpen_SucceedOnce", [] {
    std::int64_t param = 0;
    RequireEqual(sceLoginDialogInitialize(), 0, "initialize");
    RequireEqual(sceLoginDialogOpen(&param), 0, "open");
    RequireEqual(sceLoginDialogClose(), 0, "close");
    RequireEqual(sceLoginDialogTerminate(), 0, "terminate");
    RequireEqual(sceLoginDialogTerminate(), errNotInitialized, "second terminate");
}};

} // namespace

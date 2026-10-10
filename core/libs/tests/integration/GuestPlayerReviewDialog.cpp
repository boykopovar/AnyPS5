#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstdint>
#include <exception>

extern "C" {
int APS5_VABI scePlayerReviewDialogInitialize(void);
int APS5_VABI scePlayerReviewDialogTerminate(void);
int APS5_VABI scePlayerReviewDialogOpen(const void* param);
int APS5_VABI scePlayerReviewDialogClose(void);
int APS5_VABI scePlayerReviewDialogGetStatus(void);
int APS5_VABI scePlayerReviewDialogUpdateStatus(void);
int APS5_VABI scePlayerReviewDialogGetResult(void* result);
}

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr int statusNone = 0;
constexpr int statusInitialized = 1;
constexpr int statusFinished = 3;
constexpr int resultUserCanceled = 1;
constexpr int errNotInitialized = static_cast<int>(0x80B80003u);
constexpr int errNotFinished = static_cast<int>(0x80B80005u);
constexpr int errArgNull = static_cast<int>(0x80B8000Du);

class InitializedDialog {
public:
    InitializedDialog() {
        RequireEqual(scePlayerReviewDialogInitialize(), 0, "initialize");
    }

    ~InitializedDialog() {
        if (scePlayerReviewDialogGetStatus() == statusNone) return;
        try {
            scePlayerReviewDialogTerminate();
        } catch (const std::exception&) {
        }
    }

    InitializedDialog(const InitializedDialog&) = delete;
    InitializedDialog& operator=(const InitializedDialog&) = delete;
};

const Case beforeInit{"Calls_BeforeInitialize_ReportNoneAndNotInitialized", [] {
    std::uint8_t param[64] = {};
    std::int32_t result[8] = {};
    RequireEqual(scePlayerReviewDialogGetStatus(), statusNone, "status");
    RequireEqual(scePlayerReviewDialogOpen(param), errNotInitialized, "open");
    RequireEqual(scePlayerReviewDialogClose(), errNotInitialized, "close");
    RequireEqual(scePlayerReviewDialogGetResult(result), errNotInitialized, "get result");
}};

const Case initialize{"Initialize_FromNone_ReportsInitialized", [] {
    const InitializedDialog dialog;
    RequireEqual(scePlayerReviewDialogGetStatus(), statusInitialized, "status");
    RequireEqual(scePlayerReviewDialogUpdateStatus(), statusInitialized, "updated status");
}};

const Case resultBeforeOpen{"GetResult_BeforeOpen_ReturnsNotFinished", [] {
    const InitializedDialog dialog;
    std::int32_t result[8] = {};
    RequireEqual(scePlayerReviewDialogGetResult(result), errNotFinished, "get result");
}};

const Case terminateInitialized{"Terminate_AfterInitialize_ReturnsToNone", [] {
    RequireEqual(scePlayerReviewDialogInitialize(), 0, "initialize");
    RequireEqual(scePlayerReviewDialogTerminate(), 0, "terminate");
    RequireEqual(scePlayerReviewDialogGetStatus(), statusNone, "status after terminate");
}};

const Case openNull{"Open_NullParam_ReturnsArgNull", [] {
    const InitializedDialog dialog;
    RequireEqual(scePlayerReviewDialogOpen(nullptr), errArgNull, "open with a null param");
}};

const Case open{"Open_AfterInitialize_FinishesImmediately", [] {
    const InitializedDialog dialog;
    std::uint8_t param[64] = {};
    RequireEqual(scePlayerReviewDialogOpen(param), 0, "open");
    RequireEqual(scePlayerReviewDialogGetStatus(), statusFinished, "status");
    RequireEqual(scePlayerReviewDialogUpdateStatus(), statusFinished, "updated status");
}};

const Case resultNull{"GetResult_NullOutputAfterOpen_ReturnsArgNull", [] {
    const InitializedDialog dialog;
    std::uint8_t param[64] = {};
    RequireEqual(scePlayerReviewDialogOpen(param), 0, "open");
    RequireEqual(scePlayerReviewDialogGetResult(nullptr), errArgNull, "get result with a null output");
}};

const Case resultAfterOpen{"GetResult_AfterOpen_ReportsUserCanceled", [] {
    const InitializedDialog dialog;
    std::uint8_t param[64] = {};
    std::int32_t result[8] = {};
    RequireEqual(scePlayerReviewDialogOpen(param), 0, "open");
    RequireEqual(scePlayerReviewDialogGetResult(result), 0, "get result");
    RequireEqual(result[0], resultUserCanceled, "result code");
}};

const Case closeAndTerminate{"CloseAndTerminate_AfterOpen_ReturnToNone", [] {
    std::uint8_t param[64] = {};
    RequireEqual(scePlayerReviewDialogInitialize(), 0, "initialize");
    RequireEqual(scePlayerReviewDialogOpen(param), 0, "open");
    RequireEqual(scePlayerReviewDialogClose(), 0, "close");
    RequireEqual(scePlayerReviewDialogTerminate(), 0, "terminate");
    RequireEqual(scePlayerReviewDialogGetStatus(), statusNone, "status after terminate");
}};

const Case reinitialize{"Initialize_AfterTerminate_SucceedsAgain", [] {
    std::uint8_t param[64] = {};
    RequireEqual(scePlayerReviewDialogInitialize(), 0, "first initialize");
    RequireEqual(scePlayerReviewDialogOpen(param), 0, "open");
    RequireEqual(scePlayerReviewDialogTerminate(), 0, "first terminate");
    RequireEqual(scePlayerReviewDialogInitialize(), 0, "second initialize");
    RequireEqual(scePlayerReviewDialogTerminate(), 0, "second terminate");
}};

} // namespace

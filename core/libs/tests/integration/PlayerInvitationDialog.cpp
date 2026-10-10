#include "prx/libScePlayerInvitationDialog/libScePlayerInvitationDialog.h"

#include <Testing/Test.hpp>

#include <cstdint>
#include <cstring>
#include <string>

namespace {

using Testing::Case;
using Testing::RequireEqual;

constexpr auto notInitialized = static_cast<std::int32_t>(0x80B80003u);
constexpr auto notFinished = static_cast<std::int32_t>(0x80B80005u);
constexpr auto argNull = static_cast<std::int32_t>(0x80B8000Du);
constexpr std::int32_t userCanceled = 1;

constexpr std::int32_t StatusValue(PlayerInvitationDialogStatus status) {
    return static_cast<std::int32_t>(status);
}

class DialogSession {
public:
    DialogSession() {
        RequireEqual(scePlayerInvitationDialogInitialize(), 0, "initialize");
    }

    ~DialogSession() {
        scePlayerInvitationDialogTerminate();
    }

    DialogSession(const DialogSession&) = delete;
    DialogSession& operator=(const DialogSession&) = delete;
};

void OpenAndFinish() {
    ScePlayerInvitationDialogParam param{};
    RequireEqual(scePlayerInvitationDialogOpen(&param), 0, "open");
    RequireEqual(scePlayerInvitationDialogUpdateStatus(), StatusValue(PlayerInvitationDialogStatus::Finished), "status update after open");
}

const Case beforeInit{"Calls_BeforeInitialize_ReportNoneAndSucceedForCloseAndTerminate", [] {
    ScePlayerInvitationDialogResult result{};
    RequireEqual(scePlayerInvitationDialogGetResult(&result), notInitialized, "get result");
    RequireEqual(scePlayerInvitationDialogUpdateStatus(), StatusValue(PlayerInvitationDialogStatus::None), "update status");
    RequireEqual(scePlayerInvitationDialogClose(), 0, "close");
    RequireEqual(scePlayerInvitationDialogTerminate(), 0, "terminate");
}};

const Case initializeTwice{"Initialize_Twice_SucceedsAndReportsInitialized", [] {
    const DialogSession session;
    RequireEqual(scePlayerInvitationDialogInitialize(), 0, "second initialize");
    RequireEqual(scePlayerInvitationDialogGetStatus(), StatusValue(PlayerInvitationDialogStatus::Initialized), "status");
    RequireEqual(scePlayerInvitationDialogUpdateStatus(), StatusValue(PlayerInvitationDialogStatus::Initialized), "updated status");
}};

const Case resultNull{"GetResult_NullOutputWhileInitialized_ReturnsArgNull", [] {
    const DialogSession session;
    RequireEqual(scePlayerInvitationDialogGetResult(nullptr), argNull, "get result with a null output");
}};

const Case resultBeforeOpen{"GetResult_BeforeOpen_ReturnsNotFinished", [] {
    const DialogSession session;
    ScePlayerInvitationDialogResult result{};
    RequireEqual(scePlayerInvitationDialogGetResult(&result), notFinished, "get result");
}};

const Case openRuns{"Open_AfterInitialize_RunsUntilStatusUpdate", [] {
    const DialogSession session;
    ScePlayerInvitationDialogParam param{};
    ScePlayerInvitationDialogResult result{};
    RequireEqual(scePlayerInvitationDialogOpen(&param), 0, "open");
    RequireEqual(scePlayerInvitationDialogGetStatus(), StatusValue(PlayerInvitationDialogStatus::Running), "status after open");
    RequireEqual(scePlayerInvitationDialogGetResult(&result), notFinished, "get result while running");
    RequireEqual(scePlayerInvitationDialogUpdateStatus(), StatusValue(PlayerInvitationDialogStatus::Finished), "status update");
}};

const Case resultAfterFinish{"GetResult_AfterFinish_ReportsUserCanceledWithZeroedFields", [] {
    const DialogSession session;
    OpenAndFinish();
    ScePlayerInvitationDialogResult result{};
    std::memset(&result, 0xFF, sizeof(result));
    RequireEqual(scePlayerInvitationDialogGetResult(&result), 0, "get result");
    RequireEqual(result.errorCode, 0, "error code");
    RequireEqual(result.result, userCanceled, "result");
    for (std::size_t index = 0; index < sizeof(result.reserved); ++index) {
        RequireEqual(result.reserved[index], std::uint8_t{0}, "reserved byte " + std::to_string(index));
    }
    RequireEqual(scePlayerInvitationDialogUpdateStatus(), StatusValue(PlayerInvitationDialogStatus::Finished), "status after get result");
}};

const Case reopenAndClose{"OpenAndClose_WhileFinished_EndFinished", [] {
    const DialogSession session;
    OpenAndFinish();
    RequireEqual(scePlayerInvitationDialogOpen(nullptr), 0, "open with a null param");
    RequireEqual(scePlayerInvitationDialogClose(), 0, "close");
    RequireEqual(scePlayerInvitationDialogGetStatus(), StatusValue(PlayerInvitationDialogStatus::Finished), "status after close");
}};

const Case terminate{"Terminate_AfterFinish_ReturnsToNone", [] {
    RequireEqual(scePlayerInvitationDialogInitialize(), 0, "initialize");
    OpenAndFinish();
    ScePlayerInvitationDialogResult result{};
    RequireEqual(scePlayerInvitationDialogTerminate(), 0, "terminate");
    RequireEqual(scePlayerInvitationDialogGetStatus(), StatusValue(PlayerInvitationDialogStatus::None), "status after terminate");
    RequireEqual(scePlayerInvitationDialogGetResult(&result), notInitialized, "get result after terminate");
}};

const Case openWithoutInit{"Open_WithoutInitialize_RunsAndFinishes", [] {
    RequireEqual(scePlayerInvitationDialogOpen(nullptr), 0, "open with a null param");
    RequireEqual(scePlayerInvitationDialogUpdateStatus(), StatusValue(PlayerInvitationDialogStatus::Finished), "status update");
    RequireEqual(scePlayerInvitationDialogTerminate(), 0, "terminate");
}};

} // namespace

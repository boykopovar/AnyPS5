#include "prx/libSceSaveDataDialog.native/SaveDataDialog.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstring>
#include <string>

extern "C" {
int APS5_VABI sceSaveDataDialogInitialize();
int APS5_VABI sceSaveDataDialogOpen(const void* param);
int APS5_VABI sceSaveDataDialogGetStatus();
int APS5_VABI sceSaveDataDialogUpdateStatus();
int APS5_VABI sceSaveDataDialogIsReadyToDisplay();
int APS5_VABI sceSaveDataDialogProgressBarInc(int target, std::uint32_t delta);
int APS5_VABI sceSaveDataDialogProgressBarSetValue(int target, std::uint32_t rate);
int APS5_VABI sceSaveDataDialogClose(const void* param);
int APS5_VABI sceSaveDataDialogGetResult(void* result);
int APS5_VABI sceSaveDataDialogTerminate();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

class SaveDialogFixture {
public:
    SaveDialogFixture() {
        RequireEqual(sceSaveDataDialogInitialize(), 0, "initialize");
        items.dir_names = names;
        items.dir_names_num = 2;
        param.size = sizeof(param);
        param.mode = SAVE_DATA_DIALOG_MODE_PROGRESS_BAR;
        param.items = &items;
        param.user_data = &context;
    }

    ~SaveDialogFixture() {
        sceSaveDataDialogTerminate();
    }

    SaveDialogFixture(const SaveDialogFixture&) = delete;
    SaveDialogFixture& operator=(const SaveDialogFixture&) = delete;

    SaveDataDirName names[2]{{}, {"SDU1"}};
    SaveDataDialogItems items{};
    int context = 42;
    SaveDataDialogParam param{};
};

void RequireRunning(const std::string& context) {
    RequireEqual(sceSaveDataDialogGetStatus(), SAVE_DATA_DIALOG_STATUS_RUNNING, context + ": save progress must remain running until closed");
    RequireEqual(sceSaveDataDialogUpdateStatus(), SAVE_DATA_DIALOG_STATUS_RUNNING, context + ": polling save progress must not finish the dialog");
    RequireEqual(sceSaveDataDialogIsReadyToDisplay(), 1, context + ": save progress must be ready");
}

void OpenAndClose(SaveDialogFixture& fixture) {
    RequireEqual(sceSaveDataDialogOpen(&fixture.param), 0, "open save progress");
    RequireEqual(sceSaveDataDialogClose(nullptr), 0, "close save progress");
}

void RequireSaveResult(const SaveDialogFixture& fixture) {
    SaveDataDirName selected{};
    SaveDataDialogResult result{};
    result.dir_name = &selected;
    RequireEqual(sceSaveDataDialogGetResult(&result), 0, "read completed result");
    RequireEqual(result.mode, fixture.param.mode, "result mode");
    RequireEqual(result.result, SAVE_DATA_DIALOG_RESULT_OK, "result code");
    Require(result.user_data == &fixture.context, "save result must preserve the caller context");
    RequireEqual(std::string(selected.data), std::string("SDU1"), "selected directory");
}

const Case pollingKeepsRunning{"Open_ProgressBarMode_StaysRunningWhilePolled", [] {
    SaveDialogFixture fixture;
    RequireEqual(sceSaveDataDialogOpen(&fixture.param), 0, "open save progress");
    for (int poll = 0; poll < 8; ++poll) RequireRunning("poll " + std::to_string(poll));
}};

const Case openWhileRunning{"Open_WhileSaveRunning_ReturnsInvalidState", [] {
    SaveDialogFixture fixture;
    RequireEqual(sceSaveDataDialogOpen(&fixture.param), 0, "open save progress");
    RequireEqual(sceSaveDataDialogOpen(&fixture.param), SAVE_DATA_DIALOG_ERROR_INVALID_STATE, "opening another dialog must not interrupt an active save");
}};

const Case progressUpdates{"ProgressBar_SetAndIncrement_KeepDialogRunning", [] {
    SaveDialogFixture fixture;
    RequireEqual(sceSaveDataDialogOpen(&fixture.param), 0, "open save progress");
    RequireEqual(sceSaveDataDialogProgressBarSetValue(0, 25), 0, "set progress");
    RequireEqual(sceSaveDataDialogProgressBarInc(0, 25), 0, "increment progress");
    RequireRunning("after partial progress");
    RequireEqual(sceSaveDataDialogProgressBarSetValue(0, 100), 0, "complete progress");
    RequireRunning("after complete progress");
}};

const Case closeFinishes{"Close_RunningSave_FinishesDialog", [] {
    SaveDialogFixture fixture;
    OpenAndClose(fixture);
    RequireEqual(sceSaveDataDialogUpdateStatus(), SAVE_DATA_DIALOG_STATUS_FINISHED, "closing save progress must finish it");
}};

const Case resultAfterClose{"GetResult_AfterClose_PreservesModeDirectoryAndContext", [] {
    SaveDialogFixture fixture;
    OpenAndClose(fixture);
    RequireSaveResult(fixture);
}};

const Case secondSave{"Open_AfterPreviousSaveFinished_RunsSecondSave", [] {
    SaveDialogFixture fixture;
    OpenAndClose(fixture);
    RequireEqual(sceSaveDataDialogOpen(&fixture.param), 0, "open the second save");
    RequireRunning("second save");
    RequireEqual(sceSaveDataDialogClose(nullptr), 0, "close the second save");
    RequireEqual(sceSaveDataDialogUpdateStatus(), SAVE_DATA_DIALOG_STATUS_FINISHED, "second save finished");
    RequireSaveResult(fixture);
}};

const Case selectionDialog{"Open_SelectionModeAfterSave_FinishesImmediately", [] {
    SaveDialogFixture fixture;
    OpenAndClose(fixture);
    fixture.param.mode = 1;
    RequireEqual(sceSaveDataDialogOpen(&fixture.param), 0, "open selection dialog");
    RequireEqual(sceSaveDataDialogUpdateStatus(), SAVE_DATA_DIALOG_STATUS_FINISHED, "selection dialog must retain the existing automatic response");
}};

const Case terminate{"Terminate_AfterDialog_ReturnsToNone", [] {
    SaveDialogFixture fixture;
    OpenAndClose(fixture);
    RequireEqual(sceSaveDataDialogTerminate(), 0, "terminate");
    RequireEqual(sceSaveDataDialogGetStatus(), SAVE_DATA_DIALOG_STATUS_NONE, "terminated state");
}};

} // namespace

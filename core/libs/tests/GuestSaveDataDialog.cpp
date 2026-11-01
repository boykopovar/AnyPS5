#include "prx/libSceSaveDataDialog/include/Api.hpp"
#include "prx/libSceSaveDataDialog.native/include/Api.hpp"
#include "prx/libSceSaveDataDialog.native/SaveDataDialog.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* message) {
    if (!value) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

static void RequireRunning() {
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_RUNNING,
            "save progress must remain running until closed");
    Require(sceSaveDataDialogUpdateStatus() == SAVE_DATA_DIALOG_STATUS_RUNNING,
            "polling save progress must not finish the dialog");
    Require(sceSaveDataDialogIsReadyToDisplay() == 1, "save progress must be ready");
}

int main() {
    SaveDataDialogParam param{};
    param.size = sizeof(param);

    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_NONE, "initial status");
    Require(sceSaveDataDialogTerminate() == SAVE_DATA_DIALOG_ERROR_NOT_INITIALIZED, "terminate before init");
    Require(sceSaveDataDialogOpen(&param) == SAVE_DATA_DIALOG_ERROR_INVALID_STATE, "open before init");
    Require(sceSaveDataDialogClose(nullptr) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "close before init");
    SaveDataDialogResult uninitResult{};
    Require(sceSaveDataDialogGetResult(&uninitResult) == SAVE_DATA_DIALOG_ERROR_NOT_FINISHED, "get result before init");
    Require(sceSaveDataDialogProgressBarInc(0, 10) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress inc before init");
    Require(sceSaveDataDialogProgressBarSetValue(0, 50) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress set value before init");

    Require(sceSaveDataDialogInitialize() == 0, "initialize");
    Require(sceSaveDataDialogInitialize() == SAVE_DATA_DIALOG_ERROR_ALREADY_INITIALIZED, "double init");
    Require(sceSaveDataDialogOpen(nullptr) == SAVE_DATA_DIALOG_ERROR_ARG_NULL, "open with null param");
    Require(sceSaveDataDialogClose(nullptr) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "close while initialized not running");
    Require(sceSaveDataDialogGetResult(&uninitResult) == SAVE_DATA_DIALOG_ERROR_NOT_FINISHED, "get result before open");
    Require(sceSaveDataDialogProgressBarInc(0, 10) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress inc while initialized not running");
    Require(sceSaveDataDialogProgressBarSetValue(0, 50) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress set value while initialized not running");

    SaveDataDirName names[2]{{}, {"SDU1"}};
    SaveDataDialogItems items{};
    items.dir_names = names;
    items.dir_names_num = 2;
    int context = 42;
    param = {};
    param.size = sizeof(param);
    param.mode = SAVE_DATA_DIALOG_MODE_PROGRESS_BAR;
    param.items = &items;
    param.user_data = &context;

    for (int save = 0; save < 2; ++save) {
        Require(sceSaveDataDialogOpen(&param) == 0, "open save progress");
        for (int poll = 0; poll < 8; ++poll) RequireRunning();
        Require(sceSaveDataDialogOpen(&param) == SAVE_DATA_DIALOG_ERROR_INVALID_STATE,
                "opening another dialog must not interrupt an active save");
        Require(sceSaveDataDialogProgressBarSetValue(0, 25) == 0, "set progress");
        Require(sceSaveDataDialogProgressBarInc(0, 25) == 0, "increment progress");
        RequireRunning();
        Require(sceSaveDataDialogProgressBarInc(0, 10) == 0, "inc progress while running");
        Require(sceSaveDataDialogProgressBarSetValue(0, 100) == 0, "complete progress");
        RequireRunning();
        Require(sceSaveDataDialogGetResult(&uninitResult) == SAVE_DATA_DIALOG_ERROR_NOT_FINISHED,
                "get result while running");
        Require(sceSaveDataDialogClose(nullptr) == 0, "close save progress");
        Require(sceSaveDataDialogUpdateStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED,
                "closing save progress must finish it");
        SaveDataDirName selected{};
        SaveDataDialogResult result{};
        result.dir_name = &selected;
        Require(sceSaveDataDialogGetResult(&result) == 0, "read completed result");
        Require(sceSaveDataDialogGetResult(nullptr) == SAVE_DATA_DIALOG_ERROR_ARG_NULL,
                "get result with null result pointer");
        Require(result.mode == param.mode && result.result == SAVE_DATA_DIALOG_RESULT_OK &&
                result.user_data == &context && std::strcmp(selected.data, "SDU1") == 0,
                "save result must preserve its mode, directory and caller context");
    }

    Require(sceSaveDataDialogOpen(&param) == 0, "open save progress for terminate test");
    RequireRunning();
    Require(sceSaveDataDialogTerminate() == 0, "terminate while running");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_NONE, "terminated state");
    Require(sceSaveDataDialogTerminate() == SAVE_DATA_DIALOG_ERROR_NOT_INITIALIZED, "terminate after terminate");

    Require(sceSaveDataDialogInitialize() == 0, "re-initialize");
    param.mode = 1;
    Require(sceSaveDataDialogOpen(&param) == 0, "open selection dialog");
    Require(sceSaveDataDialogUpdateStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED,
            "selection dialog must retain the existing automatic response");
    Require(sceSaveDataDialogTerminate() == 0, "terminate");
    Require(sceSaveDataDialogTerminate() == SAVE_DATA_DIALOG_ERROR_NOT_INITIALIZED, "terminate when already terminated");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_NONE, "terminated state");

    Require(sceSaveDataDialogInitialize() == 0, "re-initialize for empty selection");
    SaveDataDialogParam emptySelectionParam{};
    emptySelectionParam.size = sizeof(emptySelectionParam);
    emptySelectionParam.mode = 1;
    Require(sceSaveDataDialogOpen(&emptySelectionParam) == 0, "open selection dialog without items");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED, "selection finishes immediately");
    SaveDataDirName emptySelectionDir{};
    std::memset(emptySelectionDir.data, 'X', sizeof(emptySelectionDir.data));
    SaveDataDialogResult emptySelectionResult{};
    emptySelectionResult.dir_name = &emptySelectionDir;
    Require(sceSaveDataDialogGetResult(&emptySelectionResult) == 0, "read empty selection result");
    Require(emptySelectionDir.data[0] == '\0', "empty selection directory name must be null-terminated");
    Require(emptySelectionResult.mode == emptySelectionParam.mode &&
            emptySelectionResult.result == SAVE_DATA_DIALOG_RESULT_OK, "selection result status");
    Require(sceSaveDataDialogTerminate() == 0, "terminate selection");

    Require(sceSaveDataDialogInitialize() == 0, "re-initialize for empty result");
    param = {};
    param.size = sizeof(param);
    param.mode = SAVE_DATA_DIALOG_MODE_PROGRESS_BAR;
    Require(sceSaveDataDialogOpen(&param) == 0, "open progress without items");
    Require(sceSaveDataDialogClose(nullptr) == 0, "close empty progress");
    Require(sceSaveDataDialogUpdateStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED, "finish empty progress");
    SaveDataDirName emptySelected{};
    std::memset(emptySelected.data, 'X', sizeof(emptySelected.data));
    SaveDataDialogResult emptyResult{};
    emptyResult.dir_name = &emptySelected;
    Require(sceSaveDataDialogGetResult(&emptyResult) == 0, "read empty result");
    Require(emptySelected.data[0] == '\0', "empty directory name must be null-terminated");
    Require(sceSaveDataDialogTerminate() == 0, "terminate empty progress");

    Require(sceSaveDataDialogProgressBarInc(0, 10) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress inc after terminate");
    Require(sceSaveDataDialogProgressBarSetValue(0, 50) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress set value after terminate");
}

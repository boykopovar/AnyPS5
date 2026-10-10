#include "prx/libSceSaveDataDialog.native/SaveDataDialog.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

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
    Require(sceSaveDataDialogInitialize() == 0, "initialize");

    SaveDataDirName names[2]{{}, {"SDU1"}};
    SaveDataDialogItems items{};
    items.dir_names = names;
    items.dir_names_num = 2;
    int context = 42;
    SaveDataDialogParam param{};
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
        Require(sceSaveDataDialogProgressBarSetValue(0, 100) == 0, "complete progress");
        RequireRunning();
        Require(sceSaveDataDialogClose(nullptr) == 0, "close save progress");
        Require(sceSaveDataDialogUpdateStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED,
                "closing save progress must finish it");
        SaveDataDirName selected{};
        SaveDataDialogResult result{};
        result.dir_name = &selected;
        Require(sceSaveDataDialogGetResult(&result) == 0, "read completed result");
        Require(result.mode == param.mode && result.result == SAVE_DATA_DIALOG_RESULT_OK &&
                result.user_data == &context && std::strcmp(selected.data, "SDU1") == 0,
                "save result must preserve its mode, directory and caller context");
    }

    param.mode = 1;
    Require(sceSaveDataDialogOpen(&param) == 0, "open selection dialog");
    Require(sceSaveDataDialogUpdateStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED,
            "selection dialog must retain the existing automatic response");
    Require(sceSaveDataDialogTerminate() == 0, "terminate");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_NONE, "terminated state");
}

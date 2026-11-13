#include "prx/libSceSaveDataDialog/include/Api.hpp"
#include "prx/libSceSaveDataDialog.native/include/Api.hpp"
#include "prx/libSceSaveDataDialog.native/SaveDataDialog.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cerrno>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif
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

struct SaveDataDialogParamOlderSdk {
    std::uint8_t base_param[48];
    std::int32_t size;
    std::int32_t mode;
    std::int32_t disp_type;
    std::uint32_t pad0;
    void* anim_param;
    void* items;
    void* user_msg_param;
    void* sys_msg_param;
    void* error_code_param;
    void* prog_bar_param;
    void* user_data;
};
static_assert(sizeof(SaveDataDialogParamOlderSdk) == 120);

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
    Require(sceSaveDataDialogIsReadyToDisplay() == 1, "isReadyToDisplay before init");

    Require(sceSaveDataDialogInitialize() == 0, "initialize");
    Require(sceSaveDataDialogInitialize() == SAVE_DATA_DIALOG_ERROR_ALREADY_INITIALIZED, "double init");
    Require(sceSaveDataDialogOpen(nullptr) == SAVE_DATA_DIALOG_ERROR_ARG_NULL, "open with null param");
    errno = 71;
    Require(sceSaveDataDialogOpen(reinterpret_cast<const void*>(0x10)) == SAVE_DATA_DIALOG_ERROR_ARG_NULL,
            "open with unreadable param");
    Require(errno == 71, "open preserved errno");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_INITIALIZED,
            "unreadable param must leave the dialog initialized");
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
        Require(sceSaveDataDialogProgressBarSetValue(1, 25) == SAVE_DATA_DIALOG_ERROR_PARAM, "set progress invalid target");
        Require(sceSaveDataDialogProgressBarSetValue(0, 101) == SAVE_DATA_DIALOG_ERROR_PARAM, "set progress rate > 100");
        Require(sceSaveDataDialogProgressBarSetValue(0, 25) == 0, "set progress");
        Require(sceSaveDataDialogProgressBarInc(1, 25) == SAVE_DATA_DIALOG_ERROR_PARAM, "inc progress invalid target");
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
        Require(sceSaveDataDialogProgressBarInc(0, 10) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress inc when finished");
        Require(sceSaveDataDialogProgressBarSetValue(0, 50) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress set value when finished");
        SaveDataDirName selected{};
        SaveDataDialogResult result{};
        result.dir_name = &selected;
        Require(sceSaveDataDialogGetResult(&result) == 0, "read completed result");
        Require(sceSaveDataDialogGetResult(nullptr) == SAVE_DATA_DIALOG_ERROR_ARG_NULL,
                "get result with null result pointer");
        Require(sceSaveDataDialogGetResult(reinterpret_cast<SaveDataDialogResult*>(0x10)) == SAVE_DATA_DIALOG_ERROR_ARG_NULL,
                "get result with invalid pointer");
        SaveDataDialogResult invalidDirResult{};
        invalidDirResult.dir_name = reinterpret_cast<void*>(0x10);
        Require(sceSaveDataDialogGetResult(&invalidDirResult) == SAVE_DATA_DIALOG_ERROR_ARG_NULL,
                "get result with invalid directory name pointer");
        SaveDataDialogResult nullDirResult{};
        nullDirResult.dir_name = nullptr;
        Require(sceSaveDataDialogGetResult(&nullDirResult) == 0, "get result with null directory name pointer");
        Require(nullDirResult.mode == param.mode && nullDirResult.result == SAVE_DATA_DIALOG_RESULT_OK &&
                nullDirResult.user_data == &context,
                "save result without directory name must preserve mode and context");
        Require(result.mode == param.mode && result.result == SAVE_DATA_DIALOG_RESULT_OK &&
                result.user_data == &context && std::strcmp(selected.data, "SDU1") == 0,
                "save result must preserve its mode, directory and caller context");
    }

    Require(sceSaveDataDialogOpen(&param) == 0, "open save progress for terminate test");
    RequireRunning();
    Require(sceSaveDataDialogTerminate() == 0, "terminate while running");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_NONE, "terminated state");
    Require(sceSaveDataDialogTerminate() == SAVE_DATA_DIALOG_ERROR_NOT_INITIALIZED, "terminate after terminate");

    Require(sceSaveDataDialogInitialize() == 0, "re-initialize for unreadable items");
    SaveDataDialogParam selectionParam{};
    selectionParam.size = sizeof(selectionParam);
    selectionParam.mode = 1;
    selectionParam.items = &items;
    Require(sceSaveDataDialogOpen(&selectionParam) == 0, "open selection dialog with items");
    SaveDataDialogParam unreadableItemsParam{};
    unreadableItemsParam.size = sizeof(unreadableItemsParam);
    unreadableItemsParam.mode = 0;
    unreadableItemsParam.items = reinterpret_cast<void*>(0x10);
    Require(sceSaveDataDialogOpen(&unreadableItemsParam) == SAVE_DATA_DIALOG_ERROR_ARG_NULL,
            "open with unreadable items");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED,
            "unreadable items must leave the previous result finished");
    SaveDataDirName keptDir{};
    SaveDataDialogResult keptResult{};
    keptResult.dir_name = &keptDir;
    Require(sceSaveDataDialogGetResult(&keptResult) == 0, "read result after unreadable items");
    Require(keptResult.mode == 1 && std::strcmp(keptDir.data, "SDU1") == 0,
            "unreadable items must not change the previous result");
    SaveDataDialogItems unreadableNameItems{};
    unreadableNameItems.dir_names = reinterpret_cast<const SaveDataDirName*>(0x10);
    unreadableNameItems.dir_names_num = 1;
    SaveDataDialogParam unreadableNameParam{};
    unreadableNameParam.size = sizeof(unreadableNameParam);
    unreadableNameParam.mode = 0;
    unreadableNameParam.items = &unreadableNameItems;
    Require(sceSaveDataDialogOpen(&unreadableNameParam) == SAVE_DATA_DIALOG_ERROR_ARG_NULL,
            "open with unreadable directory name entry");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED,
            "unreadable directory name entry must leave the previous result finished");
    SaveDataDirName unterminatedName{};
    std::memset(unterminatedName.data, 'U', sizeof(unterminatedName.data));
    SaveDataDialogItems unterminatedItems{};
    unterminatedItems.dir_names = &unterminatedName;
    unterminatedItems.dir_names_num = 1;
    SaveDataDialogParam unterminatedParam{};
    unterminatedParam.size = sizeof(unterminatedParam);
    unterminatedParam.mode = 0;
    unterminatedParam.items = &unterminatedItems;
    Require(sceSaveDataDialogOpen(&unterminatedParam) == SAVE_DATA_DIALOG_ERROR_ARG_NULL,
            "open with a directory name that has no terminating NUL");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED,
            "unterminated directory name must leave the previous result finished");
    Require(sceSaveDataDialogTerminate() == 0, "terminate after unreadable items");

    Require(sceSaveDataDialogInitialize() == 0, "re-initialize");
    param.mode = 1;
    Require(sceSaveDataDialogOpen(&param) == 0, "open selection dialog");
    Require(sceSaveDataDialogUpdateStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED,
            "selection dialog must retain the existing automatic response");
    Require(sceSaveDataDialogProgressBarInc(0, 10) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress inc on non-progress dialog");
    Require(sceSaveDataDialogProgressBarSetValue(0, 50) == SAVE_DATA_DIALOG_ERROR_NOT_RUNNING, "progress set value on non-progress dialog");
    Require(sceSaveDataDialogTerminate() == 0, "terminate");
    Require(sceSaveDataDialogTerminate() == SAVE_DATA_DIALOG_ERROR_NOT_INITIALIZED, "terminate when already terminated");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_NONE, "terminated state");

    Require(sceSaveDataDialogInitialize() == 0, "re-initialize for older-SDK buffer");
    SaveDataDialogParamOlderSdk olderSdkParam{};
    olderSdkParam.size = sizeof(olderSdkParam);
    olderSdkParam.mode = 1;
    Require(sceSaveDataDialogOpen(&olderSdkParam) == 0, "open selection dialog with older-SDK short buffer");
    Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED, "older-SDK short buffer finishes immediately");
    Require(sceSaveDataDialogTerminate() == 0, "terminate older-SDK buffer dialog");

#ifdef _WIN32
    Require(sceSaveDataDialogInitialize() == 0, "re-initialize for boundary test");
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t pageSize = info.dwPageSize;
    auto* pages = static_cast<char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (pages != nullptr) {
        DWORD oldProtect = 0;
        Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &oldProtect) != 0, "protect guard page");
        auto* shortParam = reinterpret_cast<SaveDataDialogParamOlderSdk*>(pages + pageSize - sizeof(SaveDataDialogParamOlderSdk));
        std::memset(shortParam, 0, sizeof(*shortParam));
        shortParam->size = sizeof(SaveDataDialogParamOlderSdk);
        shortParam->mode = 1;
        Require(sceSaveDataDialogOpen(shortParam) == 0, "open with short older-SDK buffer at page boundary");
        Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED, "short buffer at page boundary finishes");
        Require(sceSaveDataDialogTerminate() == 0, "terminate short buffer at page boundary");
        Require(VirtualFree(pages, 0, MEM_RELEASE) != 0, "free guarded pages");
    }
#else
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize > 0) {
        Require(sceSaveDataDialogInitialize() == 0, "re-initialize for boundary test");
        auto* pages = static_cast<char*>(mmap(nullptr, static_cast<std::size_t>(pageSize) * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (pages != MAP_FAILED) {
            Require(mprotect(pages + pageSize, static_cast<std::size_t>(pageSize), PROT_NONE) == 0, "protect guard page");
            auto* shortParam = reinterpret_cast<SaveDataDialogParamOlderSdk*>(pages + pageSize - sizeof(SaveDataDialogParamOlderSdk));
            std::memset(shortParam, 0, sizeof(*shortParam));
            shortParam->size = sizeof(SaveDataDialogParamOlderSdk);
            shortParam->mode = 1;
            Require(sceSaveDataDialogOpen(shortParam) == 0, "open with short older-SDK buffer at page boundary");
            Require(sceSaveDataDialogGetStatus() == SAVE_DATA_DIALOG_STATUS_FINISHED, "short buffer at page boundary finishes");
            Require(sceSaveDataDialogTerminate() == 0, "terminate short buffer at page boundary");
            Require(munmap(pages, static_cast<std::size_t>(pageSize) * 2) == 0, "unmap guarded pages");
        }
    }
#endif

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
    Require(sceSaveDataDialogIsReadyToDisplay() == 1, "isReadyToDisplay after terminate");
}

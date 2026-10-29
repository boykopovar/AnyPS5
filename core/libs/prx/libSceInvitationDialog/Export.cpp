#include "prx/libSceInvitationDialog/include/Api.hpp"
#include "prx/libSceCommonDialog/include/DialogUse.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED = static_cast<int>(0x80B80001u);
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80008u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;

constexpr std::int32_t INVITATION_DIALOG_MODE_SEND = 1;
constexpr std::int32_t INVITATION_DIALOG_MODE_RECV = 2;

struct DialogState {
    std::mutex mutex;
    int status = COMMON_DIALOG_STATUS_NONE;
    void* callbackArg = nullptr;
};

DialogState& Dialog() {
    static DialogState state;
    return state;
}

[[noreturn]] void Unsettled(const char* function, const std::string& what) {
    throw std::runtime_error(std::string(function) + ": " + what);
}

}

extern "C" {

int APS5_VABI sceInvitationDialogInitialize(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (!CommonDialogSystemInitialized_nid_no_patch()) return COMMON_DIALOG_ERROR_NOT_SYSTEM_INITIALIZED;
    if (dialog.status != COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    if (!CommonDialogAcquire_nid_no_patch()) return COMMON_DIALOG_ERROR_BUSY;
    dialog.status = COMMON_DIALOG_STATUS_INITIALIZED;
    dialog.callbackArg = nullptr;
    return COMMON_DIALOG_OK;
}

int APS5_VABI sceInvitationDialogTerminate(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    dialog.status = COMMON_DIALOG_STATUS_NONE;
    dialog.callbackArg = nullptr;
    CommonDialogRelease_nid_no_patch();
    return COMMON_DIALOG_OK;
}

int APS5_VABI sceInvitationDialogUpdateStatus(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    return dialog.status;
}

int APS5_VABI sceInvitationDialogOpen(const InvitationDialogParam* param) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status != COMMON_DIALOG_STATUS_INITIALIZED && dialog.status != COMMON_DIALOG_STATUS_FINISHED) {
        Unsettled(__func__, "status " + std::to_string(dialog.status) + " is neither INITIALIZED nor FINISHED, and the error for it is unsettled");
    }
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (param->size != sizeof(InvitationDialogParam)) {
        Unsettled(__func__, "param size " + std::to_string(param->size) + " is not the " + std::to_string(sizeof(InvitationDialogParam)) + "-byte layout this library reads");
    }
    if (param->mode != INVITATION_DIALOG_MODE_SEND && param->mode != INVITATION_DIALOG_MODE_RECV) {
        Unsettled(__func__, "mode " + std::to_string(param->mode) + " is neither SEND nor RECV, and the error for it is unsettled");
    }
    dialog.callbackArg = param->callbackArg;
    dialog.status = COMMON_DIALOG_STATUS_FINISHED;
    return COMMON_DIALOG_OK;
}

int APS5_VABI sceInvitationDialogClose(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    Unsettled(__func__, "status " + std::to_string(dialog.status) + " is not RUNNING, and the error for closing a dialog that is not running is unsettled");
}

int APS5_VABI sceInvitationDialogGetResult(InvitationDialogResult* result) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (dialog.status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (std::any_of(std::begin(result->reserved), std::end(result->reserved), [](std::uint8_t value) { return value != 0; })) {
        Unsettled(__func__, "the module checks the reserved bytes 0x18..0x37 of the result, and the error for a non-zero one is unsettled");
    }
    result->callbackArg = dialog.callbackArg;
    result->errorCode = COMMON_DIALOG_OK;
    result->result = COMMON_DIALOG_RESULT_USER_CANCELED;
    if (result->recipients != nullptr) result->recipients->count = 0;
    return COMMON_DIALOG_OK;
}

int APS5_VABI sceInvitationDialogGetStatus_nid_postfix(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    return dialog.status;
}

int APS5_VABI sceInvitationDialogOpenA_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceInvitationDialogGetResultA_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

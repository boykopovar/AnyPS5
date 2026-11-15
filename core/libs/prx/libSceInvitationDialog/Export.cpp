#include "prx/libSceInvitationDialog/include/Api.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libSceCommonDialog/include/DialogUse.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"
#include <cerrno>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>

namespace {

constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80008u);

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

template <typename Param>
int OpenDialog(const char* function, const Param* param) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status != COMMON_DIALOG_STATUS_INITIALIZED && dialog.status != COMMON_DIALOG_STATUS_FINISHED) {
        Unsettled(function, "status " + std::to_string(dialog.status) + " is neither INITIALIZED nor FINISHED, and the error for it is unsettled");
    }
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    Param localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
    errno = savedErrno;
    if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (localParam.size != sizeof(Param)) {
        Unsettled(function, "param size " + std::to_string(localParam.size) + " is not the " + std::to_string(sizeof(Param)) + "-byte layout this library reads");
    }
    if (localParam.mode != INVITATION_DIALOG_MODE_SEND && localParam.mode != INVITATION_DIALOG_MODE_RECV) {
        Unsettled(function, "mode " + std::to_string(localParam.mode) + " is neither SEND nor RECV, and the error for it is unsettled");
    }
    dialog.callbackArg = localParam.callbackArg;
    dialog.status = COMMON_DIALOG_STATUS_FINISHED;
    PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind::PsnFeature);
    return COMMON_DIALOG_OK;
}

template <typename Result, typename WriteRecipients>
int GetDialogResult(const char* function, Result* result, WriteRecipients writeRecipients) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (dialog.status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    Result localResult{};
    const int savedErrnoIn = errno;
    const bool inCopied = GuestCopyIn_nid_no_patch(&localResult, result, sizeof(localResult));
    errno = savedErrnoIn;
    if (!inCopied) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (std::any_of(std::begin(localResult.reserved), std::end(localResult.reserved), [](std::uint8_t value) { return value != 0; })) {
        Unsettled(function, "the error for a non-zero reserved byte at 0x18..0x37 of the result is unsettled");
    }
    localResult.callbackArg = dialog.callbackArg;
    localResult.errorCode = COMMON_DIALOG_OK;
    localResult.result = COMMON_DIALOG_RESULT_USER_CANCELED;
    if (!writeRecipients(localResult)) return COMMON_DIALOG_ERROR_ARG_NULL;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(result, &localResult, sizeof(localResult));
    errno = savedErrno;
    if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
    return COMMON_DIALOG_OK;
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
    return OpenDialog(__func__, param);
}

int APS5_VABI sceInvitationDialogClose(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    if (dialog.status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    Unsettled(__func__, "status " + std::to_string(dialog.status) + " is not RUNNING, and the error for closing a dialog that is not running is unsettled");
}

int APS5_VABI sceInvitationDialogGetResult(InvitationDialogResult* result) {
    return GetDialogResult(__func__, result, [](const InvitationDialogResult& localResult) {
        if (localResult.recipients == nullptr) return true;
        InvitationDialogRecipients recipients{};
        const int savedErrnoIn = errno;
        const bool inCopied = GuestCopyIn_nid_no_patch(&recipients, localResult.recipients, sizeof(recipients));
        errno = savedErrnoIn;
        if (!inCopied) return false;
        recipients.count = 0;
        const int savedErrnoOut = errno;
        const bool outCopied = GuestCopyOut_nid_no_patch(localResult.recipients, &recipients, sizeof(recipients));
        errno = savedErrnoOut;
        if (!outCopied) return false;
        return true;
    });
}

int APS5_VABI sceInvitationDialogGetStatus_nid_postfix(void) {
    DialogState& dialog = Dialog();
    std::lock_guard lock(dialog.mutex);
    return dialog.status;
}

int APS5_VABI sceInvitationDialogOpenA_nid_postfix(const InvitationDialogParamA* param) {
    return OpenDialog(__func__, param);
}

int APS5_VABI sceInvitationDialogGetResultA_nid_postfix(InvitationDialogResultA* result) {
    return GetDialogResult(__func__, result, [](const InvitationDialogResultA& localResult) {
        if (localResult.sentUsers == nullptr) return true;
        const std::uint32_t count = 0;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(localResult.sentUsers, &count, sizeof(count));
        errno = savedErrno;
        return copied;
    });
}

}

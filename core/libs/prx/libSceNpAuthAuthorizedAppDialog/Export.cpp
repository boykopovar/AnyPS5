#include "prx/libSceNpAuthAuthorizedAppDialog/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"

namespace {

constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceNpAuthAuthorizedAppDialogInitialize(void) {
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpAuthAuthorizedAppDialogOpen(const NpAuthAuthorizedAppDialogParam* param) {
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    NpAuthAuthorizedAppDialogParam localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
    errno = savedErrno;
    if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (localParam.base.size != sizeof(localParam.base) || localParam.base.magic != static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(param)) - 1060003575u || !std::all_of(std::begin(localParam.base.reserved), std::end(localParam.base.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    if (localParam.size != sizeof(localParam) || localParam.userId == -1) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    UserServiceLoginUserIdList users{};
    if (sceUserServiceGetLoginUserIdList(&users) != 0 || std::find(std::begin(users.user_id), std::end(users.user_id), localParam.userId) == std::end(users.user_id)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    char firstAppChar = 0;
    const int savedErrnoApp = errno;
    const bool appCopied = localParam.application != nullptr && GuestCopyIn_nid_no_patch(&firstAppChar, localParam.application, sizeof(firstAppChar));
    errno = savedErrnoApp;
    if (!appCopied || firstAppChar == 0) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    if (localParam.text == nullptr) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    char textBuffer[1025]{};
    std::size_t textLen = 0;
    bool textNullTerminated = false;
    for (std::size_t i = 0; i <= 1024; ++i) {
        char ch = 0;
        const int savedErrnoText = errno;
        const bool copiedCh = GuestCopyIn_nid_no_patch(&ch, localParam.text + i, sizeof(ch));
        errno = savedErrnoText;
        if (!copiedCh) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
        if (ch == 0) {
            textNullTerminated = true;
            textLen = i;
            break;
        }
        textBuffer[i] = ch;
    }
    if (!textNullTerminated || textLen == 0 || localParam.mode > 1 || !std::all_of(std::begin(localParam.reserved), std::end(localParam.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind::PsnFeature);
    return 0;
}

int APS5_VABI sceNpAuthAuthorizedAppDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI sceNpAuthAuthorizedAppDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceNpAuthAuthorizedAppDialogGetResult(std::int32_t* result) {
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    const std::int32_t canceled = COMMON_DIALOG_RESULT_USER_CANCELED;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(result, &canceled, sizeof(canceled));
    errno = savedErrno;
    if (!copied) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    return COMMON_DIALOG_RESULT_USER_CANCELED;
}

int APS5_VABI sceNpAuthAuthorizedAppDialogClose(void) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (g_status.load() != COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_NOT_RUNNING;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI sceNpAuthAuthorizedAppDialogTerminate(void) {
    if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return 0;
}

}

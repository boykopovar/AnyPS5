#include "prx/libSceNpSnsFacebookDialog/include/Api.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceIme/include/PsnDialogNotice.hpp"

namespace {

constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceNpSnsFacebookDialogInitialize(void) {
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpSnsFacebookDialogOpen(const void* param) {
    int status = g_status.load();
    do {
        if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
        if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
        if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    } while (!g_status.compare_exchange_weak(status, COMMON_DIALOG_STATUS_FINISHED));
    PsnDialogNoticeOffer_nid_postfix(PsnDialogNoticeKind::PsnFeature);
    return 0;
}

int APS5_VABI sceNpSnsFacebookDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI sceNpSnsFacebookDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceNpSnsFacebookDialogGetResult(void* result) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    const std::int32_t canceled = COMMON_DIALOG_RESULT_USER_CANCELED;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(result, &canceled, sizeof(canceled));
    errno = savedErrno;
    if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
    return 0;
}

int APS5_VABI sceNpSnsFacebookDialogClose(void) {
    int status = g_status.load();
    do {
        if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    } while (!g_status.compare_exchange_weak(status, COMMON_DIALOG_STATUS_FINISHED));
    return 0;
}

int APS5_VABI sceNpSnsFacebookDialogTerminate(void) {
    if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return 0;
}

}

#include "prx/libSceGameCustomDataDialog/include/Api.hpp"
#include <cerrno>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"

namespace {

constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceGameCustomDataDialogInitialize(void) {
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceGameCustomDataDialogOpenA(const void* param) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI sceGameCustomDataDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI sceGameCustomDataDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceGameCustomDataDialogGetResultA(void* result) {
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

int APS5_VABI sceGameCustomDataDialogClose(void) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI sceGameCustomDataDialogTerminate(void) {
    if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI sceGameCustomDataDialogGetResult() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceGameCustomDataDialogOpen() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

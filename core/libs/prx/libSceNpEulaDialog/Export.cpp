#include "prx/libSceNpEulaDialog/include/Api.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int CHECK_VERSION_ERROR_BUSY = static_cast<int>(0x80B80006u);
constexpr int CHECK_VERSION_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8055A800u);

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceNpEulaDialogCheckVersion(std::int32_t userId, std::int32_t value, void* callback, void* userdata) {
    (void)userId;
    (void)userdata;
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (value <= 0 || callback == nullptr) return CHECK_VERSION_ERROR_INVALID_ARGUMENT;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEulaDialogCheckVersionAbort(void) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return CHECK_VERSION_ERROR_BUSY;
}

int APS5_VABI sceNpEulaDialogInitialize(void) {
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceNpEulaDialogOpen(const void* param) {
    int status = g_status.load();
    do {
        if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
        if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_INVALID_STATE;
        if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    } while (!g_status.compare_exchange_weak(status, COMMON_DIALOG_STATUS_FINISHED));
    return 0;
}

int APS5_VABI sceNpEulaDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI sceNpEulaDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceNpEulaDialogGetResult(void* result) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    const std::int32_t canceled = COMMON_DIALOG_RESULT_USER_CANCELED;
    const bool copied = GuestCopyOut_nid_no_patch(result, &canceled, sizeof(canceled));
    if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
    return 0;
}

int APS5_VABI sceNpEulaDialogClose(void) {
    int status = g_status.load();
    do {
        if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    } while (!g_status.compare_exchange_weak(status, COMMON_DIALOG_STATUS_FINISHED));
    return 0;
}

int APS5_VABI sceNpEulaDialogTerminate(void) {
    if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return 0;
}

}

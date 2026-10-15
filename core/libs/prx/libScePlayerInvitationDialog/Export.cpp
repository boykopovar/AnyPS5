#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80B80004u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI scePlayerInvitationDialog(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayerInvitationDialogInitialize(void) {
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI scePlayerInvitationDialogOpen(const void* param) {
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI scePlayerInvitationDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI scePlayerInvitationDialogGetResult(void* result) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    const std::int32_t canceled = COMMON_DIALOG_RESULT_USER_CANCELED;
    std::memcpy(result, &canceled, sizeof(canceled));
    return 0;
}

int APS5_VABI scePlayerInvitationDialogClose(void) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI scePlayerInvitationDialogTerminate(void) {
    g_status = COMMON_DIALOG_STATUS_NONE;
    return 0;
}

int APS5_VABI scePlayerInvitationDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI scePlayerInvitationDialogParamInit(void* param) {
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    std::memset(param, 0, 0x80);
    return 0;
}

int APS5_VABI scePlayerInvitationDialogParamInitialize(void* param) {
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    std::memset(param, 0, 0x80);
    return 0;
}

}

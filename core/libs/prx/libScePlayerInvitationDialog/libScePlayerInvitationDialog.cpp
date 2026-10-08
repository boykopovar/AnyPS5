#include "prx/libScePlayerInvitationDialog/libScePlayerInvitationDialog.h"

#include <mutex>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/LogMacros.hpp"

namespace {

constexpr std::int32_t COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr auto COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<std::int32_t>(0x80B80003u);
constexpr auto COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<std::int32_t>(0x80B80005u);
constexpr auto COMMON_DIALOG_ERROR_ARG_NULL = static_cast<std::int32_t>(0x80B8000Du);

std::mutex g_dialog_mutex;
PlayerInvitationDialogStatus g_dialog_status = PlayerInvitationDialogStatus::None;

std::int32_t status() {
    return static_cast<std::int32_t>(g_dialog_status);
}

}

extern "C" {

std::int32_t APS5_VABI scePlayerInvitationDialogInitialize(void) {
    std::lock_guard lock(g_dialog_mutex);
    APS5_LOG_OUT("status=%d", status());
    if (g_dialog_status == PlayerInvitationDialogStatus::None) {
        g_dialog_status = PlayerInvitationDialogStatus::Initialized;
    }
    return 0;
}

std::int32_t APS5_VABI scePlayerInvitationDialogOpen(const ScePlayerInvitationDialogParam* param) {
    std::lock_guard lock(g_dialog_mutex);
    APS5_LOG_OUT("param=%p status=%d", static_cast<const void*>(param), status());
    if (g_dialog_status == PlayerInvitationDialogStatus::None) {
        g_dialog_status = PlayerInvitationDialogStatus::Initialized;
    }
    g_dialog_status = PlayerInvitationDialogStatus::Running;
    return 0;
}

std::int32_t APS5_VABI scePlayerInvitationDialogUpdateStatus(void) {
    std::lock_guard lock(g_dialog_mutex);
    APS5_LOG_OUT("status=%d", status());
    if (g_dialog_status == PlayerInvitationDialogStatus::Running) {
        g_dialog_status = PlayerInvitationDialogStatus::Finished;
    }
    return status();
}

std::int32_t APS5_VABI scePlayerInvitationDialogGetStatus(void) {
    std::lock_guard lock(g_dialog_mutex);
    APS5_LOG_OUT("status=%d", status());
    return status();
}

std::int32_t APS5_VABI scePlayerInvitationDialogClose(void) {
    std::lock_guard lock(g_dialog_mutex);
    APS5_LOG_OUT("status=%d", status());
    if (g_dialog_status == PlayerInvitationDialogStatus::Running) {
        g_dialog_status = PlayerInvitationDialogStatus::Finished;
    }
    return 0;
}

std::int32_t APS5_VABI scePlayerInvitationDialogTerminate(void) {
    std::lock_guard lock(g_dialog_mutex);
    APS5_LOG_OUT("status=%d", status());
    g_dialog_status = PlayerInvitationDialogStatus::None;
    return 0;
}

std::int32_t APS5_VABI scePlayerInvitationDialogGetResult(ScePlayerInvitationDialogResult* result) {
    std::lock_guard lock(g_dialog_mutex);
    APS5_LOG_OUT("result=%p status=%d", static_cast<void*>(result), status());
    if (g_dialog_status == PlayerInvitationDialogStatus::None) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (g_dialog_status != PlayerInvitationDialogStatus::Finished) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    *result = ScePlayerInvitationDialogResult{};
    result->result = COMMON_DIALOG_RESULT_USER_CANCELED;
    return 0;
}

}

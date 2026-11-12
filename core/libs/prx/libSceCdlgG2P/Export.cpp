#include "prx/libSceCdlgG2P/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceG2PDialogInitialize(void) {
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI sceG2PDialogOpen(const G2PDialogParam* param) {
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    G2PDialogParam localParam{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
    errno = savedErrno;
    if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
    if (localParam.base.size != sizeof(localParam.base) || localParam.base.magic != static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(param)) - 1060003575u || !std::all_of(std::begin(localParam.base.reserved), std::end(localParam.base.reserved), [](std::uint8_t value) { return value == 0; }) || localParam.size != sizeof(localParam) || localParam.userId == -1 || !std::all_of(std::begin(localParam.reserved), std::end(localParam.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    UserServiceLoginUserIdList users{};
    if (sceUserServiceGetLoginUserIdList(&users) != 0 || std::none_of(std::begin(users.user_id), std::end(users.user_id), [&localParam](std::int32_t userId) { return userId != -1 && userId == localParam.userId; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI sceG2PDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI sceG2PDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceG2PDialogGetResult(CommonDialogResultPrefix* result) {
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    const std::int32_t cancelResult = COMMON_DIALOG_RESULT_USER_CANCELED;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(&result->result, &cancelResult, sizeof(cancelResult));
    errno = savedErrno;
    if (!copied) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    return 0;
}

int APS5_VABI sceG2PDialogClose(void) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_NOT_RUNNING;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI sceG2PDialogTerminate(void) {
    if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return 0;
}

}

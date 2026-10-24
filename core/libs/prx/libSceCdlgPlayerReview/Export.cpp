#include "prx/libSceCdlgPlayerReview/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"

namespace {
std::atomic<int> g_status{0};

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_RUNNING = 2;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80B8000Au);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);
constexpr int COMMON_DIALOG_ERROR_NOT_RUNNING = static_cast<int>(0x80B8000Bu);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
}

extern "C" {

int APS5_VABI scePlayerReviewDialogInitialize(void) {
    int expected = 0;
    if (!g_status.compare_exchange_strong(expected, 1)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    return 0;
}

int APS5_VABI scePlayerReviewDialogTerminate(void) {
    int expected = 1;
    if (g_status.compare_exchange_strong(expected, 0)) return 0;
    expected = COMMON_DIALOG_STATUS_FINISHED;
    if (g_status.compare_exchange_strong(expected, 0)) return 0;
    if (g_status.load() == 0) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return COMMON_DIALOG_ERROR_NOT_RUNNING;
}

int APS5_VABI scePlayerReviewDialogOpen(const PlayerReviewDialogParam* param) {
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
    if (!ValidateCommonDialogBaseParam(param->base) || param->size != sizeof(*param) || param->userId == -1 || !std::all_of(std::begin(param->reserved), std::end(param->reserved), [](std::uint8_t value) { return value == 0; }) || param->mode > 1) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    UserServiceLoginUserIdList users{};
    if (sceUserServiceGetLoginUserIdList(&users) != 0 || std::none_of(std::begin(users.user_id), std::end(users.user_id), [param](std::int32_t userId) { return userId != -1 && userId == param->userId; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI scePlayerReviewDialogClose(void) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_NOT_RUNNING;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI scePlayerReviewDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI scePlayerReviewDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI scePlayerReviewDialogGetResult(CommonDialogResultPrefix* result) {
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    result->result = COMMON_DIALOG_RESULT_USER_CANCELED;
    return COMMON_DIALOG_RESULT_USER_CANCELED;
}

}

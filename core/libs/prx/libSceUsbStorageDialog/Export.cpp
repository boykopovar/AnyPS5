#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

extern "C" int APS5_VABI sceUserServiceGetLoginUserIdList(UserServiceLoginUserIdList* users);

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_RUNNING = 2;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80B8000Au);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);
constexpr int COMMON_DIALOG_ERROR_NOT_RUNNING = static_cast<int>(0x80B8000Bu);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};
std::uint32_t g_sdkVersion = 0;
std::atomic_bool g_closed{false};

}

extern "C" {

int APS5_VABI sceUsbStorageDialogInitialize(void) {
    if (g_status.load() != COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    const auto* process = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    std::uint32_t sdkVersion;
    std::memcpy(&sdkVersion, process + 16, sizeof(sdkVersion));
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    g_sdkVersion = sdkVersion;
    g_closed = false;
    return 0;
}

int APS5_VABI sceUsbStorageDialogOpen(const UsbStorageDialogParam* param) {
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (!ValidateCommonDialogBaseParam(param->base)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    UserServiceLoginUserIdList users{};
    if (param->userId == -1 || sceUserServiceGetLoginUserIdList(&users) != 0 || std::find(std::begin(users.user_id), std::end(users.user_id), param->userId) == std::end(users.user_id)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    if (param->path[1023] != 0 || param->path[0] == 0) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    if (g_sdkVersion >= 0x04500000 && (std::strpbrk(param->path, ",*?\"<>|") != nullptr || std::strstr(param->path, "..") != nullptr)) return static_cast<int>(0x812D0003u);
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
    g_closed = false;
    g_status = COMMON_DIALOG_STATUS_RUNNING;
    return 0;
}

int APS5_VABI sceUsbStorageDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI sceUsbStorageDialogUpdateStatus(void) {
    int expected = COMMON_DIALOG_STATUS_RUNNING;
    g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_FINISHED);
    return g_status.load();
}

int APS5_VABI sceUsbStorageDialogGetResult(UsbStorageDialogResult* result) {
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (g_closed.load()) {
        result->result = COMMON_DIALOG_ERROR_NOT_FINISHED;
        result->status = 0;
        return 0;
    }
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    if (g_sdkVersion >= 0x04500000 && (result->size != sizeof(*result) || result->result != COMMON_DIALOG_ERROR_BUSY)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    result->result = COMMON_DIALOG_RESULT_USER_CANCELED;
    return COMMON_DIALOG_RESULT_USER_CANCELED;
}

int APS5_VABI sceUsbStorageDialogClose(void) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status != COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_NOT_RUNNING;
    g_closed = true;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI sceUsbStorageDialogTerminate(void) {
    if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return 0;
}

}

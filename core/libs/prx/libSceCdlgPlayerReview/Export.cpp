#include <atomic>
#include <cstdint>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
std::atomic<int> g_status{0};

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_RUNNING = 2;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
}

extern "C" {

int APS5_VABI scePlayerReviewDialogInitialize(void) {
    int expected = 0;
    if (!g_status.compare_exchange_strong(expected, 1)) throw std::logic_error("scePlayerReviewDialogInitialize: already initialized");
    return 0;
}

int APS5_VABI scePlayerReviewDialogTerminate(void) {
    int expected = 1;
    if (g_status.compare_exchange_strong(expected, 0)) return 0;
    expected = COMMON_DIALOG_STATUS_FINISHED;
    if (!g_status.compare_exchange_strong(expected, 0)) throw std::logic_error("scePlayerReviewDialogTerminate: not initialized or still running");
    return 0;
}

int APS5_VABI scePlayerReviewDialogOpen(const void* param) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
    if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    g_status = COMMON_DIALOG_STATUS_FINISHED;
    return 0;
}

int APS5_VABI scePlayerReviewDialogClose(void) {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    return 0;
}

int APS5_VABI scePlayerReviewDialogGetStatus(void) {
    return g_status.load();
}

int APS5_VABI scePlayerReviewDialogUpdateStatus(void) {
    return g_status.load();
}

int APS5_VABI scePlayerReviewDialogGetResult(void* result) {
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
    *static_cast<std::int32_t*>(result) = COMMON_DIALOG_RESULT_USER_CANCELED;
    return 0;
}

}

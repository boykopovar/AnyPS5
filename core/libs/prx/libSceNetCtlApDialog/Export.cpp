#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;
constexpr int COMMON_DIALOG_STATUS_RUNNING = 2;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80B8000Au);
constexpr int COMMON_DIALOG_ERROR_NOT_RUNNING = static_cast<int>(0x80B8000Bu);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceNetCtlApDialogInitialize(void) {
 int expected = COMMON_DIALOG_STATUS_NONE;
 if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
 return 0;
}

int APS5_VABI sceNetCtlApDialogTerminate(void) {
 if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 return 0;
}

int APS5_VABI sceNetCtlApDialogOpen(const NetCtlApDialogParam* param) {
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (!ValidateCommonDialogBaseParam(param->base)) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (param->size != sizeof(*param) || param->reserved60 != 0 || std::find(std::begin(param->option), std::end(param->option), '\0') == std::end(param->option) || !std::all_of(std::begin(param->reserved), std::end(param->reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_BUSY;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceNetCtlApDialogClose(void) {
 if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (g_status.load() != COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_NOT_RUNNING;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceNetCtlApDialogUpdateStatus(void) {
 return g_status.load();
}

int APS5_VABI sceNetCtlApDialogGetStatus(void) {
 return g_status.load();
}

int APS5_VABI sceNetCtlApDialogGetResult(NetCtlApDialogResult* result) {
 if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (!std::all_of(std::begin(result->reserved), std::end(result->reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
 result->result = COMMON_DIALOG_RESULT_USER_CANCELED;
 return COMMON_DIALOG_RESULT_USER_CANCELED;
}

}

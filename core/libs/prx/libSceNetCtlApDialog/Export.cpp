#include "prx/libSceNetCtlApDialog/include/Api.hpp"
#include <cerrno>
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int COMMON_DIALOG_ERROR_BUSY = static_cast<int>(0x80B80007u);

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
 NetCtlApDialogParam localParam{};
 const int savedErrno = errno;
 const bool copied = GuestCopyIn_nid_no_patch(&localParam, param, sizeof(localParam));
 errno = savedErrno;
 if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (localParam.base.size != sizeof(localParam.base) || localParam.base.magic != static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(param)) - 1060003575u || !std::all_of(std::begin(localParam.base.reserved), std::end(localParam.base.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (localParam.size != sizeof(localParam) || localParam.reserved60 != 0 || std::find(std::begin(localParam.option), std::end(localParam.option), '\0') == std::end(localParam.option) || !std::all_of(std::begin(localParam.reserved), std::end(localParam.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
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
 NetCtlApDialogResult inResult{};
 const int savedErrno = errno;
 const bool read = GuestCopyIn_nid_no_patch(&inResult, result, sizeof(inResult));
 errno = savedErrno;
 if (!read) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (!std::all_of(std::begin(inResult.reserved), std::end(inResult.reserved), [](std::uint8_t value) { return value == 0; })) return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
 if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
 const std::int32_t cancelResult = COMMON_DIALOG_RESULT_USER_CANCELED;
 const bool copied = GuestCopyOut_nid_no_patch(&result->result, &cancelResult, sizeof(cancelResult));
 errno = savedErrno;
 if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
 return COMMON_DIALOG_RESULT_USER_CANCELED;
}

}

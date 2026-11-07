#include "prx/libSceLoginDialog/include/Api.hpp"
#include <atomic>
#include <cerrno>
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
constexpr int LOGIN_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x81340001u);
constexpr int LOGIN_DIALOG_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x81340002u);
constexpr int LOGIN_DIALOG_ERROR_BUSY = static_cast<int>(0x81340005u);
constexpr int LOGIN_DIALOG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x81340003u);
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;

constexpr int LOGIN_DIALOG_ERROR_INVALID_USER = static_cast<int>(0x81340007u);

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceLoginDialogInitialize(void) {
 int expected = COMMON_DIALOG_STATUS_NONE;
 if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return LOGIN_DIALOG_ERROR_ALREADY_INITIALIZED;
 return 0;
}

int APS5_VABI sceLoginDialogOpen(const LoginDialogParam* param) {
 if (param == nullptr || param->size != sizeof(*param) || param->mode > 1 || !std::all_of(std::begin(param->reserved), std::end(param->reserved), [](std::uint32_t value) { return value == 0; })) return LOGIN_DIALOG_ERROR_INVALID_ARGUMENT;
 for (const auto userId : param->userIds) {
  if (userId != -1 && (static_cast<std::uint32_t>(userId) < 0x10000000u || static_cast<std::uint32_t>(userId) >= 0x40000000u)) return LOGIN_DIALOG_ERROR_INVALID_USER;
 }
 int status = g_status.load();
 do {
  if (status == COMMON_DIALOG_STATUS_NONE) return LOGIN_DIALOG_ERROR_NOT_INITIALIZED;
  if (status == COMMON_DIALOG_STATUS_RUNNING) return LOGIN_DIALOG_ERROR_BUSY;
 } while (!g_status.compare_exchange_weak(status, COMMON_DIALOG_STATUS_FINISHED));
 return 0;
}

int APS5_VABI sceLoginDialogUpdateStatus(void) {
 return g_status.load();
}

int APS5_VABI sceLoginDialogGetStatus(void) {
 return g_status.load();
}

int APS5_VABI sceLoginDialogGetResult(LoginDialogResult* result) {
 if (result == nullptr) return LOGIN_DIALOG_ERROR_INVALID_ARGUMENT;
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return LOGIN_DIALOG_ERROR_NOT_INITIALIZED;
 const std::int32_t fields[2] = {COMMON_DIALOG_RESULT_USER_CANCELED, LOGIN_DIALOG_ERROR_INVALID_USER};
 const int savedErrno = errno;
 const bool copied = GuestCopyOut_nid_no_patch(result, fields, sizeof(fields));
 errno = savedErrno;
 if (!copied) return LOGIN_DIALOG_ERROR_INVALID_ARGUMENT;
 if (status != COMMON_DIALOG_STATUS_FINISHED) return LOGIN_DIALOG_ERROR_BUSY;
 return 0;
}

int APS5_VABI sceLoginDialogClose(void) {
 int status = g_status.load();
 do {
  if (status == COMMON_DIALOG_STATUS_NONE) return LOGIN_DIALOG_ERROR_NOT_INITIALIZED;
 } while (!g_status.compare_exchange_weak(status, COMMON_DIALOG_STATUS_FINISHED));
 return 0;
}

int APS5_VABI sceLoginDialogTerminate(void) {
 if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return LOGIN_DIALOG_ERROR_NOT_INITIALIZED;
 return 0;
}

}

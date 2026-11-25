#include "prx/libSceSigninDialog/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"

namespace {

struct SceSigninDialogResult {
 std::int32_t result = COMMON_DIALOG_RESULT_USER_CANCELED;
 std::int32_t reserved[3] = {};
};

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceSigninDialogClose(void) {
 if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceSigninDialogGetResult(void* result) {
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
 if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 const SceSigninDialogResult dialogResult{};
 const bool copied = GuestCopyOut_nid_no_patch(result, &dialogResult, sizeof(dialogResult));
 if (!copied) return COMMON_DIALOG_ERROR_ARG_NULL;
 return 0;
}

int APS5_VABI sceSigninDialogGetStatus(void) {
 return g_status.load();
}

int APS5_VABI sceSigninDialogInitialize(void) {
 int expected = COMMON_DIALOG_STATUS_NONE;
 if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
 return 0;
}

int APS5_VABI sceSigninDialogOpen(const void* param) {
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (status == COMMON_DIALOG_STATUS_RUNNING) return COMMON_DIALOG_ERROR_RESULT_NONE;
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 std::uint8_t dummy = 0;
 const bool readable = GuestCopyIn_nid_no_patch(&dummy, param, sizeof(dummy));
 if (!readable) return COMMON_DIALOG_ERROR_ARG_NULL;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceSigninDialogTerminate(void) {
 if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 return 0;
}

int APS5_VABI sceSigninDialogUpdateStatus(void) {
 return g_status.load();
}

}

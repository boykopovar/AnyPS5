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
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80006u);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B80009u);

int g_status = COMMON_DIALOG_STATUS_NONE;

}

extern "C" {

// The store dialog has nothing to show: every open finishes at once as cancelled by the user.
int APS5_VABI sceNpCommerceDialogInitialize() {
 if (g_status != COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
 g_status = COMMON_DIALOG_STATUS_INITIALIZED;
 return 0;
}

int APS5_VABI sceNpCommerceDialogOpen(const void* param) {
 if (g_status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceNpCommerceDialogUpdateStatus(void) {
 return g_status;
}

int APS5_VABI sceNpCommerceDialogGetStatus(void) {
 return g_status;
}

int APS5_VABI sceNpCommerceDialogGetResult(void* result) {
 if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (g_status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
 // SceNpCommerceDialogResult: int32 result, bool authorized, then reserved bytes.
 std::int32_t value = COMMON_DIALOG_RESULT_USER_CANCELED;
 std::memcpy(result, &value, sizeof(value));
 std::uint8_t authorized = 0;
 std::memcpy(static_cast<std::uint8_t*>(result) + sizeof(value), &authorized, sizeof(authorized));
 return 0;
}

int APS5_VABI sceNpCommerceDialogClose(void) {
 if (g_status == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceNpCommerceDialogTerminate() {
 g_status = COMMON_DIALOG_STATUS_NONE;
 return 0;
}

}

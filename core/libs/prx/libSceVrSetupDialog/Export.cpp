#include "prx/libSceVrSetupDialog/include/Api.hpp"
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
constexpr int VR_SETUP_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x8A7C0001u);
constexpr int VR_SETUP_DIALOG_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8A7C0002u);
constexpr int VR_SETUP_DIALOG_ERROR_INVALID_ARGUMENT = static_cast<int>(0x8A7C0003u);
constexpr int VR_SETUP_DIALOG_ERROR_BUSY = static_cast<int>(0x8A7C0005u);
constexpr int COMMON_DIALOG_RESULT_USER_CANCELED = 1;

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceVrSetupDialogInitialize(void) {
 int expected = COMMON_DIALOG_STATUS_NONE;
 if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) return VR_SETUP_DIALOG_ERROR_ALREADY_INITIALIZED;
 return 0;
}

int APS5_VABI sceVrSetupDialogOpen(const VrSetupDialogParam* param) {
 if (param == nullptr || param->size != sizeof(*param) || param->reserved8 != 0 || param->reserved9 != 0) return VR_SETUP_DIALOG_ERROR_INVALID_ARGUMENT;
 const int status = g_status.load();
 if (status == COMMON_DIALOG_STATUS_NONE) return VR_SETUP_DIALOG_ERROR_NOT_INITIALIZED;
 if (status == COMMON_DIALOG_STATUS_RUNNING) return VR_SETUP_DIALOG_ERROR_BUSY;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceVrSetupDialogUpdateStatus(void) {
 return g_status.load();
}

int APS5_VABI sceVrSetupDialogGetStatus(void) {
 return g_status.load();
}

int APS5_VABI sceVrSetupDialogGetResult(std::int32_t* result) {
    if (result == nullptr) return VR_SETUP_DIALOG_ERROR_INVALID_ARGUMENT;
    const int status = g_status.load();
    if (status == COMMON_DIALOG_STATUS_NONE) return VR_SETUP_DIALOG_ERROR_NOT_INITIALIZED;
    *result = COMMON_DIALOG_RESULT_USER_CANCELED;
    if (status != COMMON_DIALOG_STATUS_FINISHED) return VR_SETUP_DIALOG_ERROR_BUSY;
    return 0;
}

int APS5_VABI sceVrSetupDialogClose(void) {
 if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return VR_SETUP_DIALOG_ERROR_NOT_INITIALIZED;
 if (g_status.load() == COMMON_DIALOG_STATUS_RUNNING) g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceVrSetupDialogTerminate(void) {
 if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) return VR_SETUP_DIALOG_ERROR_NOT_INITIALIZED;
 return 0;
}

}

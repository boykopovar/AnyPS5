#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {
std::atomic<int> g_status{0};

constexpr int COMMON_DIALOG_STATUS_RUNNING = 2;
constexpr int COMMON_DIALOG_STATUS_FINISHED = 3;
constexpr int COMMON_DIALOG_RESULT_OK = 0;
constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_NOT_FINISHED = static_cast<int>(0x80B80005u);
constexpr int COMMON_DIALOG_ERROR_NOT_RUNNING = static_cast<int>(0x80B8000Bu);
constexpr int COMMON_DIALOG_ERROR_ARG_NULL = static_cast<int>(0x80B8000Du);
constexpr int BUTTON_ID_OK = 1;
}

extern "C" {

int SceMsgDialogNativeModuleLoaded_nid_no_patch = 1;

int APS5_VABI sceMsgDialogClose(void) {
 int expected = COMMON_DIALOG_STATUS_RUNNING;
 if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_FINISHED)) return COMMON_DIALOG_ERROR_NOT_RUNNING;
 return 0;
}

int APS5_VABI sceMsgDialogGetResult(MsgDialogResult* result) {
 const int status = g_status.load();
 if (status == 0) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (result == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 if (status != COMMON_DIALOG_STATUS_FINISHED) return COMMON_DIALOG_ERROR_NOT_FINISHED;
 *result = MsgDialogResult{};
 result->result = COMMON_DIALOG_RESULT_OK;
 result->button_id = BUTTON_ID_OK;
 return 0;
}

int APS5_VABI sceMsgDialogInitialize(void) {
    int expected = 0;
    if (!g_status.compare_exchange_strong(expected, 1)) throw std::logic_error("sceMsgDialogInitialize: already initialized");
    return 0;
}

int APS5_VABI sceMsgDialogOpen(const void* param) {
 const int status = g_status.load();
 if (status == 0) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
 if (param == nullptr) return COMMON_DIALOG_ERROR_ARG_NULL;
 g_status = COMMON_DIALOG_STATUS_FINISHED;
 return 0;
}

int APS5_VABI sceMsgDialogTerminate(void) {
    int expected = 1;
    if (g_status.compare_exchange_strong(expected, 0)) return 0;
    expected = COMMON_DIALOG_STATUS_FINISHED;
    if (!g_status.compare_exchange_strong(expected, 0)) throw std::logic_error("sceMsgDialogTerminate: not initialized or still running");
    return 0;
}

int APS5_VABI sceMsgDialogUpdateStatus(void) {
    return g_status.load();
}

APS5_EXPORT("CWVW78Qc3fI", sceMsgDialogUnknown00);
int APS5_VABI sceMsgDialogUnknown00(void) {
    NotImplemented_nid_no_patch("CWVW78Qc3fI");
    return 0;
}

}

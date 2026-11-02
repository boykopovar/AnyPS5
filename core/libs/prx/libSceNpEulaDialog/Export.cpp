#include "prx/libSceNpEulaDialog/include/Api.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int COMMON_DIALOG_STATUS_NONE = 0;
constexpr int COMMON_DIALOG_STATUS_INITIALIZED = 1;

constexpr int COMMON_DIALOG_ERROR_NOT_INITIALIZED = static_cast<int>(0x80B80003u);
constexpr int COMMON_DIALOG_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80B80004u);

std::atomic<int> g_status{COMMON_DIALOG_STATUS_NONE};

}

extern "C" {

int APS5_VABI sceNpEulaDialogCheckVersion() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEulaDialogCheckVersionAbort() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEulaDialogClose() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEulaDialogGetResult() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEulaDialogGetStatus() {
    return g_status.load();
}

int APS5_VABI sceNpEulaDialogInitialize() {
    int expected = COMMON_DIALOG_STATUS_NONE;
    if (!g_status.compare_exchange_strong(expected, COMMON_DIALOG_STATUS_INITIALIZED)) {
        return COMMON_DIALOG_ERROR_ALREADY_INITIALIZED;
    }
    return 0;
}

int APS5_VABI sceNpEulaDialogOpen() {
    if (g_status.load() == COMMON_DIALOG_STATUS_NONE) return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpEulaDialogTerminate() {
    if (g_status.exchange(COMMON_DIALOG_STATUS_NONE) == COMMON_DIALOG_STATUS_NONE) {
        return COMMON_DIALOG_ERROR_NOT_INITIALIZED;
    }
    return 0;
}

int APS5_VABI sceNpEulaDialogUpdateStatus() {
    return g_status.load();
}

}

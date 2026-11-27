#include "prx/libScePatchCheckerClient/include/Api.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_PATCH_CHECKER_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80182401);
constexpr int SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED = static_cast<int>(0x80182402);

std::atomic<bool> g_initialized{false};

}

extern "C" {

int APS5_VABI scePatchCheckerCancel() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerCheckPatch() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerCreateHandler() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerDestroyHandler() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerDisableAutoDownload() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerEnableAutoDownload() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerGetApplicableTick() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerGetPackageInfo() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerInitialize() {
    bool expected = false;
    if (!g_initialized.compare_exchange_strong(expected, true)) {
        return SCE_PATCH_CHECKER_ERROR_ALREADY_INITIALIZED;
    }
    return 0;
}

int APS5_VABI scePatchCheckerRequestCheckPatch() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerRequestCheckPatchByType() {
    if (!g_initialized.load()) return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerTerminate() {
    g_initialized.store(false);
    return 0;
}

}

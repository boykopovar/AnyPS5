#include "prx/libScePatchCheckerClient/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED = static_cast<int>(0x80182402);

}

extern "C" {

int APS5_VABI scePatchCheckerCancel() {
    return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePatchCheckerCheckPatch() {
    return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePatchCheckerCreateHandler() {
    return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePatchCheckerDestroyHandler() {
    return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePatchCheckerDisableAutoDownload() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerEnableAutoDownload() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerGetApplicableTick() {
    return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePatchCheckerGetPackageInfo() {
    return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePatchCheckerInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePatchCheckerRequestCheckPatch() {
    return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePatchCheckerRequestCheckPatchByType() {
    return SCE_PATCH_CHECKER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI scePatchCheckerTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

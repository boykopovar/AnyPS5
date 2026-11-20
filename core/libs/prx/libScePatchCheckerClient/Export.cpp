#include "prx/libScePatchCheckerClient/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libScePatchCheckerClient/include/Firmware1340Exports.hpp"

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
    return 0;
}

}

extern "C" {

int APS5_VABI libScePatchCheckerClientUnknown_6f4BOeN6_plus_Bs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6f4BOeN6+Bs", libScePatchCheckerClientUnknown_6f4BOeN6_plus_Bs_nid_no_patch);

}

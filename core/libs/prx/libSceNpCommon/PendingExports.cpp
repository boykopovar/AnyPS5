#include "prx/libSceNpCommon/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceNpGetSandboxMode() {
    return sceNpUtilGetSandboxMode() != 0 ? 1 : 0;
}

int APS5_VABI sceNpUtilGetSandboxMode() {
    return 0;
}

int APS5_VABI sceNpCloseSema(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpOpenSema(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUtilGetDateSetAuto(void) {
    return 0;
}

int APS5_VABI sceNpUtilGetDbgCommerce(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUtilGetEnv(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUtilGetFakeDisplayNameMode(void) {
    return 0;
}

int APS5_VABI sceNpUtilGetFakeRateLimit(void) {
    return 100;
}

int APS5_VABI sceNpUtilGetIgnoreNpTitleId(void) {
    return 0;
}

int APS5_VABI sceNpUtilGetNpDebug(void) {
    return 0;
}

int APS5_VABI sceNpUtilGetNpTestPatch(void) {
    return 0;
}

int APS5_VABI sceNpUtilGetShareTitleCheck(void) {
    return 0;
}

int APS5_VABI sceNpUtilGetSystemLanguage(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUtilGetTrcNotify(void) {
    return 0;
}

int APS5_VABI sceNpUtilGetWebApi2FakeRateLimit(void) {
    return 0;
}

int APS5_VABI sceNpUtilGetWebApi2FakeRateLimitTarget(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNpUtilGetWebTraceSetting(void) {
    return 0;
}

}

#include "prx/libSceRnpsAppMgr/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorNotInitialized = static_cast<int>(0x81850001u);

} // namespace

extern "C" {

int APS5_VABI sceRnpsAppMgrBlockAppInstall() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrCreateUpdateRequest() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrCreateUrlRequest() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrDestroyBundle() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrDestroyUpdateRequest() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrFinishUrlRequest() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrGetAppInfo() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrGetAppInfoDebugString() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrGetAppInfoList() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrGetEvent() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrGetEventForShellUI() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRnpsAppMgrGetMboInfo() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrGetMboManifestDebugString() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrGetUpdateProgress() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrHasEventForShellUI() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRnpsAppMgrInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRnpsAppMgrIsBgsInstallReady() {
    return 0;
}

int APS5_VABI sceRnpsAppMgrIsCrlUpdated() {
    return 0;
}

int APS5_VABI sceRnpsAppMgrIsMboUpdated() {
    return 0;
}

int APS5_VABI sceRnpsAppMgrPushEventForShellUI() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRnpsAppMgrRecoverUfsImage() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrRegisterShellUI() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRnpsAppMgrRemoveApp() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrRemoveAppOnSystemShutdown() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrRemoveUfsImageOnSystemShutdown() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrRequestUpdateCheck() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrSetCrlUrl() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrSetEventFlagForClients() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrSetUpdateUrl() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrStartService() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrTerminate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceRnpsAppMgrUnblockAppInstall() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrUnregisterShellUI() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}
int APS5_VABI sceRnpsAppMgrVerifyApp() {
    return ErrorNotInitialized;
}

int APS5_VABI sceRnpsAppMgrVerifyFileInApp() {
    return ErrorNotInitialized;
}


}

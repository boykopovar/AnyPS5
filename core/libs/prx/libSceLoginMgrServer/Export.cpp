#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceLoginMgrServer/include/InventoryExports.hpp"
#include "prx/libSceLoginMgrServer/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED = static_cast<int>(0x813E0001);

}

extern "C" {

int APS5_VABI sceLoginMgrServerCheckSafetyNoticeShown() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerClearMorpheusRequiredUserId() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerConvertToUserId() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerCreateUser() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerDestroyUser() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerDialogGetAppId() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerDialogGetOpenParam() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerDialogNotifyCloseFinished() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerDialogNotifyOpenFinished() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerDialogSetResult() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerGetLogoutInfo() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerGetMorpheusRequiredUserId() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerGetSharePlayAllowPadOperation() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerInitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLoginMgrServerInitializeSharePlayAllowPadOperation() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerIsRequestedCdlgClose() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLoginMgrServerLoginServiceGetRequestParam() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerLoginServiceNotifyRequestFinished() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerNotifyEasySignInFinished() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerNotifyEasySignInUserCode() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerNotifyHmdConnectInfo() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerNotifySafetyNoticeShown() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerNotifyShellUIState() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerSetLoginFlag() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerSetMorpheusAssignEnabledFlag() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerSetMorpheusRequiredUserId() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerSetSharePlayMode() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerSetUserStatus() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerTerminate() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerUpdateUserIdRalatedToPadUniqueId() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerNotifySafetyNoticeShownForGen2Vr() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

int APS5_VABI sceLoginMgrServerCheckSafetyNoticeShownForGen2Vr() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}

}

extern "C" {

int APS5_VABI libSceLoginMgrServerUnknown_GzbYqYyRTBg_nid_no_patch() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("GzbYqYyRTBg", libSceLoginMgrServerUnknown_GzbYqYyRTBg_nid_no_patch);

int APS5_VABI libSceLoginMgrServerUnknown_M1h7vFE7Hm0_nid_no_patch() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("M1h7vFE7Hm0", libSceLoginMgrServerUnknown_M1h7vFE7Hm0_nid_no_patch);

int APS5_VABI libSceLoginMgrServerUnknown_NvO5e_minus_NOkXU_nid_no_patch() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("NvO5e-NOkXU", libSceLoginMgrServerUnknown_NvO5e_minus_NOkXU_nid_no_patch);

int APS5_VABI libSceLoginMgrServerUnknown_T7HQTX_minus_ylss_nid_no_patch() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("T7HQTX-ylss", libSceLoginMgrServerUnknown_T7HQTX_minus_ylss_nid_no_patch);

int APS5_VABI libSceLoginMgrServerUnknown_uD_minus_Sde_plus_R4D8_nid_no_patch() {
    return SCE_LOGIN_MGR_SERVER_ERROR_NOT_INITIALIZED;
}
APS5_EXPORT("uD-Sde+R4D8", libSceLoginMgrServerUnknown_uD_minus_Sde_plus_R4D8_nid_no_patch);

}

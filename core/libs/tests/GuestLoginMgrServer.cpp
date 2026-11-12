#include "prx/libSceLoginMgrServer/libSceLoginMgrServer.hpp"

#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

} // namespace

int main() {
    constexpr int notInitialized = static_cast<int>(0x813E0001u);
    for (int call = 0; call < 2; ++call) {
        Require(sceLoginMgrServerCheckSafetyNoticeShown() == notInitialized);
        Require(sceLoginMgrServerClearMorpheusRequiredUserId() == notInitialized);
        Require(sceLoginMgrServerConvertToUserId() == notInitialized);
        Require(sceLoginMgrServerCreateUser() == notInitialized);
        Require(sceLoginMgrServerDestroyUser() == notInitialized);
        Require(sceLoginMgrServerDialogGetAppId() == notInitialized);
        Require(sceLoginMgrServerDialogGetOpenParam() == notInitialized);
        Require(sceLoginMgrServerDialogNotifyCloseFinished() == notInitialized);
        Require(sceLoginMgrServerDialogNotifyOpenFinished() == notInitialized);
        Require(sceLoginMgrServerDialogSetResult() == notInitialized);
        Require(sceLoginMgrServerGetLogoutInfo() == notInitialized);
        Require(sceLoginMgrServerGetMorpheusRequiredUserId() == notInitialized);
        Require(sceLoginMgrServerGetSharePlayAllowPadOperation() == notInitialized);
        Require(sceLoginMgrServerInitializeSharePlayAllowPadOperation() == notInitialized);
        Require(sceLoginMgrServerLoginServiceGetRequestParam() == notInitialized);
        Require(sceLoginMgrServerLoginServiceNotifyRequestFinished() == notInitialized);
        Require(sceLoginMgrServerNotifyEasySignInFinished() == notInitialized);
        Require(sceLoginMgrServerNotifyEasySignInUserCode() == notInitialized);
        Require(sceLoginMgrServerNotifyHmdConnectInfo() == notInitialized);
        Require(sceLoginMgrServerNotifySafetyNoticeShown() == notInitialized);
        Require(sceLoginMgrServerNotifyShellUIState() == notInitialized);
        Require(sceLoginMgrServerSetLoginFlag() == notInitialized);
        Require(sceLoginMgrServerSetMorpheusAssignEnabledFlag() == notInitialized);
        Require(sceLoginMgrServerSetMorpheusRequiredUserId() == notInitialized);
        Require(sceLoginMgrServerSetSharePlayMode() == notInitialized);
        Require(sceLoginMgrServerSetUserStatus() == notInitialized);
        Require(sceLoginMgrServerTerminate() == notInitialized);
        Require(sceLoginMgrServerUpdateUserIdRalatedToPadUniqueId() == notInitialized);
        Require(sceLoginMgrServerNotifySafetyNoticeShownForGen2Vr() == notInitialized);
        Require(sceLoginMgrServerCheckSafetyNoticeShownForGen2Vr() == notInitialized);
        Require(libSceLoginMgrServerUnknown_GzbYqYyRTBg_nid_no_patch() == notInitialized);
        Require(libSceLoginMgrServerUnknown_M1h7vFE7Hm0_nid_no_patch() == notInitialized);
        Require(libSceLoginMgrServerUnknown_NvO5e_minus_NOkXU_nid_no_patch() == notInitialized);
        Require(libSceLoginMgrServerUnknown_T7HQTX_minus_ylss_nid_no_patch() == notInitialized);
        Require(libSceLoginMgrServerUnknown_uD_minus_Sde_plus_R4D8_nid_no_patch() == notInitialized);
    }
    return 0;
}

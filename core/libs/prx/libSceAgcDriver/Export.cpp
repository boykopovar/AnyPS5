#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAgcDriver/include/AgcDriver.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgcDriver/include/Firmware1340Exports.hpp"

extern "C" {

int APS5_VABI sceAgcDriverCreateQueue(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverCwsrResumeAcq(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverCwsrSuspendAcq(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverDebugHardwareStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverDestroyQueue(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverGetRegShadowInfo(void* info) {
    if (info == nullptr) return static_cast<int>(0x8A6D0003u);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAgcDriverGetRegShadowInfoAgr(void* info) {
    if (info == nullptr) return static_cast<int>(0x8A6D0003u);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAgcDriverGetReservedDmemForAgc(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverGetWorkloadStreamInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverIDHSSubmit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverNotifyDefaultStates(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverPassInfoDownward(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverPatchClearState(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetFlip(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetWorkloadComplete(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetWorkloadsActive(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetupAsyncGraphics(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSetupRegisterShadow(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSubmitToHDRScopesACQ(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSuspendPointSubmit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSysEnableSubmitDone45Exception(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSysGetClientNumber(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverSysIsGameClosed(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcDriverTmpInitIdhs(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaClose(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaConstFill(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaCopyLinear(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaCopyTiledBC(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaCopyTiledGen2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaCopyWindowBC(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaCopyWindowGen2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaFlush(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaOpen(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAgcSdmaSetTileModesBC(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceAgcDriverUnknown_6WO4CscVnfM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6WO4CscVnfM", libSceAgcDriverUnknown_6WO4CscVnfM_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_6v_minus_D6CQHA4Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6v-D6CQHA4Y", libSceAgcDriverUnknown_6v_minus_D6CQHA4Y_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_8CXPPlNmvIc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8CXPPlNmvIc", libSceAgcDriverUnknown_8CXPPlNmvIc_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_Cia3HgY0WiY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Cia3HgY0WiY", libSceAgcDriverUnknown_Cia3HgY0WiY_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_DGN_plus_IaHNR8o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DGN+IaHNR8o", libSceAgcDriverUnknown_DGN_plus_IaHNR8o_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_EqwPDQyh_plus_Ec_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("EqwPDQyh+Ec", libSceAgcDriverUnknown_EqwPDQyh_plus_Ec_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_IGYqP_plus_WKCgc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("IGYqP+WKCgc", libSceAgcDriverUnknown_IGYqP_plus_WKCgc_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_LgctzJSGy7s_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("LgctzJSGy7s", libSceAgcDriverUnknown_LgctzJSGy7s_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_NmE3PfE211o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NmE3PfE211o", libSceAgcDriverUnknown_NmE3PfE211o_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_O144kUYDSBo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("O144kUYDSBo", libSceAgcDriverUnknown_O144kUYDSBo_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_gpF0LZ3_plus_19c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gpF0LZ3+19c", libSceAgcDriverUnknown_gpF0LZ3_plus_19c_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_mxyUaQfPgb8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mxyUaQfPgb8", libSceAgcDriverUnknown_mxyUaQfPgb8_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_oFb2hMcoJa4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("oFb2hMcoJa4", libSceAgcDriverUnknown_oFb2hMcoJa4_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_sW7yp7NVlrk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("sW7yp7NVlrk", libSceAgcDriverUnknown_sW7yp7NVlrk_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_tD2ECNSuTQQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tD2ECNSuTQQ", libSceAgcDriverUnknown_tD2ECNSuTQQ_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_wxdcyVqXJEY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wxdcyVqXJEY", libSceAgcDriverUnknown_wxdcyVqXJEY_nid_no_patch);

int APS5_VABI sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do(std::int32_t kind, std::int64_t arg) {
    if (kind != 1) return static_cast<int>(0x8A6D0003u);
    if (arg != 0) return static_cast<int>(0x8A6D0005u);
    sceKernelDebugRaiseException(0xA002030A, 0);
    return static_cast<int>(0x8A6D0003u);
}
APS5_EXPORT("yuO+lNrj+Do", sceAgcDriverUnknown_yuO_plus_lNrj_plus_Do);

}

extern "C" {

int APS5_VABI libSceAgcDriverUnknown_1l15IdtXNWw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1l15IdtXNWw", libSceAgcDriverUnknown_1l15IdtXNWw_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_38aK8XQj6Cg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("38aK8XQj6Cg", libSceAgcDriverUnknown_38aK8XQj6Cg_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_6fugJiMPLGQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("6fugJiMPLGQ", libSceAgcDriverUnknown_6fugJiMPLGQ_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_8j6_minus_e3dPf4c_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8j6-e3dPf4c", libSceAgcDriverUnknown_8j6_minus_e3dPf4c_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_9rDcI6cJb_plus_8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9rDcI6cJb+8", libSceAgcDriverUnknown_9rDcI6cJb_plus_8_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_CkL1hUyKZyo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("CkL1hUyKZyo", libSceAgcDriverUnknown_CkL1hUyKZyo_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_Hb3p8BJm2PI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Hb3p8BJm2PI", libSceAgcDriverUnknown_Hb3p8BJm2PI_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_IfCY5tDfgK8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("IfCY5tDfgK8", libSceAgcDriverUnknown_IfCY5tDfgK8_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_JvVqPfac9vE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("JvVqPfac9vE", libSceAgcDriverUnknown_JvVqPfac9vE_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_RXqABdLqFFA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RXqABdLqFFA", libSceAgcDriverUnknown_RXqABdLqFFA_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_V_plus_rBNI2HzGA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("V+rBNI2HzGA", libSceAgcDriverUnknown_V_plus_rBNI2HzGA_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_cRjqDvsrGkg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cRjqDvsrGkg", libSceAgcDriverUnknown_cRjqDvsrGkg_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_dN1zf573IKU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dN1zf573IKU", libSceAgcDriverUnknown_dN1zf573IKU_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_kFiKcGagv5k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("kFiKcGagv5k", libSceAgcDriverUnknown_kFiKcGagv5k_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_myaO_minus_KfzonI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("myaO-KfzonI", libSceAgcDriverUnknown_myaO_minus_KfzonI_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_nTwzz_minus_6W9Jg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("nTwzz-6W9Jg", libSceAgcDriverUnknown_nTwzz_minus_6W9Jg_nid_no_patch);

int APS5_VABI libSceAgcDriverUnknown_nhzPF9s5WF0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("nhzPF9s5WF0", libSceAgcDriverUnknown_nhzPF9s5WF0_nid_no_patch);

}

#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceAudioOut/include/InventoryExports.hpp"
#include "prx/libSceAudioOut/include/LaterExports.hpp"
#include "prx/libc/include/General.hpp"
#include "AudioOut2Internal.hpp"

static int ValidateHrtfAssignment(int32_t userId, int32_t hrtfId) {
    constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);
    constexpr int ErrorInvalidUser = static_cast<int>(0x80268010);
    if (hrtfId != -1 && hrtfId != 0x80 && static_cast<uint32_t>(hrtfId) - 1u > 4u) return ErrorInvalidParam;
    if (static_cast<uint32_t>(userId) - 0xfeu < 2u || userId == -1) return ErrorInvalidUser;
    return 0;
}

extern "C" {

int APS5_VABI sceAudioOut2GetSystemDebugState(float* state) {
    constexpr int ErrorNotInitialized = static_cast<int>(0x80268006);
    (void)state;
    if (!AudioOut2IsInitialized_nid_no_patch()) return ErrorNotInitialized;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2SetHrtfIdForCronos(int32_t userId, int32_t hrtfId) {
    const int status = ValidateHrtfAssignment(userId, hrtfId);
    if (status != 0) return status;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2SetSpeakerInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2SetTvCorrectionInfo(uint32_t mode, const void* info) {
    constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);
    constexpr int ErrorInvalidPointer = static_cast<int>(0x8026800c);
    if (info == nullptr) return ErrorInvalidPointer;
    if (mode > 1u) return ErrorInvalidParam;
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceAudioOutUnknown__minus_3HTIXaIx7U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-3HTIXaIx7U", libSceAudioOutUnknown__minus_3HTIXaIx7U_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_0Oz9yG27ovw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0Oz9yG27ovw", libSceAudioOutUnknown_0Oz9yG27ovw_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_1pWftEwJg6w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1pWftEwJg6w", libSceAudioOutUnknown_1pWftEwJg6w_nid_no_patch);

int APS5_VABI sceAudioOut2GetLoContextParam() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2LoPortGetParam() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAudioOutUnknown_87IcZgIbuxQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("87IcZgIbuxQ", libSceAudioOutUnknown_87IcZgIbuxQ_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_8qn2ceJ4HVg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8qn2ceJ4HVg", libSceAudioOutUnknown_8qn2ceJ4HVg_nid_no_patch);

int APS5_VABI sceAudioOut2GetLoContextHandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2SetDebugAudioMode() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAudioOutUnknown_DEpjrJh_minus_flc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("DEpjrJh-flc", libSceAudioOutUnknown_DEpjrJh_minus_flc_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_GIcJie_plus_rjvM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GIcJie+rjvM", libSceAudioOutUnknown_GIcJie_plus_rjvM_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_KeHpCnAvtGw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KeHpCnAvtGw", libSceAudioOutUnknown_KeHpCnAvtGw_nid_no_patch);

int APS5_VABI sceAudioOut2GetMainUserHandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAudioOutUnknown_PNeVjMfwq1M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PNeVjMfwq1M", libSceAudioOutUnknown_PNeVjMfwq1M_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_PSg2qlLNCqM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PSg2qlLNCqM", libSceAudioOutUnknown_PSg2qlLNCqM_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_UAk5DtTuUMo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("UAk5DtTuUMo", libSceAudioOutUnknown_UAk5DtTuUMo_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_WKpJvbBdcbU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WKpJvbBdcbU", libSceAudioOutUnknown_WKpJvbBdcbU_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_WOfIge9NmBA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WOfIge9NmBA", libSceAudioOutUnknown_WOfIge9NmBA_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_YSm2BXd3gfE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YSm2BXd3gfE", libSceAudioOutUnknown_YSm2BXd3gfE_nid_no_patch);

int APS5_VABI sceAudioOut2GetDebugMemoryInfo() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAudioOutUnknown_bd5H255vnsk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("bd5H255vnsk", libSceAudioOutUnknown_bd5H255vnsk_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_f3xQNbru2m8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("f3xQNbru2m8", libSceAudioOutUnknown_f3xQNbru2m8_nid_no_patch);

int APS5_VABI sceAudioOut2EnableTVVirtual() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAudioOutUnknown_gaUU6uC15vs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gaUU6uC15vs", libSceAudioOutUnknown_gaUU6uC15vs_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_jEcHKuQ4k8I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("jEcHKuQ4k8I", libSceAudioOutUnknown_jEcHKuQ4k8I_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_lyHeaetMYXg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lyHeaetMYXg", libSceAudioOutUnknown_lyHeaetMYXg_nid_no_patch);

int APS5_VABI sceAudioOutSysGetLastOutputTime() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAudioOutUnknown_rSyeebu6wWI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("rSyeebu6wWI", libSceAudioOutUnknown_rSyeebu6wWI_nid_no_patch);

int APS5_VABI sceAudioOut2AssignHRTF(int32_t userId, int32_t hrtfId) {
    const int status = ValidateHrtfAssignment(userId, hrtfId);
    if (status != 0) return status;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceAudioOutUnknown_spvw78DsDzU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("spvw78DsDzU", libSceAudioOutUnknown_spvw78DsDzU_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_tqTyWw6NgyU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tqTyWw6NgyU", libSceAudioOutUnknown_tqTyWw6NgyU_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_tvPlXmc4MHs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tvPlXmc4MHs", libSceAudioOutUnknown_tvPlXmc4MHs_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_yt9g5kdJy1o_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yt9g5kdJy1o", libSceAudioOutUnknown_yt9g5kdJy1o_nid_no_patch);

}

extern "C" {

int APS5_VABI sceAudioOut2CustomHrtfIsExist() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2Enable3DAudioForUserForCronos() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2EnableCinematicVrRotation() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2EnableMonoHpOutput() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2EnableTVCorrection() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2SetHearingAssist() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2SetSparkEQ() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceAudioOut2SetSpeakerInfoForCronos() {
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI libSceAudioOutUnknown_0E_minus_UkHm_plus_hZI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0E-UkHm+hZI", libSceAudioOutUnknown_0E_minus_UkHm_plus_hZI_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_0rE3t9ZgyXE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("0rE3t9ZgyXE", libSceAudioOutUnknown_0rE3t9ZgyXE_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_7uryG3ZsXQU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7uryG3ZsXQU", libSceAudioOutUnknown_7uryG3ZsXQU_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_9rWEssK8GMk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("9rWEssK8GMk", libSceAudioOutUnknown_9rWEssK8GMk_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_BDnwsIqujuQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BDnwsIqujuQ", libSceAudioOutUnknown_BDnwsIqujuQ_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_EeEaULhU2_minus_g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("EeEaULhU2-g", libSceAudioOutUnknown_EeEaULhU2_minus_g_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_ElgxuhkuY_minus_k_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ElgxuhkuY-k", libSceAudioOutUnknown_ElgxuhkuY_minus_k_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_GKzVK6pYgTk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("GKzVK6pYgTk", libSceAudioOutUnknown_GKzVK6pYgTk_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_NuGwMXbT8RU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("NuGwMXbT8RU", libSceAudioOutUnknown_NuGwMXbT8RU_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_QEcXEGqvnb4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QEcXEGqvnb4", libSceAudioOutUnknown_QEcXEGqvnb4_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_aFxSMCVm5mQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aFxSMCVm5mQ", libSceAudioOutUnknown_aFxSMCVm5mQ_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_cRISiyyeyps_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cRISiyyeyps", libSceAudioOutUnknown_cRISiyyeyps_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_cYDvFPDrl6g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("cYDvFPDrl6g", libSceAudioOutUnknown_cYDvFPDrl6g_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_dDyfkQG1MPc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("dDyfkQG1MPc", libSceAudioOutUnknown_dDyfkQG1MPc_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_fcv6kkqIs1Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("fcv6kkqIs1Y", libSceAudioOutUnknown_fcv6kkqIs1Y_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_g_minus_JTbmmuf7M_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("g-JTbmmuf7M", libSceAudioOutUnknown_g_minus_JTbmmuf7M_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_mWypJWwaLcc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mWypJWwaLcc", libSceAudioOutUnknown_mWypJWwaLcc_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_tylNHKM16GA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("tylNHKM16GA", libSceAudioOutUnknown_tylNHKM16GA_nid_no_patch);

int APS5_VABI libSceAudioOutUnknown_yd_minus_81WQGSTw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yd-81WQGSTw", libSceAudioOutUnknown_yd_minus_81WQGSTw_nid_no_patch);

}

#include "prx/libSceWebRtc/include/InventoryExports.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <cstdint>
#include "prx/libSceWebRtc/include/Firmware1340Exports.hpp"

namespace {

constexpr std::uint16_t kSysmoduleRandom = 0x00bau;
constexpr std::uint32_t kSysmoduleRtc = 0x80000020u;
constexpr std::uint32_t kSysmoduleOpusDec = 0x80000069u;
constexpr std::uint32_t kSysmoduleOpusCeltEnc = 0x80000043u;
constexpr std::uint32_t kSysmoduleOpusSilkEnc = 0x80000068u;
constexpr std::uint32_t kSysmoduleResourceArbitrator = 0x80000092u;

} // namespace

extern "C" {

int APS5_VABI _ZN3sce2np3rtc9TerminateEv() {
    (void)sceSysmoduleUnloadModule(kSysmoduleRandom);
    (void)sceSysmoduleUnloadModuleInternal(kSysmoduleRtc);
    (void)sceSysmoduleUnloadModuleInternal(kSysmoduleOpusDec);
    (void)sceSysmoduleUnloadModuleInternal(kSysmoduleOpusCeltEnc);
    (void)sceSysmoduleUnloadModuleInternal(kSysmoduleOpusSilkEnc);
    return sceSysmoduleUnloadModuleInternal(kSysmoduleResourceArbitrator);
}

int APS5_VABI libSceWebRtcUnknown_SRJUfO0JINA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SRJUfO0JINA", libSceWebRtcUnknown_SRJUfO0JINA_nid_no_patch);

int APS5_VABI libSceWebRtcUnknown_Z7F8YLISPDI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Z7F8YLISPDI", libSceWebRtcUnknown_Z7F8YLISPDI_nid_no_patch);

int APS5_VABI libSceWebRtcUnknown_ZZV_plus_i_plus__minus_wetM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ZZV+i+-wetM", libSceWebRtcUnknown_ZZV_plus_i_plus__minus_wetM_nid_no_patch);

int APS5_VABI libSceWebRtcUnknown_z1bMECaLQhM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("z1bMECaLQhM", libSceWebRtcUnknown_z1bMECaLQhM_nid_no_patch);

}

extern "C" {

int APS5_VABI libSceWebRtcUnknown_4ZADb2AOVjA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("4ZADb2AOVjA", libSceWebRtcUnknown_4ZADb2AOVjA_nid_no_patch);

int APS5_VABI libSceWebRtcUnknown_KagFilTdh_minus_A_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KagFilTdh-A", libSceWebRtcUnknown_KagFilTdh_minus_A_nid_no_patch);

int APS5_VABI libSceWebRtcUnknown_TpKtnsQa_plus_kM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("TpKtnsQa+kM", libSceWebRtcUnknown_TpKtnsQa_plus_kM_nid_no_patch);

int APS5_VABI libSceWebRtcUnknown_UGkRT49_minus_n9E_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("UGkRT49-n9E", libSceWebRtcUnknown_UGkRT49_minus_n9E_nid_no_patch);

int APS5_VABI libSceWebRtcUnknown_gTNthJLY4gY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("gTNthJLY4gY", libSceWebRtcUnknown_gTNthJLY4gY_nid_no_patch);

int APS5_VABI libSceWebRtcUnknown_lCy4Fu02MPQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lCy4Fu02MPQ", libSceWebRtcUnknown_lCy4Fu02MPQ_nid_no_patch);

}

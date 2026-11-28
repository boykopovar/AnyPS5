#include "prx/libSceWebRtc/include/InventoryExports.hpp"
#include "prx/libSceSysmodule/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <cstdint>

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

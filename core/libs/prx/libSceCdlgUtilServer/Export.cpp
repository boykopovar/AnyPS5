#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceCdlgUtilServer/include/InventoryExports.hpp"
#include "prx/libSceCdlgUtilServer/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <string>

namespace {

constexpr int kServerResult80B8000E = static_cast<int>(0x80B8000Eu);

bool HasServerImplementation(const void* self, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
    std::uint64_t implementation = 0;
    if (!GuestCopyIn_nid_no_patch(&implementation, static_cast<const unsigned char*>(self) + 0x8, sizeof(implementation)))
        throw std::invalid_argument(std::string(function) + ": unreadable object");
    return implementation != 0;
}

} // namespace

extern "C" {

int APS5_VABI _ZN3sce16CommonDialogUtil6Server13setFinishDataEPvm(void* self, void* data, std::size_t size) {
    (void)data;
    (void)size;
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return kServerResult80B8000E;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6Server6setRunEPvm(void* self, void* data, std::size_t size) {
    (void)data;
    (void)size;
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return kServerResult80B8000E;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6Server9setFinishEv(void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return kServerResult80B8000E;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6Server9setResultEi(void* self, int result) {
    (void)result;
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return kServerResult80B8000E;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6ServerC2EPKcmmis() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6ServerD0Ev() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6ServerD1Ev() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6ServerD2Ev() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Server10getVersionERj(const void* self, std::uint32_t* version) {
    (void)version;
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return kServerResult80B8000E;
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Server10isCloseReqEv(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Server14getServerStateEv(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return -1;
}

}

extern "C" {

int APS5_VABI libSceCdlgUtilServerUnknown_1IzIqsh2lVE_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return -1;
}
APS5_EXPORT("1IzIqsh2lVE", libSceCdlgUtilServerUnknown_1IzIqsh2lVE_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_2pswOa0ZQoQ_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2pswOa0ZQoQ", libSceCdlgUtilServerUnknown_2pswOa0ZQoQ_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_33zrWSbHxcI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("33zrWSbHxcI", libSceCdlgUtilServerUnknown_33zrWSbHxcI_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_56KPLy5Uc_minus_w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("56KPLy5Uc-w", libSceCdlgUtilServerUnknown_56KPLy5Uc_minus_w_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_CK_minus_7W3zhCjk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("CK-7W3zhCjk", libSceCdlgUtilServerUnknown_CK_minus_7W3zhCjk_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_Dv1KWHTwGeM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Dv1KWHTwGeM", libSceCdlgUtilServerUnknown_Dv1KWHTwGeM_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_KaTogANOP6U_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KaTogANOP6U", libSceCdlgUtilServerUnknown_KaTogANOP6U_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_MQDTRLwdnrU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("MQDTRLwdnrU", libSceCdlgUtilServerUnknown_MQDTRLwdnrU_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_NKhtpQTUjpY_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return -1;
}
APS5_EXPORT("NKhtpQTUjpY", libSceCdlgUtilServerUnknown_NKhtpQTUjpY_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_Pl1B1_plus_pzwLM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Pl1B1+pzwLM", libSceCdlgUtilServerUnknown_Pl1B1_plus_pzwLM_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_RiJYtG8H1Yk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("RiJYtG8H1Yk", libSceCdlgUtilServerUnknown_RiJYtG8H1Yk_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_VMUTkgazl40_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("VMUTkgazl40", libSceCdlgUtilServerUnknown_VMUTkgazl40_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_VrN0lR_plus_BJyc_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return kServerResult80B8000E;
}
APS5_EXPORT("VrN0lR+BJyc", libSceCdlgUtilServerUnknown_VrN0lR_plus_BJyc_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_W4zcizD4i3I_nid_no_patch(const void* self) {
    if (self == nullptr) throw std::invalid_argument("W4zcizD4i3I: null object");
    return static_cast<const unsigned char*>(self)[0x10];
}
APS5_EXPORT("W4zcizD4i3I", libSceCdlgUtilServerUnknown_W4zcizD4i3I_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_WlSxsADNweU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WlSxsADNweU", libSceCdlgUtilServerUnknown_WlSxsADNweU_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_YNi5ipH59bE_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YNi5ipH59bE", libSceCdlgUtilServerUnknown_YNi5ipH59bE_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_ZRy0WQp2COs_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return kServerResult80B8000E;
}
APS5_EXPORT("ZRy0WQp2COs", libSceCdlgUtilServerUnknown_ZRy0WQp2COs_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_aFrwE9W9faU_nid_no_patch() {
    return 0;
}
APS5_EXPORT("aFrwE9W9faU", libSceCdlgUtilServerUnknown_aFrwE9W9faU_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_aX1ZGnOo9vg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aX1ZGnOo9vg", libSceCdlgUtilServerUnknown_aX1ZGnOo9vg_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_aztojjv0xT0_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return kServerResult80B8000E;
}
APS5_EXPORT("aztojjv0xT0", libSceCdlgUtilServerUnknown_aztojjv0xT0_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_dQte4n_minus_nQlg_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("dQte4n-nQlg", libSceCdlgUtilServerUnknown_dQte4n_minus_nQlg_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_e3l9WkmIjyw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("e3l9WkmIjyw", libSceCdlgUtilServerUnknown_e3l9WkmIjyw_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_ht7qLD4xIAU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ht7qLD4xIAU", libSceCdlgUtilServerUnknown_ht7qLD4xIAU_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_j6vGxXgVnCQ_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return kServerResult80B8000E;
}
APS5_EXPORT("j6vGxXgVnCQ", libSceCdlgUtilServerUnknown_j6vGxXgVnCQ_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_k9wR_plus_1LoaoU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("k9wR+1LoaoU", libSceCdlgUtilServerUnknown_k9wR_plus_1LoaoU_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_lhSjnuF2bzg_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("lhSjnuF2bzg", libSceCdlgUtilServerUnknown_lhSjnuF2bzg_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_mCWvwveN2gM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mCWvwveN2gM", libSceCdlgUtilServerUnknown_mCWvwveN2gM_nid_no_patch);

std::uint16_t APS5_VABI libSceCdlgUtilServerUnknown_qFygU3yA0HE_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return 0xFFFF;
}
APS5_EXPORT("qFygU3yA0HE", libSceCdlgUtilServerUnknown_qFygU3yA0HE_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_ucY3L5rXqjs_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ucY3L5rXqjs", libSceCdlgUtilServerUnknown_ucY3L5rXqjs_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_vfUweaM01gw_nid_no_patch(const void* self) {
    if (HasServerImplementation(self, __func__)) NotImplemented_nid_no_patch(__func__);
    return -1;
}
APS5_EXPORT("vfUweaM01gw", libSceCdlgUtilServerUnknown_vfUweaM01gw_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_yPspG9QpJK4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yPspG9QpJK4", libSceCdlgUtilServerUnknown_yPspG9QpJK4_nid_no_patch);

int APS5_VABI libSceCdlgUtilServerUnknown_zjj3moy7KpM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("zjj3moy7KpM", libSceCdlgUtilServerUnknown_zjj3moy7KpM_nid_no_patch);

}

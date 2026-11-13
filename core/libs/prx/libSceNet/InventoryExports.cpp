#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include <algorithm>
#include <cstddef>

static_assert(sizeof(NetDhcpOption) == 16 && offsetof(NetDhcpOption, code) == 8 && offsetof(NetDhcpOption, declaredLength) == 10);

extern "C" {

int APS5_VABI libSceNetUnknown__plus_hZ3zpjYiYw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("+hZ3zpjYiYw", libSceNetUnknown__plus_hZ3zpjYiYw_nid_no_patch);

int APS5_VABI libSceNetUnknown__minus_qjsa0xOWMc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("-qjsa0xOWMc", libSceNetUnknown__minus_qjsa0xOWMc_nid_no_patch);

int APS5_VABI libSceNetUnknown_DnB6WJ91HGg_nid_no_patch(const std::uint8_t* options, std::int32_t length, NetDhcpOption* option) {
    constexpr int invalid = static_cast<int>(0x80411506u);
    if (options == nullptr || option == nullptr) return invalid;
    option->length = 0;
    option->declaredLength = 0;
    if (length < 2) return invalid;
    for (std::int32_t offset = 0; offset < length;) {
        offset += 2;
        const std::int32_t remaining = length - offset;
        const std::uint8_t code = options[0];
        const std::uint8_t size = options[1];
        options += 2;
        if (code == option->code) {
            option->declaredLength = size;
            option->length = static_cast<std::uint8_t>(std::min<std::int32_t>(size, remaining));
            option->value = options;
            return 0;
        }
        if (size == 0 || remaining <= size) return invalid;
        offset += size;
        options += size;
    }
    return 0;
}
APS5_EXPORT("DnB6WJ91HGg", libSceNetUnknown_DnB6WJ91HGg_nid_no_patch);

int APS5_VABI libSceNetUnknown_HMievhVSU3g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("HMievhVSU3g", libSceNetUnknown_HMievhVSU3g_nid_no_patch);

int APS5_VABI libSceNetUnknown_YQZkUbAw6rg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("YQZkUbAw6rg", libSceNetUnknown_YQZkUbAw6rg_nid_no_patch);

int APS5_VABI libSceNetUnknown_guwFkKH_plus_wP8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("guwFkKH+wP8", libSceNetUnknown_guwFkKH_plus_wP8_nid_no_patch);

int APS5_VABI libSceNetUnknown_mnXcgCBmiV0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mnXcgCBmiV0", libSceNetUnknown_mnXcgCBmiV0_nid_no_patch);

int APS5_VABI libSceNetUnknown_pGboVhl_minus_6P0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("pGboVhl-6P0", libSceNetUnknown_pGboVhl_minus_6P0_nid_no_patch);

}

#include <cstdint>
#include "prx/libharfbuzz/include/InventoryExports.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI hb_ft_face_create_cached_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI hb_icu_script_to_script_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI hb_ot_layout_language_find_feature_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI hb_ot_layout_script_get_language_tags_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI hb_ot_layout_table_get_script_tags_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

std::uint32_t APS5_VABI hb_ot_tag_to_script_nid_postfix(std::uint32_t tag) {
    const auto digit = static_cast<std::uint8_t>(tag & 0xFFu);
    if (digit == '2' || digit == '3') {
        switch (tag & 0xFFFFFF32u) {
        case 0x626E6732u:
            return 0x42656E67u;
        case 0x64657632u:
            return 0x44657661u;
        case 0x676A7232u:
            return 0x47756A72u;
        case 0x67757232u:
            return 0x47757275u;
        case 0x6B6E6432u:
            return 0x4B6E6461u;
        case 0x6D6C6D32u:
            return 0x4D6C796Du;
        case 0x6F727932u:
            return 0x4F727961u;
        case 0x746D6C32u:
            return 0x54616D6Cu;
        case 0x74656C32u:
            return 0x54656C75u;
        case 0x6D796D32u:
            return 0x4D796D72u;
        default:
            return 0x5A7A7A7Au;
        }
    }

    if (tag == 0x44464C54u) {
        return 0;
    }
    if (tag == 0x6D617468u) {
        return 0x5A6D7468u;
    }

    if ((tag & 0x0000FF00u) == 0x00002000u) {
        tag |= (tag >> 8) & 0x0000FF00u;
    }
    if ((tag & 0x000000FFu) == 0x00000020u) {
        tag |= (tag >> 8) & 0x000000FFu;
    }

    return tag & ~0x20000000u;
}

int APS5_VABI hb_shape_nid_postfix() {
    NotImplemented_nid_no_patch(__func__);
}

}

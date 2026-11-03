#include "prx/libfreetype/include/FaceSettings.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include FT_COLOR_H
#include <limits>
#include <vector>

namespace {

FT_Face HostFace(const FreeType::GuestFaceRec* face) {
    const auto* owner = face ? static_cast<const FreeType::GuestFace*>(face->internal) : nullptr;
    return owner ? owner->host : nullptr;
}

FT_ULong HostTag(std::uint64_t tag) {
    return tag <= std::numeric_limits<FT_ULong>::max() ? static_cast<FT_ULong>(tag) : 0;
}

}

extern "C" {

std::int32_t APS5_VABI FT_Face_Properties_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t numProperties, LegacyFreeType::GuestParameter* properties) {
    const FT_Face host = HostFace(face);
    if (!host) return FT_Err_Invalid_Face_Handle;
    std::vector<FT_Parameter> native(properties ? static_cast<std::size_t>(numProperties) + 1 : 0);
    if (properties) {
        for (std::uint32_t index = 0; index < numProperties; ++index) native[index] = {HostTag(properties[index].tag), properties[index].data};
    }
    return FT_Face_Properties(host, numProperties, properties ? native.data() : nullptr);
}

std::int32_t APS5_VABI FT_Palette_Set_Foreground_Color_nid_postfix(FreeType::GuestFaceRec* face, LegacyFreeType::GuestColor color) {
    return FT_Palette_Set_Foreground_Color(HostFace(face), FT_Color{color.blue, color.green, color.red, color.alpha});
}

}

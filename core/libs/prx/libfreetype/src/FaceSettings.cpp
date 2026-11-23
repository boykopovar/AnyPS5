#include "prx/libfreetype/include/FaceSettings.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include FT_COLOR_H
#include FT_PARAMETER_TAGS_H
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <limits>

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
    if (!properties) return FT_Face_Properties(host, numProperties, nullptr);
    for (std::uint32_t index = 0; index < numProperties; ++index) {
        LegacyFreeType::GuestParameter property{};
        if (!GuestCopyIn_nid_no_patch(&property, properties + index, sizeof(property))) return FT_Err_Invalid_Argument;
        FT_Parameter native{HostTag(property.tag), property.data};
        FT_Bool darkening = 0;
        FT_Int32 seed = 0;
        if (property.data && native.tag == FT_PARAM_TAG_STEM_DARKENING) {
            if (!GuestCopyIn_nid_no_patch(&darkening, property.data, sizeof(darkening))) return FT_Err_Invalid_Argument;
            native.data = &darkening;
        } else if (property.data && native.tag == FT_PARAM_TAG_RANDOM_SEED) {
            if (!GuestCopyIn_nid_no_patch(&seed, property.data, sizeof(seed))) return FT_Err_Invalid_Argument;
            native.data = &seed;
        }
        if (const FT_Error error = FT_Face_Properties(host, 1, &native)) return error;
    }
    return FT_Err_Ok;
}

std::int32_t APS5_VABI FT_Palette_Set_Foreground_Color_nid_postfix(FreeType::GuestFaceRec* face, LegacyFreeType::GuestColor color) {
    return FT_Palette_Set_Foreground_Color(HostFace(face), FT_Color{color.blue, color.green, color.red, color.alpha});
}

}

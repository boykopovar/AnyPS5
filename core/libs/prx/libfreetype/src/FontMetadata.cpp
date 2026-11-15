#include "prx/libfreetype/include/FontMetadata.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include FT_BDF_H
#include FT_SFNT_NAMES_H
#include FT_WINFONTS_H

#include <algorithm>

namespace {

FT_Face HostFace(const FreeType::GuestFaceRec* face) {
    const auto* owner = face ? static_cast<const FreeType::GuestFace*>(face->internal) : nullptr;
    return owner ? owner->host : nullptr;
}

}

extern "C" {

int APS5_VABI freetypeLegacyGetBDFProperty_nid_no_patch(FreeType::GuestFaceRec* face, const char* name, LegacyFreeType::BdfProperty* property) {
    const FT_Face host = HostFace(face);
    BDF_PropertyRec native{};
    const int error = FT_Get_BDF_Property(host, name, property ? &native : nullptr);
    if (host && property) {
        GuestArena::HostWrite write(property, sizeof(*property));
        if (!write.Open()) return FT_Err_Invalid_Argument;
        property->type = native.type;
        if (!error) {
            switch (native.type) {
                case BDF_PROPERTY_TYPE_ATOM: property->value.atom = native.u.atom; break;
                case BDF_PROPERTY_TYPE_INTEGER: property->value.integer = native.u.integer; break;
                case BDF_PROPERTY_TYPE_CARDINAL: property->value.cardinal = native.u.cardinal; break;
                default: break;
            }
        }
    }
    return error;
}
APS5_EXPORT("4kVFLShKGkQ", freetypeLegacyGetBDFProperty_nid_no_patch);

int APS5_VABI freetypeLegacyGetBDFCharsetID_nid_no_patch(FreeType::GuestFaceRec* face, const char** encoding, const char** registry) {
    const FT_Face host = HostFace(face);
    if (!host) return FT_Get_BDF_Charset_ID(nullptr, nullptr, nullptr);
    GuestArena::HostWrite encodingWrite(encoding, encoding ? sizeof(*encoding) : 0);
    GuestArena::HostWrite registryWrite(registry, registry ? sizeof(*registry) : 0);
    if ((encoding && !encodingWrite.Open()) || (registry && !registryWrite.Open())) return FT_Err_Invalid_Argument;
    return FT_Get_BDF_Charset_ID(host, encoding, registry);
}
APS5_EXPORT("KVRW5HPnbgU", freetypeLegacyGetBDFCharsetID_nid_no_patch);

int APS5_VABI freetypeLegacyGetSfntLangTag_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t language, LegacyFreeType::SfntLangTag* tag) {
    FT_SfntLangTag native{};
    const int error = FT_Get_Sfnt_LangTag(HostFace(face), language, tag ? &native : nullptr);
    if (!error) {
        GuestArena::HostWrite write(tag, sizeof(*tag));
        if (!write.Open()) return FT_Err_Invalid_Argument;
        tag->string = native.string;
        tag->string_len = native.string_len;
    }
    return error;
}
APS5_EXPORT("MDsMjUGulyY", freetypeLegacyGetSfntLangTag_nid_no_patch);

int APS5_VABI freetypeLegacyGetWinFNTHeader_nid_no_patch(FreeType::GuestFaceRec* face, LegacyFreeType::WinFntHeader* header) {
    FT_WinFNT_HeaderRec native{};
    const int error = FT_Get_WinFNT_Header(HostFace(face), header ? &native : nullptr);
    if (!error) {
        GuestArena::HostWrite write(header, sizeof(*header));
        if (!write.Open()) return FT_Err_Invalid_Argument;
        header->version = native.version;
        header->file_size = native.file_size;
        std::copy_n(native.copyright, 60, header->copyright);
        header->file_type = native.file_type;
        header->nominal_point_size = native.nominal_point_size;
        header->vertical_resolution = native.vertical_resolution;
        header->horizontal_resolution = native.horizontal_resolution;
        header->ascent = native.ascent;
        header->internal_leading = native.internal_leading;
        header->external_leading = native.external_leading;
        header->italic = native.italic;
        header->underline = native.underline;
        header->strike_out = native.strike_out;
        header->weight = native.weight;
        header->charset = native.charset;
        header->pixel_width = native.pixel_width;
        header->pixel_height = native.pixel_height;
        header->pitch_and_family = native.pitch_and_family;
        header->avg_width = native.avg_width;
        header->max_width = native.max_width;
        header->first_char = native.first_char;
        header->last_char = native.last_char;
        header->default_char = native.default_char;
        header->break_char = native.break_char;
        header->bytes_per_row = native.bytes_per_row;
        header->device_offset = native.device_offset;
        header->face_name_offset = native.face_name_offset;
        header->bits_pointer = native.bits_pointer;
        header->bits_offset = native.bits_offset;
        header->reserved = native.reserved;
        header->flags = native.flags;
        header->A_space = native.A_space;
        header->B_space = native.B_space;
        header->C_space = native.C_space;
        header->color_table_offset = native.color_table_offset;
        std::copy_n(native.reserved1, 4, header->reserved1);
    }
    return error;
}
APS5_EXPORT("suWwpZzQl00", freetypeLegacyGetWinFNTHeader_nid_no_patch);

}

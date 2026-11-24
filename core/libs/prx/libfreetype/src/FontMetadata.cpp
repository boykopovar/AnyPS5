#include "prx/libfreetype/include/FontMetadata.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include FT_BDF_H
#include FT_SFNT_NAMES_H
#include FT_WINFONTS_H

#include <algorithm>
#include <cstddef>

namespace {

FT_Face HostFace(const FreeType::GuestFaceRec* face) {
    const auto* owner = face ? static_cast<const FreeType::GuestFace*>(face->internal) : nullptr;
    return owner ? owner->host : nullptr;
}

template <typename Record, typename Modify>
bool Update(Record* destination, Modify modify) {
    GuestArena::HostWrite write(destination, sizeof(Record));
    Record record{};
    if (!write.Open() || !GuestWritable_nid_no_patch(destination, sizeof(Record)) || !GuestCopyIn_nid_no_patch(&record, destination, sizeof(Record))) return false;
    modify(record);
    return GuestCopyOut_nid_no_patch(destination, &record, sizeof(Record));
}

}

extern "C" {

int APS5_VABI freetypeLegacyGetBDFProperty_nid_no_patch(FreeType::GuestFaceRec* face, const char* name, LegacyFreeType::BdfProperty* property) {
    const FT_Face host = HostFace(face);
    BDF_PropertyRec native{};
    const int error = FT_Get_BDF_Property(host, name, property ? &native : nullptr);
    if (host && property && !Update(property, [&](LegacyFreeType::BdfProperty& record) {
            record.type = native.type;
            if (!error) {
                switch (native.type) {
                    case BDF_PROPERTY_TYPE_ATOM: record.value.atom = native.u.atom; break;
                    case BDF_PROPERTY_TYPE_INTEGER: record.value.integer = native.u.integer; break;
                    case BDF_PROPERTY_TYPE_CARDINAL: record.value.cardinal = native.u.cardinal; break;
                    default: break;
                }
            }
        })) {
        return FT_Err_Invalid_Argument;
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
    if (!error && !Update(tag, [&](LegacyFreeType::SfntLangTag& record) {
            record.string = native.string;
            record.string_len = native.string_len;
        })) {
        return FT_Err_Invalid_Argument;
    }
    return error;
}
APS5_EXPORT("MDsMjUGulyY", freetypeLegacyGetSfntLangTag_nid_no_patch);

int APS5_VABI freetypeLegacyGetWinFNTHeader_nid_no_patch(FreeType::GuestFaceRec* face, LegacyFreeType::WinFntHeader* header) {
    FT_WinFNT_HeaderRec native{};
    const int error = FT_Get_WinFNT_Header(HostFace(face), header ? &native : nullptr);
    if (!error && !Update(header, [&](LegacyFreeType::WinFntHeader& record) {
            record.version = native.version;
            record.file_size = native.file_size;
            std::copy_n(native.copyright, 60, record.copyright);
            record.file_type = native.file_type;
            record.nominal_point_size = native.nominal_point_size;
            record.vertical_resolution = native.vertical_resolution;
            record.horizontal_resolution = native.horizontal_resolution;
            record.ascent = native.ascent;
            record.internal_leading = native.internal_leading;
            record.external_leading = native.external_leading;
            record.italic = native.italic;
            record.underline = native.underline;
            record.strike_out = native.strike_out;
            record.weight = native.weight;
            record.charset = native.charset;
            record.pixel_width = native.pixel_width;
            record.pixel_height = native.pixel_height;
            record.pitch_and_family = native.pitch_and_family;
            record.avg_width = native.avg_width;
            record.max_width = native.max_width;
            record.first_char = native.first_char;
            record.last_char = native.last_char;
            record.default_char = native.default_char;
            record.break_char = native.break_char;
            record.bytes_per_row = native.bytes_per_row;
            record.device_offset = native.device_offset;
            record.face_name_offset = native.face_name_offset;
            record.bits_pointer = native.bits_pointer;
            record.bits_offset = native.bits_offset;
            record.reserved = native.reserved;
            record.flags = native.flags;
            record.A_space = native.A_space;
            record.B_space = native.B_space;
            record.C_space = native.C_space;
            record.color_table_offset = native.color_table_offset;
            std::copy_n(native.reserved1, 4, record.reserved1);
        })) {
        return FT_Err_Invalid_Argument;
    }
    return error;
}
APS5_EXPORT("suWwpZzQl00", freetypeLegacyGetWinFNTHeader_nid_no_patch);

}

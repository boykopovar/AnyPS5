#pragma once

#include "SceTypes.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <cstddef>
#include <cstdint>

namespace LegacyFreeType {

struct BdfProperty {
    std::int32_t type;
    union {
        const char* atom;
        std::int32_t integer;
        std::uint32_t cardinal;
    } value;
};

struct SfntLangTag {
    unsigned char* string;
    std::uint32_t string_len;
};

struct WinFntHeader {
    std::uint16_t version;
    std::uint64_t file_size;
    unsigned char copyright[60];
    std::uint16_t file_type;
    std::uint16_t nominal_point_size;
    std::uint16_t vertical_resolution;
    std::uint16_t horizontal_resolution;
    std::uint16_t ascent;
    std::uint16_t internal_leading;
    std::uint16_t external_leading;
    unsigned char italic;
    unsigned char underline;
    unsigned char strike_out;
    std::uint16_t weight;
    unsigned char charset;
    std::uint16_t pixel_width;
    std::uint16_t pixel_height;
    unsigned char pitch_and_family;
    std::uint16_t avg_width;
    std::uint16_t max_width;
    unsigned char first_char;
    unsigned char last_char;
    unsigned char default_char;
    unsigned char break_char;
    std::uint16_t bytes_per_row;
    std::uint64_t device_offset;
    std::uint64_t face_name_offset;
    std::uint64_t bits_pointer;
    std::uint64_t bits_offset;
    unsigned char reserved;
    std::uint64_t flags;
    std::uint16_t A_space;
    std::uint16_t B_space;
    std::uint16_t C_space;
    std::uint16_t color_table_offset;
    std::uint64_t reserved1[4];
};

static_assert(sizeof(BdfProperty) == 16 && offsetof(BdfProperty, value) == 8);
static_assert(sizeof(SfntLangTag) == 16 && offsetof(SfntLangTag, string_len) == 8);
static_assert(sizeof(WinFntHeader) == 208);
static_assert(offsetof(WinFntHeader, file_size) == 8);
static_assert(offsetof(WinFntHeader, device_offset) == 120);
static_assert(offsetof(WinFntHeader, reserved1) == 176);

}

extern "C" {

int APS5_VABI freetypeLegacyGetBDFProperty_nid_no_patch(FreeType::GuestFaceRec* face, const char* name, LegacyFreeType::BdfProperty* property);
int APS5_VABI freetypeLegacyGetBDFCharsetID_nid_no_patch(FreeType::GuestFaceRec* face, const char** encoding, const char** registry);
int APS5_VABI freetypeLegacyGetSfntLangTag_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t language, LegacyFreeType::SfntLangTag* tag);
int APS5_VABI freetypeLegacyGetWinFNTHeader_nid_no_patch(FreeType::GuestFaceRec* face, LegacyFreeType::WinFntHeader* header);

}

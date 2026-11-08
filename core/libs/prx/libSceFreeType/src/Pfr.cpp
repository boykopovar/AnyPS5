#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libSceFreeType/include/GuestPfr.hpp"
#include FT_FONT_FORMATS_H
#include FT_PFR_H
#include <cstring>

extern "C" {

int FreeTypePfrAdvance_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t index, std::int64_t* advance) {
    if (!face) return FT_Err_Invalid_Argument;
    const auto* owner = FreeType::FaceOf(face);
    const FT_Face host = owner ? owner->host : nullptr;
    if (!host || !advance) return FT_Get_PFR_Advance(host, index, nullptr);
    FT_Pos value = 0;
    const auto error = FT_Get_PFR_Advance(host, index, &value);
    const char* format = FT_Get_Font_Format(host);
    if (error == 0 || (format && std::strcmp(format, "PFR") == 0)) *advance = value;
    return error;
}

int FreeTypePfrKerning_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t left, std::uint32_t right, FreeType::GuestVector* vector) {
    if (!face) return FT_Err_Invalid_Argument;
    const auto* owner = FreeType::FaceOf(face);
    const FT_Face host = owner ? owner->host : nullptr;
    if (!host || !vector) return FT_Get_PFR_Kerning(host, left, right, nullptr);
    FT_Vector value{};
    const auto error = FT_Get_PFR_Kerning(host, left, right, &value);
    *vector = {value.x, value.y};
    return error;
}

int FreeTypePfrMetrics_nid_no_patch(FreeType::GuestFaceRec* face, std::uint32_t* outline, std::uint32_t* metrics, std::int64_t* xScale, std::int64_t* yScale) {
    if (!face) return FT_Err_Invalid_Argument;
    const auto* owner = FreeType::FaceOf(face);
    const FT_Face host = owner ? owner->host : nullptr;
    if (!host) return FT_Get_PFR_Metrics(nullptr, nullptr, nullptr, nullptr, nullptr);
    FT_UInt hostOutline = 0, hostMetrics = 0;
    FT_Fixed hostX = 0, hostY = 0;
    const auto error = FT_Get_PFR_Metrics(host, outline ? &hostOutline : nullptr, metrics ? &hostMetrics : nullptr,
                                          xScale ? &hostX : nullptr, yScale ? &hostY : nullptr);
    if (outline) *outline = hostOutline;
    if (metrics) *metrics = hostMetrics;
    if (xScale) *xScale = hostX;
    if (yScale) *yScale = hostY;
    return error;
}

}

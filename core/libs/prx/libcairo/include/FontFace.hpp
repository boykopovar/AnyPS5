#pragma once

#include "SceTypes.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libcairo/include/Surface.hpp"
#include "prx/libfontconfig/include/Types.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string>

struct CairoFontFace;

struct CairoFtFontFaceKey {
    bool fromFace{};
    const void* face{};
    std::string filename;
    std::int64_t id{};
    int loadFlags{};
    unsigned synthFlags{};
    int antialias{};
    int subpixelOrder{};
    int lcdFilter{};
    int hintStyle{};
    std::optional<std::string> variations;
    auto operator<=>(const CairoFtFontFaceKey&) const = default;
};

CairoFontFace* CairoFtFontFaceFind(const CairoFtFontFaceKey& key);
CairoFontFace* CairoFtFontFaceForPattern(FcPattern* duplicate);
CairoFontFace* CairoFontFaceNilNoMemory();
CairoFontFace* CairoFontFaceNilFileNotFound();

extern "C" {
CairoFontFace* APS5_VABI cairo_ft_font_face_create_for_ft_face(FreeType::GuestFaceRec* face, int loadFlags);
CairoFontFace* APS5_VABI cairo_ft_font_face_create_for_pattern(FcPattern* pattern);
CairoFontFace* APS5_VABI cairo_font_face_reference(CairoFontFace* fontFace);
void APS5_VABI cairo_font_face_destroy(CairoFontFace* fontFace);
void* APS5_VABI cairo_font_face_get_user_data(CairoFontFace* fontFace, const CairoUserDataKey* key);
int APS5_VABI cairo_font_face_set_user_data(CairoFontFace* fontFace, const CairoUserDataKey* key,
                                            void* data, CairoDestroyFunc destroy);
}

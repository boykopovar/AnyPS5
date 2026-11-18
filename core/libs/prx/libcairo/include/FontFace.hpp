#pragma once

#include "SceTypes.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libcairo/include/Surface.hpp"

struct CairoFontFace;

extern "C" {
CairoFontFace* APS5_VABI cairo_ft_font_face_create_for_ft_face(FreeType::GuestFaceRec* face, int loadFlags);
CairoFontFace* APS5_VABI cairo_font_face_reference(CairoFontFace* fontFace);
void APS5_VABI cairo_font_face_destroy(CairoFontFace* fontFace);
void* APS5_VABI cairo_font_face_get_user_data(CairoFontFace* fontFace, const CairoUserDataKey* key);
int APS5_VABI cairo_font_face_set_user_data(CairoFontFace* fontFace, const CairoUserDataKey* key,
                                            void* data, CairoDestroyFunc destroy);
}

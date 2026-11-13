#pragma once

#include "SceTypes.hpp"

#include <cstdint>

struct hb_face_t;
struct hb_font_t;
struct hb_font_funcs_t;
struct hb_glyph_extents_t;

namespace HarfBuzz {

using Destroy = void (APS5_VABI *)(void*);
using NominalGlyph = int (APS5_VABI *)(hb_font_t*, void*, std::uint32_t, std::uint32_t*, void*);
using VariationGlyph = int (APS5_VABI *)(hb_font_t*, void*, std::uint32_t, std::uint32_t, std::uint32_t*, void*);
using GlyphExtents = int (APS5_VABI *)(hb_font_t*, void*, std::uint32_t, hb_glyph_extents_t*, void*);
using GlyphAdvance = std::int32_t (APS5_VABI *)(hb_font_t*, void*, std::uint32_t, void*);
using GlyphOrigin = int (APS5_VABI *)(hb_font_t*, void*, std::uint32_t, std::int32_t*, std::int32_t*, void*);

}

extern "C" {

hb_font_t* APS5_VABI hb_font_create_nid_postfix(hb_face_t* face);
void APS5_VABI hb_font_destroy_nid_postfix(hb_font_t* font);
hb_font_funcs_t* APS5_VABI hb_font_funcs_create_nid_postfix();
void APS5_VABI hb_font_funcs_make_immutable_nid_postfix(hb_font_funcs_t* functions);
void APS5_VABI hb_font_funcs_set_glyph_extents_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::GlyphExtents callback, void* data, HarfBuzz::Destroy destroy);
void APS5_VABI hb_font_funcs_set_glyph_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::VariationGlyph callback, void* data, HarfBuzz::Destroy destroy);
void APS5_VABI hb_font_funcs_set_glyph_h_advance_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::GlyphAdvance callback, void* data, HarfBuzz::Destroy destroy);
void APS5_VABI hb_font_funcs_set_glyph_h_origin_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::GlyphOrigin callback, void* data, HarfBuzz::Destroy destroy);
void APS5_VABI hb_font_funcs_set_nominal_glyph_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::NominalGlyph callback, void* data, HarfBuzz::Destroy destroy);
void APS5_VABI hb_font_funcs_set_variation_glyph_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::VariationGlyph callback, void* data, HarfBuzz::Destroy destroy);
void APS5_VABI hb_font_make_immutable_nid_postfix(hb_font_t* font);
void APS5_VABI hb_font_set_funcs_nid_postfix(hb_font_t* font, hb_font_funcs_t* functions, void* data, HarfBuzz::Destroy destroy);
void APS5_VABI hb_font_set_ppem_nid_postfix(hb_font_t* font, std::uint32_t x, std::uint32_t y);
void APS5_VABI hb_font_set_scale_nid_postfix(hb_font_t* font, std::int32_t x, std::int32_t y);

}

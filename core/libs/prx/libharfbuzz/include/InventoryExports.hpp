#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI hb_blob_create();
int APS5_VABI hb_buffer_add_utf16();
int APS5_VABI hb_buffer_create();
int APS5_VABI hb_buffer_destroy();
int APS5_VABI hb_buffer_get_direction();
int APS5_VABI hb_buffer_get_glyph_infos();
int APS5_VABI hb_buffer_get_glyph_positions();
int APS5_VABI hb_buffer_get_length();
int APS5_VABI hb_buffer_guess_segment_properties();
int APS5_VABI hb_buffer_reset();
int APS5_VABI hb_buffer_set_direction();
int APS5_VABI hb_buffer_set_script();
int APS5_VABI hb_face_create_for_tables();
int APS5_VABI hb_face_destroy();
int APS5_VABI hb_font_create();
int APS5_VABI hb_font_destroy();
int APS5_VABI hb_font_funcs_create();
int APS5_VABI hb_font_funcs_make_immutable();
int APS5_VABI hb_font_funcs_set_glyph_extents_func();
int APS5_VABI hb_font_funcs_set_glyph_func();
int APS5_VABI hb_font_funcs_set_glyph_h_advance_func();
int APS5_VABI hb_font_funcs_set_glyph_h_origin_func();
int APS5_VABI hb_font_funcs_set_nominal_glyph_func();
int APS5_VABI hb_font_funcs_set_variation_glyph_func();
int APS5_VABI hb_font_make_immutable();
int APS5_VABI hb_font_set_funcs();
int APS5_VABI hb_font_set_ppem();
int APS5_VABI hb_font_set_scale();
int APS5_VABI hb_ft_face_create_cached();
int APS5_VABI hb_icu_script_to_script();
int APS5_VABI hb_ot_layout_language_find_feature();
int APS5_VABI hb_ot_layout_script_get_language_tags();
int APS5_VABI hb_ot_layout_table_get_script_tags();
int APS5_VABI hb_ot_tag_to_script();
int APS5_VABI hb_shape();

}

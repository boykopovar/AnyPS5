#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI hb_blob_create_nid_postfix();
int APS5_VABI hb_buffer_add_utf16_nid_postfix();
int APS5_VABI hb_buffer_create_nid_postfix();
int APS5_VABI hb_buffer_destroy_nid_postfix();
int APS5_VABI hb_buffer_get_direction_nid_postfix();
int APS5_VABI hb_buffer_get_glyph_infos_nid_postfix();
int APS5_VABI hb_buffer_get_glyph_positions_nid_postfix();
int APS5_VABI hb_buffer_get_length_nid_postfix();
int APS5_VABI hb_buffer_guess_segment_properties_nid_postfix();
int APS5_VABI hb_buffer_reset_nid_postfix();
int APS5_VABI hb_buffer_set_direction_nid_postfix();
int APS5_VABI hb_buffer_set_script_nid_postfix();
int APS5_VABI hb_face_create_for_tables_nid_postfix();
int APS5_VABI hb_face_destroy_nid_postfix();
int APS5_VABI hb_ft_face_create_cached_nid_postfix();
int APS5_VABI hb_icu_script_to_script_nid_postfix();
int APS5_VABI hb_ot_tag_to_script_nid_postfix();
int APS5_VABI hb_shape_nid_postfix();

}

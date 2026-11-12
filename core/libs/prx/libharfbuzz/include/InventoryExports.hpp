#pragma once

#include "SceTypes.hpp"

struct hb_buffer_t;
struct hb_glyph_info_t;
struct hb_glyph_position_t;
struct hb_unicode_funcs_t;

extern "C" {

void APS5_VABI hb_buffer_add_utf16_nid_postfix(hb_buffer_t* buffer, const std::uint16_t* text, int textLength, unsigned itemOffset, int itemLength);
hb_buffer_t* APS5_VABI hb_buffer_create_nid_postfix();
void APS5_VABI hb_buffer_destroy_nid_postfix(hb_buffer_t* buffer);
std::uint32_t APS5_VABI hb_buffer_get_direction_nid_postfix(const hb_buffer_t* buffer);
hb_glyph_info_t* APS5_VABI hb_buffer_get_glyph_infos_nid_postfix(hb_buffer_t* buffer, unsigned* length);
hb_glyph_position_t* APS5_VABI hb_buffer_get_glyph_positions_nid_postfix(hb_buffer_t* buffer, unsigned* length);
unsigned APS5_VABI hb_buffer_get_length_nid_postfix(const hb_buffer_t* buffer);
void APS5_VABI hb_buffer_guess_segment_properties_nid_postfix(hb_buffer_t* buffer);
void APS5_VABI hb_buffer_reset_nid_postfix(hb_buffer_t* buffer);
void APS5_VABI hb_buffer_set_direction_nid_postfix(hb_buffer_t* buffer, std::uint32_t direction);
void APS5_VABI hb_buffer_set_script_nid_postfix(hb_buffer_t* buffer, std::uint32_t script);
void APS5_VABI hb_buffer_set_unicode_funcs_nid_postfix(hb_buffer_t* buffer, hb_unicode_funcs_t* unicode_funcs);
int APS5_VABI hb_ft_face_create_cached_nid_postfix();
std::uint32_t APS5_VABI hb_icu_script_to_script_nid_postfix(std::int32_t scriptCode);
std::uint32_t APS5_VABI hb_ot_tag_to_script_nid_postfix(std::uint32_t tag);

}

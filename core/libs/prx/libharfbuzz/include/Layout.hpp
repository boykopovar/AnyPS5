#pragma once

#include "SceTypes.hpp"

#include <cstdint>

struct hb_face_t;

extern "C" {

std::uint32_t APS5_VABI hb_ot_layout_table_get_script_tags_nid_postfix(hb_face_t* face, std::uint32_t table, std::uint32_t start, std::uint32_t* count, std::uint32_t* tags);
std::uint32_t APS5_VABI hb_ot_layout_script_get_language_tags_nid_postfix(hb_face_t* face, std::uint32_t table, std::uint32_t script, std::uint32_t start, std::uint32_t* count, std::uint32_t* tags);
int APS5_VABI hb_ot_layout_language_find_feature_nid_postfix(hb_face_t* face, std::uint32_t table, std::uint32_t script, std::uint32_t language, std::uint32_t feature, std::uint32_t* index);

}

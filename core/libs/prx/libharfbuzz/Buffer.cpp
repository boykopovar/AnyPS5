#include "prx/libharfbuzz/libharfbuzz.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"

#include <hb.h>

#include <cstddef>
#include <cstdint>
#include <type_traits>

static_assert(sizeof(hb_direction_t) == 4);
static_assert(sizeof(hb_script_t) == 4);
static_assert(std::is_same_v<hb_codepoint_t, std::uint32_t>);
static_assert(std::is_same_v<hb_position_t, std::int32_t>);
static_assert(sizeof(unsigned) == 4);
static_assert(sizeof(hb_glyph_info_t) == 20);
static_assert(alignof(hb_glyph_info_t) == 4);
static_assert(offsetof(hb_glyph_info_t, codepoint) == 0);
static_assert(offsetof(hb_glyph_info_t, mask) == 4);
static_assert(offsetof(hb_glyph_info_t, cluster) == 8);
static_assert(offsetof(hb_glyph_info_t, var1) == 12);
static_assert(offsetof(hb_glyph_info_t, var2) == 16);
static_assert(sizeof(hb_glyph_position_t) == 20);
static_assert(alignof(hb_glyph_position_t) == 4);
static_assert(offsetof(hb_glyph_position_t, x_advance) == 0);
static_assert(offsetof(hb_glyph_position_t, y_advance) == 4);
static_assert(offsetof(hb_glyph_position_t, x_offset) == 8);
static_assert(offsetof(hb_glyph_position_t, y_offset) == 12);
static_assert(offsetof(hb_glyph_position_t, var) == 16);

extern "C" {

hb_buffer_t* APS5_VABI hb_buffer_create_nid_postfix() {
    return hb_buffer_create();
}

void APS5_VABI hb_buffer_destroy_nid_postfix(hb_buffer_t* buffer) {
    hb_buffer_destroy(buffer);
}

void APS5_VABI hb_buffer_add_utf16_nid_postfix(hb_buffer_t* buffer, const std::uint16_t* text, int textLength, unsigned itemOffset, int itemLength) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    hb_buffer_add_utf16(buffer, text, textLength, itemOffset, itemLength);
}

unsigned APS5_VABI hb_buffer_get_length_nid_postfix(const hb_buffer_t* buffer) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    return hb_buffer_get_length(buffer);
}

std::uint32_t APS5_VABI hb_buffer_get_direction_nid_postfix(const hb_buffer_t* buffer) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    return hb_buffer_get_direction(buffer);
}

void APS5_VABI hb_buffer_set_direction_nid_postfix(hb_buffer_t* buffer, std::uint32_t direction) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    hb_buffer_set_direction(buffer, static_cast<hb_direction_t>(direction));
}

void APS5_VABI hb_buffer_set_script_nid_postfix(hb_buffer_t* buffer, std::uint32_t script) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    hb_buffer_set_script(buffer, static_cast<hb_script_t>(script));
}

void APS5_VABI hb_buffer_guess_segment_properties_nid_postfix(hb_buffer_t* buffer) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    hb_buffer_guess_segment_properties(buffer);
}

hb_glyph_info_t* APS5_VABI hb_buffer_get_glyph_infos_nid_postfix(hb_buffer_t* buffer, unsigned* length) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    GuestArena::HostWrite write(length, length ? sizeof(*length) : 0);
    return hb_buffer_get_glyph_infos(buffer, length);
}

hb_glyph_position_t* APS5_VABI hb_buffer_get_glyph_positions_nid_postfix(hb_buffer_t* buffer, unsigned* length) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    GuestArena::HostWrite write(length, length ? sizeof(*length) : 0);
    return hb_buffer_get_glyph_positions(buffer, length);
}

void APS5_VABI hb_buffer_reset_nid_postfix(hb_buffer_t* buffer) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    hb_buffer_reset(buffer);
}

}

#include "prx/libharfbuzz/libharfbuzz.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"

#include <hb.h>
#include <unicode/uchar.h>
#include <unicode/unorm2.h>
#include <unicode/uscript.h>
#include <unicode/utf16.h>

#include <array>

#include <cstddef>
#include <cstdint>
#include <type_traits>

/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2009  Keith Stribley
 * Copyright © 2011  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 *
 * Derived from src/hb-icu.cc@a1d9bfe62818ef0fa9cf63b6e6d51436b1c93cbc
 */

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

namespace {

hb_unicode_general_category_t IcuCategory(hb_unicode_funcs_t*, hb_codepoint_t code, void*) {
    constexpr std::array categories{
        HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED, HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER,
        HB_UNICODE_GENERAL_CATEGORY_LOWERCASE_LETTER, HB_UNICODE_GENERAL_CATEGORY_TITLECASE_LETTER,
        HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER, HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER,
        HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK, HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK,
        HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK, HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER,
        HB_UNICODE_GENERAL_CATEGORY_LETTER_NUMBER, HB_UNICODE_GENERAL_CATEGORY_OTHER_NUMBER,
        HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR, HB_UNICODE_GENERAL_CATEGORY_LINE_SEPARATOR,
        HB_UNICODE_GENERAL_CATEGORY_PARAGRAPH_SEPARATOR, HB_UNICODE_GENERAL_CATEGORY_CONTROL,
        HB_UNICODE_GENERAL_CATEGORY_FORMAT, HB_UNICODE_GENERAL_CATEGORY_PRIVATE_USE,
        HB_UNICODE_GENERAL_CATEGORY_SURROGATE, HB_UNICODE_GENERAL_CATEGORY_DASH_PUNCTUATION,
        HB_UNICODE_GENERAL_CATEGORY_OPEN_PUNCTUATION, HB_UNICODE_GENERAL_CATEGORY_CLOSE_PUNCTUATION,
        HB_UNICODE_GENERAL_CATEGORY_CONNECT_PUNCTUATION, HB_UNICODE_GENERAL_CATEGORY_OTHER_PUNCTUATION,
        HB_UNICODE_GENERAL_CATEGORY_MATH_SYMBOL, HB_UNICODE_GENERAL_CATEGORY_CURRENCY_SYMBOL,
        HB_UNICODE_GENERAL_CATEGORY_MODIFIER_SYMBOL, HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL,
        HB_UNICODE_GENERAL_CATEGORY_INITIAL_PUNCTUATION, HB_UNICODE_GENERAL_CATEGORY_FINAL_PUNCTUATION,
    };
    static_assert(categories.size() == U_CHAR_CATEGORY_COUNT);
    return categories[u_charType(code)];
}

hb_unicode_funcs_t* CreateIcuFunctions() {
    UErrorCode error = U_ZERO_ERROR;
    const auto* nfc = unorm2_getNFCInstance(&error);
    if (U_FAILURE(error)) NotImplemented_nid_no_patch("HarfBuzz ICU NFC data");
    auto* funcs = hb_unicode_funcs_create(nullptr);
    hb_unicode_funcs_set_general_category_func(funcs, IcuCategory, nullptr, nullptr);
    hb_unicode_funcs_set_combining_class_func(funcs, [](hb_unicode_funcs_t*, hb_codepoint_t code, void*) {
        return static_cast<hb_unicode_combining_class_t>(u_getCombiningClass(code));
    }, nullptr, nullptr);
    hb_unicode_funcs_set_mirroring_func(funcs, [](hb_unicode_funcs_t*, hb_codepoint_t code, void*) {
        return static_cast<hb_codepoint_t>(u_charMirror(code));
    }, nullptr, nullptr);
    hb_unicode_funcs_set_script_func(funcs, [](hb_unicode_funcs_t*, hb_codepoint_t code, void*) {
        UErrorCode error = U_ZERO_ERROR;
        const auto script = uscript_getScript(code, &error);
        return U_FAILURE(error) ? HB_SCRIPT_UNKNOWN : hb_script_from_string(uscript_getShortName(script), -1);
    }, nullptr, nullptr);
    hb_unicode_funcs_set_compose_func(funcs, [](hb_unicode_funcs_t*, hb_codepoint_t a, hb_codepoint_t b,
                                               hb_codepoint_t* composed, void* context) -> hb_bool_t {
        const auto result = unorm2_composePair(static_cast<const UNormalizer2*>(context), a, b);
        if (result < 0) return false;
        *composed = result;
        return true;
    }, const_cast<UNormalizer2*>(nfc), nullptr);
    hb_unicode_funcs_set_decompose_func(funcs, [](hb_unicode_funcs_t*, hb_codepoint_t code, hb_codepoint_t* a,
                                                 hb_codepoint_t* b, void* context) -> hb_bool_t {
        UErrorCode error = U_ZERO_ERROR;
        UChar units[4];
        const auto length = unorm2_getRawDecomposition(static_cast<const UNormalizer2*>(context), code, units, 4, &error);
        if (U_FAILURE(error) || length <= 0) return false;
        int offset = 0;
        U16_NEXT_UNSAFE(units, offset, *a);
        *b = 0;
        if (offset < length) U16_NEXT_UNSAFE(units, offset, *b);
        return offset == length && (*a != code || *b != 0);
    }, const_cast<UNormalizer2*>(nfc), nullptr);
    hb_unicode_funcs_make_immutable(funcs);
    return funcs;
}

} // namespace

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

hb_unicode_funcs_t* APS5_VABI hb_icu_get_unicode_funcs_nid_postfix() {
    static const auto funcs = CreateIcuFunctions();
    return funcs;
}

void APS5_VABI hb_buffer_set_unicode_funcs_nid_postfix(hb_buffer_t* buffer, hb_unicode_funcs_t* unicode_funcs) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    hb_buffer_set_unicode_funcs(buffer, unicode_funcs);
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

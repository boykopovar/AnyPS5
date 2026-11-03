#include "prx/libharfbuzz/include/FontFunctions.hpp"
#include "prx/libc/include/General.hpp"

#include <hb.h>

#include <memory>
#include <type_traits>

static_assert(std::is_same_v<hb_bool_t, int>);
static_assert(std::is_same_v<hb_codepoint_t, std::uint32_t>);
static_assert(std::is_same_v<hb_position_t, std::int32_t>);
static_assert(sizeof(hb_glyph_extents_t) == 16 && alignof(hb_glyph_extents_t) == 4);

namespace {

struct FontData {
    void* data;
    HarfBuzz::Destroy destroy;
};

template <typename TCallback>
struct CallbackData {
    TCallback callback;
    void* data;
    HarfBuzz::Destroy destroy;
};

void DestroyFontData(void* opaque) {
    const std::unique_ptr<FontData> data(static_cast<FontData*>(opaque));
    if (data->destroy) data->destroy(data->data);
}

template <typename TCallback>
void DestroyCallbackData(void* opaque) {
    const std::unique_ptr<CallbackData<TCallback>> data(static_cast<CallbackData<TCallback>*>(opaque));
    if (data->destroy) data->destroy(data->data);
}

void* UnwrapFontData(void* opaque) {
    return opaque ? static_cast<FontData*>(opaque)->data : nullptr;
}

template <typename TCallback, typename TNative, typename TSetter>
void SetCallback(hb_font_funcs_t* functions, TCallback callback, void* data, HarfBuzz::Destroy destroy, TNative adapter, TSetter setter) {
    if (!functions) NotImplemented_nid_no_patch(__func__);
    auto owner = std::make_unique<CallbackData<TCallback>>(callback, data, destroy);
    auto* payload = owner.release();
    setter(functions, callback ? adapter : nullptr, payload, DestroyCallbackData<TCallback>);
}

int NominalGlyph(hb_font_t* font, void* fontData, std::uint32_t unicode, std::uint32_t* glyph, void* opaque) {
    const auto& data = *static_cast<CallbackData<HarfBuzz::NominalGlyph>*>(opaque);
    return data.callback(font, UnwrapFontData(fontData), unicode, glyph, data.data);
}

int VariationGlyph(hb_font_t* font, void* fontData, std::uint32_t unicode, std::uint32_t selector, std::uint32_t* glyph, void* opaque) {
    const auto& data = *static_cast<CallbackData<HarfBuzz::VariationGlyph>*>(opaque);
    return data.callback(font, UnwrapFontData(fontData), unicode, selector, glyph, data.data);
}

int GlyphExtents(hb_font_t* font, void* fontData, std::uint32_t glyph, hb_glyph_extents_t* extents, void* opaque) {
    const auto& data = *static_cast<CallbackData<HarfBuzz::GlyphExtents>*>(opaque);
    return data.callback(font, UnwrapFontData(fontData), glyph, extents, data.data);
}

std::int32_t GlyphAdvance(hb_font_t* font, void* fontData, std::uint32_t glyph, void* opaque) {
    const auto& data = *static_cast<CallbackData<HarfBuzz::GlyphAdvance>*>(opaque);
    return data.callback(font, UnwrapFontData(fontData), glyph, data.data);
}

int GlyphOrigin(hb_font_t* font, void* fontData, std::uint32_t glyph, std::int32_t* x, std::int32_t* y, void* opaque) {
    const auto& data = *static_cast<CallbackData<HarfBuzz::GlyphOrigin>*>(opaque);
    return data.callback(font, UnwrapFontData(fontData), glyph, x, y, data.data);
}

}

extern "C" {

hb_font_t* APS5_VABI hb_font_create_nid_postfix(hb_face_t* face) {
    return hb_font_create(face);
}

void APS5_VABI hb_font_destroy_nid_postfix(hb_font_t* font) {
    hb_font_destroy(font);
}

hb_font_funcs_t* APS5_VABI hb_font_funcs_create_nid_postfix() {
    return hb_font_funcs_create();
}

void APS5_VABI hb_font_funcs_make_immutable_nid_postfix(hb_font_funcs_t* functions) {
    if (!functions) NotImplemented_nid_no_patch(__func__);
    hb_font_funcs_make_immutable(functions);
}

void APS5_VABI hb_font_funcs_set_glyph_extents_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::GlyphExtents callback, void* data, HarfBuzz::Destroy destroy) {
    SetCallback(functions, callback, data, destroy, GlyphExtents, hb_font_funcs_set_glyph_extents_func);
}

void APS5_VABI hb_font_funcs_set_glyph_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::VariationGlyph callback, void* data, HarfBuzz::Destroy destroy) {
    if (!callback) NotImplemented_nid_no_patch(__func__);
    SetCallback(functions, callback, data, destroy, VariationGlyph, hb_font_funcs_set_glyph_func);
}

void APS5_VABI hb_font_funcs_set_glyph_h_advance_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::GlyphAdvance callback, void* data, HarfBuzz::Destroy destroy) {
    SetCallback(functions, callback, data, destroy, GlyphAdvance, hb_font_funcs_set_glyph_h_advance_func);
}

void APS5_VABI hb_font_funcs_set_glyph_h_origin_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::GlyphOrigin callback, void* data, HarfBuzz::Destroy destroy) {
    SetCallback(functions, callback, data, destroy, GlyphOrigin, hb_font_funcs_set_glyph_h_origin_func);
}

void APS5_VABI hb_font_funcs_set_nominal_glyph_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::NominalGlyph callback, void* data, HarfBuzz::Destroy destroy) {
    SetCallback(functions, callback, data, destroy, NominalGlyph, hb_font_funcs_set_nominal_glyph_func);
}

void APS5_VABI hb_font_funcs_set_variation_glyph_func_nid_postfix(hb_font_funcs_t* functions, HarfBuzz::VariationGlyph callback, void* data, HarfBuzz::Destroy destroy) {
    SetCallback(functions, callback, data, destroy, VariationGlyph, hb_font_funcs_set_variation_glyph_func);
}

void APS5_VABI hb_font_make_immutable_nid_postfix(hb_font_t* font) {
    if (!font) NotImplemented_nid_no_patch(__func__);
    hb_font_make_immutable(font);
}

void APS5_VABI hb_font_set_funcs_nid_postfix(hb_font_t* font, hb_font_funcs_t* functions, void* data, HarfBuzz::Destroy destroy) {
    if (!font) NotImplemented_nid_no_patch(__func__);
    auto owner = std::make_unique<FontData>(data, destroy);
    hb_font_set_funcs(font, functions, owner.release(), DestroyFontData);
}

void APS5_VABI hb_font_set_ppem_nid_postfix(hb_font_t* font, std::uint32_t x, std::uint32_t y) {
    if (!font) NotImplemented_nid_no_patch(__func__);
    hb_font_set_ppem(font, x, y);
}

void APS5_VABI hb_font_set_scale_nid_postfix(hb_font_t* font, std::int32_t x, std::int32_t y) {
    if (!font) NotImplemented_nid_no_patch(__func__);
    hb_font_set_scale(font, x, y);
}

}

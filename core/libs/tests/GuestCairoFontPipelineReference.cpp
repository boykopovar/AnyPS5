#include "CairoFontPipelineReference.hpp"

#include <CairoBackend.h>

#include <bit>
#include <memory>

namespace CairoFontPipelineReference {

namespace {

using Font = std::unique_ptr<cairo_scaled_font_t, decltype(&aps5_cairo_scaled_font_destroy)>;

Font Create(const CairoMatrix& matrix, const CairoMatrix& ctm, const std::array<int, 4>& values) {
    cairo_font_face_t* face = aps5_cairo_toy_font_face_create("sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_font_options_t* options = aps5_cairo_font_options_create();
    aps5_cairo_font_options_set_antialias(options, static_cast<cairo_antialias_t>(values[0]));
    aps5_cairo_font_options_set_subpixel_order(options, static_cast<cairo_subpixel_order_t>(values[1]));
    aps5_cairo_font_options_set_hint_style(options, static_cast<cairo_hint_style_t>(values[2]));
    aps5_cairo_font_options_set_hint_metrics(options, static_cast<cairo_hint_metrics_t>(values[3]));
    const auto nativeMatrix = std::bit_cast<cairo_matrix_t>(matrix);
    const auto nativeCtm = std::bit_cast<cairo_matrix_t>(ctm);
    Font font(aps5_cairo_scaled_font_create(face, &nativeMatrix, &nativeCtm, options), aps5_cairo_scaled_font_destroy);
    aps5_cairo_font_options_destroy(options);
    aps5_cairo_font_face_destroy(face);
    return font;
}

}

CairoFontFace* Face(CairoContext* context) {
    return reinterpret_cast<CairoFontFace*>(aps5_cairo_get_font_face(reinterpret_cast<cairo_t*>(context)));
}

std::array<int, 4> Options(CairoScaledFont* font) {
    cairo_font_options_t* options = aps5_cairo_font_options_create();
    aps5_cairo_scaled_font_get_font_options(reinterpret_cast<cairo_scaled_font_t*>(font), options);
    const std::array<int, 4> values{aps5_cairo_font_options_get_antialias(options),
        aps5_cairo_font_options_get_subpixel_order(options), aps5_cairo_font_options_get_hint_style(options),
        aps5_cairo_font_options_get_hint_metrics(options)};
    aps5_cairo_font_options_destroy(options);
    return values;
}

int ContextStatus(CairoContext* context) {
    return aps5_cairo_status(reinterpret_cast<cairo_t*>(context));
}

CairoScaledFont* ContextFont(CairoContext* context) {
    return reinterpret_cast<CairoScaledFont*>(aps5_cairo_get_scaled_font(reinterpret_cast<cairo_t*>(context)));
}

Conversion Convert(const CairoMatrix& matrix, const CairoMatrix& ctm, const std::array<int, 4>& options,
    const char* text, int length, int glyphCapacity, int clusterCapacity, bool clusters) {
    auto font = Create(matrix, ctm, options);
    std::vector<cairo_glyph_t> glyphStorage(glyphCapacity > 0 ? glyphCapacity : 1);
    std::vector<cairo_text_cluster_t> clusterStorage(clusterCapacity > 0 ? clusterCapacity : 1);
    auto* initialGlyphs = glyphCapacity == 0 ? nullptr : glyphStorage.data();
    auto* initialClusters = clusterCapacity == 0 ? nullptr : clusterStorage.data();
    auto* glyphs = initialGlyphs;
    auto* clusterData = initialClusters;
    int glyphCount = glyphCapacity, clusterCount = clusterCapacity;
    cairo_text_cluster_flags_t flags = static_cast<cairo_text_cluster_flags_t>(99);
    const int status = aps5_cairo_scaled_font_text_to_glyphs(font.get(), 3.25, 21.5, text, length,
        &glyphs, &glyphCount, clusters ? &clusterData : nullptr, &clusterCount, &flags);
    Conversion result{status, glyphCount, clusterCount, static_cast<int>(flags),
        glyphs != initialGlyphs, clusters && clusterData != initialClusters, {}, {}};
    if (status == CAIRO_STATUS_SUCCESS) {
        for (int i = 0; i < glyphCount; ++i) result.glyphs.push_back({glyphs[i].index, glyphs[i].x, glyphs[i].y});
        if (clusters) for (int i = 0; i < clusterCount; ++i)
            result.clusters.push_back({clusterData[i].num_bytes, clusterData[i].num_glyphs});
    }
    if (glyphs != initialGlyphs) aps5_cairo_glyph_free(glyphs);
    if (clusters && clusterData != initialClusters) aps5_cairo_text_cluster_free(clusterData);
    return result;
}

CairoTextExtents Extents(const CairoMatrix& matrix, const CairoMatrix& ctm, const std::array<int, 4>& options,
    const std::vector<CairoGlyph>& glyphs) {
    auto font = Create(matrix, ctm, options);
    std::vector<cairo_glyph_t> converted;
    for (const auto& glyph : glyphs) converted.push_back({static_cast<unsigned long>(glyph.index), glyph.x, glyph.y});
    cairo_text_extents_t extents{};
    aps5_cairo_scaled_font_glyph_extents(font.get(), converted.data(), static_cast<int>(converted.size()), &extents);
    return std::bit_cast<CairoTextExtents>(extents);
}

}

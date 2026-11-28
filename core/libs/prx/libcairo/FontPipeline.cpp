#include "prx/libcairo/include/ScaledFontOptions.hpp"
#include "prx/libcairo/include/FontFace.hpp"
#include "prx/libc/include/General.hpp"

#include <CairoBackend.h>

#include <bit>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <vector>

static_assert(sizeof(CairoTextExtents) == sizeof(cairo_text_extents_t));
static_assert(offsetof(CairoTextExtents, x_advance) == offsetof(cairo_text_extents_t, x_advance));
static_assert(sizeof(CairoTextCluster) == sizeof(cairo_text_cluster_t));
static_assert(offsetof(CairoTextCluster, numGlyphs) == offsetof(cairo_text_cluster_t, num_glyphs));
static_assert(sizeof(CairoMatrix) == sizeof(cairo_matrix_t));

extern "C" {
cairo_status_t _cairo_error(cairo_status_t status);
double _cairo_matrix_compute_determinant(const cairo_matrix_t* matrix);
cairo_bool_t _cairo_matrix_is_invertible(const cairo_matrix_t* matrix);
cairo_scaled_font_t* _cairo_scaled_font_create_in_error(cairo_status_t status);
}

namespace {

struct PreserveErrno {
    int value = errno;
    ~PreserveErrno() { errno = value; }
};

cairo_scaled_font_t* Backend(CairoScaledFont* font, const char* function) {
    if (!font) NotImplemented_nid_no_patch(function);
    return reinterpret_cast<cairo_scaled_font_t*>(font);
}

template<typename T>
T Read(const T* source, const char* function) {
    T value{};
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value))) NotImplemented_nid_no_patch(function);
    return value;
}

template<typename T>
void Write(T* destination, const T& value, const char* function) {
    if (!GuestCopyOut_nid_no_patch(destination, &value, sizeof(value))) NotImplemented_nid_no_patch(function);
}

template<typename T>
void Writable(T* destination, const char* function) {
    if (destination && !GuestWritable_nid_no_patch(destination, sizeof(T))) NotImplemented_nid_no_patch(function);
}

int NullArguments(int* numGlyphs, int* numClusters, const char* function) {
    if (numGlyphs) Write(numGlyphs, 0, function);
    if (numClusters) Write(numClusters, 0, function);
    return CAIRO_STATUS_NULL_POINTER;
}

}

CairoScaledFont* CairoScaledFontCreate_nid_no_patch(CairoFontFace* face,
    const CairoMatrix* fontMatrix, const CairoMatrix* ctm,
    const CairoContextFontOptions* options, bool failedOptions) {
    auto* backend = CairoFontFaceBackend_nid_no_patch(face, "cairo_scaled_font_create");
    if (!backend || !fontMatrix || !ctm) {
        auto status = backend ? aps5_cairo_font_face_status(backend) : CAIRO_STATUS_NULL_POINTER;
        if (status == CAIRO_STATUS_SUCCESS && fontMatrix) {
            const auto matrix = std::bit_cast<cairo_matrix_t>(*fontMatrix);
            if (_cairo_matrix_compute_determinant(&matrix) != 0 && !_cairo_matrix_is_invertible(&matrix)) {
                status = _cairo_error(CAIRO_STATUS_INVALID_MATRIX);
            }
        }
        if (status == CAIRO_STATUS_SUCCESS) status = CAIRO_STATUS_NULL_POINTER;
        return reinterpret_cast<CairoScaledFont*>(_cairo_scaled_font_create_in_error(status));
    }
    const auto matrix = std::bit_cast<cairo_matrix_t>(*fontMatrix);
    const auto transform = std::bit_cast<cairo_matrix_t>(*ctm);
    std::unique_ptr<cairo_font_options_t, decltype(&aps5_cairo_font_options_destroy)> native(
        failedOptions ? aps5_cairo_font_options_copy(nullptr) :
        options ? aps5_cairo_font_options_create() : nullptr, aps5_cairo_font_options_destroy);
    if (options && !failedOptions) {
        aps5_cairo_font_options_set_antialias(native.get(), static_cast<cairo_antialias_t>(options->antialias));
        aps5_cairo_font_options_set_subpixel_order(native.get(), static_cast<cairo_subpixel_order_t>(options->subpixelOrder));
        aps5_cairo_font_options_set_hint_style(native.get(), static_cast<cairo_hint_style_t>(options->hintStyle));
        aps5_cairo_font_options_set_hint_metrics(native.get(), static_cast<cairo_hint_metrics_t>(options->hintMetrics));
    }
    return reinterpret_cast<CairoScaledFont*>(aps5_cairo_scaled_font_create(
        backend, &matrix, &transform, native.get()));
}

CairoContextFontOptions CairoScaledFontOptions_nid_no_patch(CairoScaledFont* scaledFont) {
    std::unique_ptr<cairo_font_options_t, decltype(&aps5_cairo_font_options_destroy)> options(
        aps5_cairo_font_options_create(), aps5_cairo_font_options_destroy);
    if (aps5_cairo_font_options_status(options.get())) NotImplemented_nid_no_patch("cairo_scaled_font_get_font_options");
    aps5_cairo_scaled_font_get_font_options(Backend(scaledFont, "cairo_scaled_font_get_font_options"), options.get());
    return {aps5_cairo_font_options_get_antialias(options.get()),
        aps5_cairo_font_options_get_subpixel_order(options.get()),
        aps5_cairo_font_options_get_hint_style(options.get()),
        aps5_cairo_font_options_get_hint_metrics(options.get())};
}

extern "C" {

CairoFontFace* APS5_VABI cairo_scaled_font_get_font_face(CairoScaledFont* scaledFont) {
    return CairoFontFaceFromBackend_nid_no_patch(aps5_cairo_scaled_font_get_font_face(Backend(scaledFont, __func__)));
}

void APS5_VABI cairo_set_font_face(CairoContext* context, CairoFontFace* fontFace) {
    aps5_cairo_set_font_face(reinterpret_cast<cairo_t*>(context), CairoFontFaceBackend_nid_no_patch(fontFace, __func__));
}

void APS5_VABI cairo_set_scaled_font(CairoContext* context, const CairoScaledFont* scaledFont) {
    aps5_cairo_set_scaled_font(reinterpret_cast<cairo_t*>(context), reinterpret_cast<const cairo_scaled_font_t*>(scaledFont));
}

void APS5_VABI cairo_scaled_font_glyph_extents(CairoScaledFont* scaledFont,
    const CairoGlyph* glyphs, int numGlyphs, CairoTextExtents* extents) {
    PreserveErrno preserved;
    auto* font = Backend(scaledFont, __func__);
    Writable(extents, __func__);
    std::vector<cairo_glyph_t> native;
    if (glyphs && numGlyphs > 0 && aps5_cairo_scaled_font_status(font) == CAIRO_STATUS_SUCCESS) {
        native.resize(static_cast<std::size_t>(numGlyphs));
        for (std::size_t i = 0; i < native.size(); ++i) {
            const auto glyph = Read(glyphs + i, __func__);
            if (glyph.index > std::numeric_limits<unsigned long>::max()) NotImplemented_nid_no_patch(__func__);
            native[i] = {static_cast<unsigned long>(glyph.index), glyph.x, glyph.y};
        }
    }
    cairo_text_extents_t result{};
    aps5_cairo_scaled_font_glyph_extents(font, native.empty() ? nullptr : native.data(), numGlyphs, &result);
    Write(extents, std::bit_cast<CairoTextExtents>(result), __func__);
}

int APS5_VABI cairo_scaled_font_text_to_glyphs(CairoScaledFont* scaledFont, double x, double y,
    const char* utf8, int utf8Length, CairoGlyph** glyphs, int* numGlyphs,
    CairoTextCluster** clusters, int* numClusters, int* clusterFlags) {
    PreserveErrno preserved;
    auto* font = Backend(scaledFont, __func__);
    const auto status = aps5_cairo_scaled_font_status(font);
    if (status) return status;
    const bool missing = !glyphs || !numGlyphs || (!utf8 && utf8Length != 0 && utf8Length != -1) ||
        (clusters && (!numClusters || !clusterFlags));
    Writable(numGlyphs, __func__);
    if (missing || clusters) Writable(numClusters, __func__);
    if (missing) return NullArguments(numGlyphs, numClusters, __func__);

    Writable(glyphs, __func__);
    Writable(clusters, __func__);
    Writable(clusterFlags, __func__);
    auto* originalGlyphs = Read(glyphs, __func__);
    const int glyphCapacity = originalGlyphs ? Read(numGlyphs, __func__) : 0;
    auto* originalClusters = clusters ? Read(clusters, __func__) : nullptr;
    const int clusterCapacity = originalClusters ? Read(numClusters, __func__) : 0;
    if (glyphCapacity < 0 || clusterCapacity < 0) {
        Write(numGlyphs, 0, __func__);
        if (clusters) Write(numClusters, 0, __func__);
        if (clusterFlags) Write(clusterFlags, 0, __func__);
        return _cairo_error(CAIRO_STATUS_NEGATIVE_COUNT);
    }

    std::string text;
    if (utf8 && utf8Length == -1) {
        if (CopyGuestString_nid_no_patch(utf8, 1024 * 1024, text)) NotImplemented_nid_no_patch(__func__);
    } else if (utf8 && utf8Length > 0) {
        text.resize(static_cast<std::size_t>(utf8Length));
        if (!GuestCopyIn_nid_no_patch(text.data(), utf8, text.size())) NotImplemented_nid_no_patch(__func__);
    }

    cairo_glyph_t* nativeGlyphs = nullptr;
    cairo_text_cluster_t* nativeClusters = nullptr;
    int glyphCount = 0, clusterCount = 0;
    cairo_text_cluster_flags_t flags{};
    const auto result = aps5_cairo_scaled_font_text_to_glyphs(font, x, y,
        utf8 ? text.c_str() : nullptr, utf8Length, &nativeGlyphs, &glyphCount,
        clusters ? &nativeClusters : nullptr, clusters ? &clusterCount : nullptr, clusterFlags ? &flags : nullptr);
    std::unique_ptr<cairo_glyph_t, decltype(&aps5_cairo_glyph_free)> allocatedGlyphs(
        nativeGlyphs, aps5_cairo_glyph_free);
    std::unique_ptr<cairo_text_cluster_t, decltype(&aps5_cairo_text_cluster_free)> allocatedClusters(
        nativeClusters, aps5_cairo_text_cluster_free);
    std::unique_ptr<CairoGlyph, decltype(&std::free)> guestGlyphs(nullptr, std::free);
    std::unique_ptr<CairoTextCluster, decltype(&std::free)> guestClusters(nullptr, std::free);
    auto* outputGlyphs = originalGlyphs;
    auto* outputClusters = originalClusters;
    if (result == CAIRO_STATUS_SUCCESS && glyphCount > 0) {
        const auto bytes = static_cast<std::size_t>(glyphCount) * sizeof(CairoGlyph);
        if (glyphCount > glyphCapacity) {
            guestGlyphs.reset(static_cast<CairoGlyph*>(std::malloc(bytes)));
            if (!guestGlyphs) throw std::bad_alloc();
            outputGlyphs = guestGlyphs.get();
        }
        std::vector<CairoGlyph> converted(static_cast<std::size_t>(glyphCount));
        for (std::size_t i = 0; i < converted.size(); ++i) {
            converted[i] = {nativeGlyphs[i].index, nativeGlyphs[i].x, nativeGlyphs[i].y};
        }
        if (!GuestCopyOut_nid_no_patch(outputGlyphs, converted.data(), bytes)) NotImplemented_nid_no_patch(__func__);
    }
    if (result == CAIRO_STATUS_SUCCESS && clusters && clusterCount > 0) {
        const auto bytes = static_cast<std::size_t>(clusterCount) * sizeof(CairoTextCluster);
        if (clusterCount > clusterCapacity) {
            guestClusters.reset(static_cast<CairoTextCluster*>(std::malloc(bytes)));
            if (!guestClusters) throw std::bad_alloc();
            outputClusters = guestClusters.get();
        }
        if (!GuestCopyOut_nid_no_patch(outputClusters, nativeClusters, bytes)) NotImplemented_nid_no_patch(__func__);
    }
    Write(glyphs, outputGlyphs, __func__);
    Write(numGlyphs, glyphCount, __func__);
    if (clusters) {
        Write(clusters, outputClusters, __func__);
        Write(numClusters, clusterCount, __func__);
    }
    if (clusterFlags) Write(clusterFlags, static_cast<int>(flags), __func__);
    guestGlyphs.release();
    guestClusters.release();
    return result;
}

}

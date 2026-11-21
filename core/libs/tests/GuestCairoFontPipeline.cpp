#include "prx/libcairo/libcairo.hpp"
#include "CairoFontPipelineReference.hpp"

#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {

namespace Reference = CairoFontPipelineReference;

void Check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "cairo font pipeline: %s\n", reason);
        std::abort();
    }
}

constexpr CairoMatrix matrix{18, 1.5, 2, 14, 0, 0};
constexpr CairoMatrix ctm{1.25, 0, 0, 0.75, 0, 0};
constexpr std::array<int, 4> values{CAIRO_ANTIALIAS_GRAY, CAIRO_SUBPIXEL_ORDER_BGR,
    CAIRO_HINT_STYLE_SLIGHT, CAIRO_HINT_METRICS_OFF};

cairo_font_options_t* Options() {
    auto* options = cairo_font_options_create();
    cairo_font_options_set_antialias(options, static_cast<cairo_antialias_t>(values[0]));
    cairo_font_options_set_subpixel_order(options, static_cast<cairo_subpixel_order_t>(values[1]));
    cairo_font_options_set_hint_style(options, static_cast<cairo_hint_style_t>(values[2]));
    cairo_font_options_set_hint_metrics(options, static_cast<cairo_hint_metrics_t>(values[3]));
    return options;
}

using Font = std::unique_ptr<CairoScaledFont, decltype(&cairo_scaled_font_destroy)>;

Font Create(const CairoMatrix& fontMatrix = matrix, const CairoMatrix& transform = ctm) {
    auto* surface = cairo_image_surface_create(0, 96, 64);
    auto* context = cairo_create(surface);
    cairo_select_font_face(context, "sans", 0, 0);
    auto* face = Reference::Face(context);
    auto* options = Options();
    Font font(cairo_scaled_font_create(face, &fontMatrix, &transform, options), cairo_scaled_font_destroy);
    cairo_font_options_destroy(options);
    cairo_destroy(context);
    cairo_surface_destroy(surface);
    return font;
}

template<class F>
void Rejects(F function) {
    errno = EDOM;
    bool threw = false;
    try { function(); } catch (const std::runtime_error&) { threw = true; }
    Check(threw && errno == EDOM, "invalid guest memory throws and preserves errno");
}

void Creation() {
    auto font = Create();
    Check(cairo_scaled_font_status(font.get()) == 0, "created font succeeds");
    Check(Reference::Options(font.get()) == values, "constructor preserves every public font option");
    auto* copied = cairo_font_options_create();
    cairo_scaled_font_get_font_options(font.get(), copied);
    auto* face = cairo_scaled_font_get_font_face(font.get());
    Check(face != nullptr, "font face is borrowed from scaled font");
    Font roundtrip(cairo_scaled_font_create(face, &matrix, &ctm, copied), cairo_scaled_font_destroy);
    Check(Reference::Options(roundtrip.get()) == values, "queried options reconstruct the font");
    cairo_font_options_destroy(copied);
    cairo_scaled_font_get_font_options(font.get(), nullptr);
    auto* nil = cairo_font_options_copy(nullptr);
    cairo_scaled_font_get_font_options(font.get(), nil);
    Font failed(cairo_scaled_font_create(face, &matrix, &ctm, nil), cairo_scaled_font_destroy);
    Check(cairo_scaled_font_status(failed.get()) == 1, "nil font options produce no-memory font");
    cairo_font_options_destroy(nil);
    Font nullOptions(cairo_scaled_font_create(face, &matrix, &ctm, nullptr), cairo_scaled_font_destroy);
    Check(cairo_scaled_font_status(nullOptions.get()) == 7, "null options produce null-pointer font");
    CairoMatrix invalid = matrix;
    invalid.xx = std::numeric_limits<double>::infinity();
    auto bad = Create(invalid);
    Check(cairo_scaled_font_status(bad.get()) == 5, "nonfinite matrix produces invalid-matrix font");
    auto* defaults = Options();
    cairo_scaled_font_get_font_options(bad.get(), defaults);
    Check(cairo_font_options_get_antialias(defaults) == CAIRO_ANTIALIAS_DEFAULT,
        "failed font query resets options to defaults");
    cairo_font_options_destroy(defaults);
    const CairoMatrix zero{};
    auto degenerate = Create(zero);
    Check(cairo_scaled_font_status(degenerate.get()) == 0, "zero font matrix remains valid");
    auto* options = Options();
    Font nullFace(cairo_scaled_font_create(nullptr, &matrix, &ctm, options), cairo_scaled_font_destroy);
    Check(cairo_scaled_font_status(nullFace.get()) == 7, "null face produces null-pointer font");
    Font nullMatrix(cairo_scaled_font_create(face, nullptr, &ctm, options), cairo_scaled_font_destroy);
    Check(cairo_scaled_font_status(nullMatrix.get()) == 7, "null font matrix produces null-pointer font");
    Font nullCtm(cairo_scaled_font_create(face, &matrix, nullptr, options), cairo_scaled_font_destroy);
    Check(cairo_scaled_font_status(nullCtm.get()) == 7, "null ctm produces null-pointer font");
    CairoMatrix undefined = matrix;
    undefined.xx = std::numeric_limits<double>::quiet_NaN();
    Font invalidBeforeNull(cairo_scaled_font_create(face, &undefined, nullptr, options), cairo_scaled_font_destroy);
    Check(cairo_scaled_font_status(invalidBeforeNull.get()) == 5, "font matrix is validated before ctm");
    Font faceBeforeNull(cairo_scaled_font_create(cairo_scaled_font_get_font_face(bad.get()), nullptr, nullptr,
        options), cairo_scaled_font_destroy);
    Check(cairo_scaled_font_status(faceBeforeNull.get()) == 1, "face status precedes null arguments");
    Rejects([&] { cairo_scaled_font_create(face, reinterpret_cast<const CairoMatrix*>(1), &ctm, options); });
    Rejects([&] { cairo_scaled_font_create(face, &matrix, reinterpret_cast<const CairoMatrix*>(1), options); });
    cairo_font_options_destroy(options);
}

void Conversion(const char* text, int length, int glyphCapacity, int clusterCapacity, bool withClusters) {
    const auto expected = Reference::Convert(matrix, ctm, values, text, length, glyphCapacity, clusterCapacity, withClusters);
    auto font = Create();
    std::vector<CairoGlyph> storage(glyphCapacity > 0 ? glyphCapacity : 1);
    std::vector<CairoTextCluster> clusterStorage(clusterCapacity > 0 ? clusterCapacity : 1);
    auto* originalGlyphs = glyphCapacity == 0 ? nullptr : storage.data();
    auto* originalClusters = clusterCapacity == 0 ? nullptr : clusterStorage.data();
    auto* glyphs = originalGlyphs;
    auto* clusters = originalClusters;
    int glyphCount = glyphCapacity, clusterCount = clusterCapacity, flags = 99;
    errno = EDOM;
    const int status = cairo_scaled_font_text_to_glyphs(font.get(), 3.25, 21.5, text, length,
        &glyphs, &glyphCount, withClusters ? &clusters : nullptr, &clusterCount, &flags);
    Check(errno == EDOM, "conversion preserves errno");
    Check(status == expected.status && glyphCount == expected.glyphCount && flags == expected.flags,
        "conversion status, count and flags match native");
    if (withClusters) Check(clusterCount == expected.clusterCount, "cluster count matches native");
    Check((glyphs != originalGlyphs) == expected.glyphsReplaced, "glyph storage reuse matches native");
    if (withClusters) Check((clusters != originalClusters) == expected.clustersReplaced, "cluster storage reuse matches native");
    if (status == 0) {
        for (int i = 0; i < glyphCount; ++i) {
            const auto& want = expected.glyphs[static_cast<std::size_t>(i)];
            Check(glyphs[i].index == want.index && glyphs[i].x == want.x && glyphs[i].y == want.y,
                "glyph indices and positions match native");
        }
        if (withClusters) for (int i = 0; i < clusterCount; ++i) {
            const auto& want = expected.clusters[static_cast<std::size_t>(i)];
            Check(clusters[i].numBytes == want.numBytes && clusters[i].numGlyphs == want.numGlyphs,
                "UTF-8 cluster mapping matches native");
        }
        CairoTextExtents extents{};
        cairo_scaled_font_glyph_extents(font.get(), glyphs, glyphCount, &extents);
        const auto want = Reference::Extents(matrix, ctm, values, expected.glyphs);
        Check(std::bit_cast<std::array<double, 6>>(extents) == std::bit_cast<std::array<double, 6>>(want),
            "glyph extents match native");
        auto* surface = cairo_image_surface_create(0, 96, 64);
        auto* context = cairo_create(surface);
        cairo_set_scaled_font(context, font.get());
        Check(Reference::ContextStatus(context) == 0, "created scaled font attaches to guest context");
        cairo_show_glyphs(context, glyphs, glyphCount);
        Check(Reference::ContextStatus(context) == 0, "produced glyphs draw on guest surface");
        cairo_destroy(context);
        cairo_surface_destroy(surface);
    }
    if (glyphs != originalGlyphs) cairo_glyph_free(glyphs);
    if (withClusters && clusters != originalClusters) std::free(clusters);
}

void Conversions() {
    for (bool clusters : {false, true}) {
        for (int capacity : {0, 1, 64}) {
            Conversion("A gW", -1, capacity, capacity, clusters);
            Conversion("\xc3\xa9 \xe4\xb8\xad", -1, capacity, capacity, clusters);
            Conversion("", -1, capacity, capacity, clusters);
            Conversion(nullptr, -1, capacity, capacity, clusters);
            Conversion("A\0B", 3, capacity, capacity, clusters);
            Conversion("A gW", 2, capacity, capacity, clusters);
            Conversion("\xc0\xaf", 2, capacity, capacity, clusters);
            Conversion("\xe2\x82", 2, capacity, capacity, clusters);
            Conversion("text", -2, capacity, capacity, clusters);
        }
        Conversion("A gW", -1, 0, 64, clusters);
        Conversion("A gW", -1, 64, 0, clusters);
        Conversion("text", -1, -1, 0, clusters);
        Conversion("text", -1, 0, -1, clusters);
    }
}

int finalized;

void FlushScaledFonts() {
    Reference::FlushScaledFonts();
    auto* surface = cairo_image_surface_create(0, 8, 8);
    auto* context = cairo_create(surface);
    cairo_select_font_face(context, "monospace", 1, 1);
    auto* face = Reference::Face(context);
    auto* options = cairo_font_options_create();
    const CairoMatrix identity{1, 0, 0, 1, 0, 0};
    for (int i = 0; i < 300; ++i) {
        const CairoMatrix scale{1.0 + i, 0, 0, 1.0 + i, 0, 0};
        cairo_scaled_font_destroy(cairo_scaled_font_create(face, &scale, &identity, options));
    }
    cairo_font_options_destroy(options);
    cairo_destroy(context);
    cairo_surface_destroy(surface);
    Reference::FlushScaledFonts();
}

void APS5_VABI Finalized(void* data) {
    finalized += *static_cast<int*>(data);
}

void Faces() {
    int value = 3;
    CairoUserDataKey key{};
    auto* surface = cairo_image_surface_create(0, 96, 64);
    auto* context = cairo_create(surface);
    cairo_select_font_face(context, "serif", 0, 0);
    auto* face = Reference::Face(context);
    Check(Reference::Face(context) == face, "native face keeps one guest handle");
    auto* options = Options();
    Font font(cairo_scaled_font_create(face, &matrix, &ctm, options), cairo_scaled_font_destroy);
    Check(cairo_scaled_font_status(font.get()) == 0, "guest handle creates a scaled font");
    Check(cairo_scaled_font_get_font_face(font.get()) == face, "scaled font returns the face it was created with");
    const auto references = Reference::FaceReferences(font.get());
    Check(cairo_font_face_reference(face) == face, "reference returns the guest handle");
    Check(Reference::FaceReferences(font.get()) == references + 1, "reference reaches the native face");
    Check(cairo_font_face_set_user_data(face, &key, &value, Finalized) == 0, "native-backed face stores user data");
    Check(cairo_font_face_get_user_data(face, &key) == &value, "native-backed face returns user data");
    cairo_destroy(context);
    cairo_surface_destroy(surface);
    font.reset();
    FlushScaledFonts();
    Check(finalized == 0, "guest reference keeps the native face alive");
    cairo_font_face_destroy(face);
    FlushScaledFonts();
    Check(finalized == 3, "last native release finalizes the guest handle");

    auto bad = Create(CairoMatrix{std::numeric_limits<double>::infinity(), 0, 0, 1, 0, 0});
    auto* nil = cairo_scaled_font_get_font_face(bad.get());
    Check(nil == cairo_scaled_font_get_font_face(bad.get()), "static native face keeps one guest handle");
    Check(cairo_font_face_reference(nil) == nil, "static native face ignores references");
    cairo_font_face_destroy(nil);
    Check(cairo_font_face_set_user_data(nil, &key, &value, Finalized) != 0, "static native face rejects user data");

    FreeType::GuestFaceRec record{};
    auto* unbacked = cairo_ft_font_face_create_for_ft_face(&record, 0);
    Rejects([&] { cairo_scaled_font_create(unbacked, &matrix, &ctm, options); });
    auto* drawSurface = cairo_image_surface_create(0, 96, 64);
    auto* drawContext = cairo_create(drawSurface);
    Rejects([&] { cairo_set_font_face(drawContext, unbacked); });
    cairo_destroy(drawContext);
    cairo_surface_destroy(drawSurface);
    cairo_font_face_destroy(unbacked);
    cairo_font_options_destroy(options);
}

void Contexts() {
    auto* surface = cairo_image_surface_create(0, 96, 64);
    auto* context = cairo_create(surface);
    auto font = Create();
    auto* face = cairo_scaled_font_get_font_face(font.get());
    cairo_select_font_face(context, "monospace", 1, 1);
    Check(Reference::Face(context) != face, "different starting font face");
    cairo_set_font_face(context, face);
    Check(Reference::Face(context) == face, "font face attachment preserves pointer");
    cairo_set_font_face(context, nullptr);
    Check(Reference::ContextStatus(context) == 0, "null face resets context font");
    cairo_scale(context, ctm.xx, ctm.yy);
    cairo_set_scaled_font(context, font.get());
    CairoMatrix usedMatrix{};
    cairo_scaled_font_get_font_matrix(Reference::ContextFont(context), &usedMatrix);
    Check(std::bit_cast<std::array<double, 6>>(usedMatrix) == std::bit_cast<std::array<double, 6>>(matrix),
        "context uses created font matrix");
    Check(Reference::Options(Reference::ContextFont(context)) == values, "context uses created font options");
    font.reset();
    cairo_show_text(context, "Alive");
    Check(Reference::ContextStatus(context) == 0, "context retains font after caller releases it");
    cairo_set_scaled_font(context, nullptr);
    Check(Reference::ContextStatus(context) == 7, "null scaled font sets sticky context error");
    cairo_destroy(context);
    cairo_surface_destroy(surface);
}

void Errors() {
    auto font = Create();
    int glyphCount = 99, clusterCount = 99, flags = 99;
    Check(cairo_scaled_font_text_to_glyphs(font.get(), 0, 0, "A", -1, nullptr, &glyphCount,
        nullptr, &clusterCount, &flags) == 7, "missing glyph output reports null pointer");
    Check(glyphCount == 0 && clusterCount == 0 && flags == 99, "early argument errors reset counts only");
    CairoGlyph* glyphs = nullptr;
    CairoTextCluster* clusters = nullptr;
    Check(cairo_scaled_font_text_to_glyphs(font.get(), 0, 0, "A", -1, &glyphs, &glyphCount,
        &clusters, &clusterCount, nullptr) == 7, "cluster flags are mandatory with clusters");
    Check(cairo_scaled_font_status(font.get()) == 0, "argument errors do not poison font");
    Rejects([&] { cairo_scaled_font_text_to_glyphs(font.get(), 0, 0,
        reinterpret_cast<const char*>(1), -1, &glyphs, &glyphCount, nullptr, nullptr, nullptr); });
    Rejects([&] { cairo_scaled_font_text_to_glyphs(font.get(), 0, 0,
        "A", 1, reinterpret_cast<CairoGlyph**>(1), &glyphCount, nullptr, nullptr, nullptr); });
    Rejects([&] { cairo_scaled_font_text_to_glyphs(font.get(), 0, 0,
        "A", -1, nullptr, reinterpret_cast<int*>(1), nullptr, nullptr, nullptr); });
    Rejects([&] { cairo_scaled_font_text_to_glyphs(font.get(), 0, 0,
        "A", -1, &glyphs, reinterpret_cast<int*>(1), nullptr, nullptr, nullptr); });
    Rejects([&] { cairo_scaled_font_text_to_glyphs(font.get(), 0, 0,
        "A", -1, nullptr, &glyphCount, nullptr, reinterpret_cast<int*>(1), nullptr); });
    glyphs = nullptr;
    glyphCount = 0;
    Check(cairo_scaled_font_text_to_glyphs(font.get(), 0, 0, "A", -1, &glyphs, &glyphCount,
        nullptr, reinterpret_cast<int*>(1), nullptr) == 0 && glyphCount == 1,
        "cluster count is ignored without clusters");
    cairo_glyph_free(glyphs);
    glyphs = nullptr;
    Rejects([&] { cairo_scaled_font_glyph_extents(font.get(), reinterpret_cast<const CairoGlyph*>(1), 1,
        reinterpret_cast<CairoTextExtents*>(1)); });
    auto failed = Create(CairoMatrix{std::numeric_limits<double>::infinity(), 0, 0, 1, 0, 0});
    glyphCount = 17;
    Check(cairo_scaled_font_text_to_glyphs(failed.get(), 0, 0, reinterpret_cast<const char*>(1), -1,
        nullptr, &glyphCount, nullptr, nullptr, nullptr) == 5 && glyphCount == 17,
        "failed font returns before inspecting guest arguments");
    CairoTextExtents empty{};
    cairo_scaled_font_glyph_extents(failed.get(), reinterpret_cast<const CairoGlyph*>(1), 1, &empty);
    Check(std::bit_cast<std::array<double, 6>>(empty) == std::array<double, 6>{},
        "failed font gives empty extents without reading glyphs");
#ifdef _WIN32
    CairoGlyph wide{std::uint64_t{1} << 32, 0, 0};
    Rejects([&] { cairo_scaled_font_glyph_extents(font.get(), &wide, 1, &empty); });
#endif
}

}

int main(int argc, char** argv) {
    Check(argc == 2, "test mode supplied");
    const std::string_view mode = argv[1];
    if (mode == "creation") Creation();
    else if (mode == "conversion") Conversions();
    else if (mode == "contexts") Contexts();
    else if (mode == "errors") Errors();
    else if (mode == "faces") Faces();
    else Check(false, "known test mode");
}

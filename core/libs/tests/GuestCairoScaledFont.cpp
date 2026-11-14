#include "prx/libcairo/include/ScaledFont.hpp"

#include <CairoBackend.h>

#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

using Reference = CairoScaledFont* (APS5_VABI*)(CairoScaledFont*);
using Destroy = void (APS5_VABI*)(CairoScaledFont*);
using Status = int (APS5_VABI*)(CairoScaledFont*);
using GetMatrix = void (APS5_VABI*)(CairoScaledFont*, CairoMatrix*);
using GetExtents = void (APS5_VABI*)(CairoScaledFont*, CairoFontExtents*);
using GetTextExtents = void (APS5_VABI*)(CairoScaledFont*, const char*, CairoTextExtents*);
static_assert(std::is_same_v<decltype(&cairo_scaled_font_reference), Reference>);
static_assert(std::is_same_v<decltype(&cairo_scaled_font_destroy), Destroy>);
static_assert(std::is_same_v<decltype(&cairo_scaled_font_status), Status>);
static_assert(std::is_same_v<decltype(&cairo_scaled_font_get_font_matrix), GetMatrix>);
static_assert(std::is_same_v<decltype(&cairo_scaled_font_get_ctm), GetMatrix>);
static_assert(std::is_same_v<decltype(&cairo_scaled_font_extents), GetExtents>);
static_assert(std::is_same_v<decltype(&cairo_scaled_font_text_extents), GetTextExtents>);

namespace {

void Check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "scaled-font native differential failed: %s\n", reason);
        std::abort();
    }
}

template <class T>
void Compare(const T& guest, const T& native, const char* reason) {
    static_assert(sizeof(T) % sizeof(double) == 0);
    using Values = std::array<double, sizeof(T) / sizeof(double)>;
    const auto guestValues = std::bit_cast<Values>(guest);
    const auto nativeValues = std::bit_cast<Values>(native);
    for (std::size_t i = 0; i < sizeof(T) / sizeof(double); ++i) {
        if (!(guestValues[i] == nativeValues[i])) {
            std::fprintf(stderr, "mismatch %s at %zu: %.17g != %.17g\n",
                         reason, i, guestValues[i], nativeValues[i]);
            std::abort();
        }
    }
}

template <class Callback>
void Rejects(Callback&& callback) {
    const int saved = EDOM;
    errno = saved;
    bool rejected = false;
    try {
        callback();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Check(rejected, "invalid guest pointer goes through existing failure path");
    Check(errno == saved, "invalid guest pointer preserves errno");
}

struct Example {
    const char* family;
    int slant;
    int weight;
    double size;
    double scaleX;
    double scaleY;
    const char* text;
};

void CompareMetricRasters(const CairoTextExtents& guest, const cairo_text_extents_t& native) {
    constexpr int Width = 256;
    constexpr int Height = 96;
    cairo_surface_t* guestSurface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Width, Height);
    cairo_surface_t* nativeSurface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Width, Height);
    Check(guestSurface && nativeSurface, "paired metric surfaces created");
    const auto draw = [](cairo_surface_t* surface, double x, double y, double width, double height) {
        cairo_t* context = aps5_cairo_create(surface);
        aps5_cairo_rectangle(context, 16 + x, 64 + y, width, height);
        aps5_cairo_fill(context);
        Check(aps5_cairo_status(context) == CAIRO_STATUS_SUCCESS, "metric rectangle draw succeeded");
        aps5_cairo_destroy(context);
        aps5_cairo_surface_flush(surface);
    };
    draw(guestSurface, guest.x_bearing, guest.y_bearing, guest.width, guest.height);
    draw(nativeSurface, native.x_bearing, native.y_bearing, native.width, native.height);
    const auto stride = aps5_cairo_image_surface_get_stride(guestSurface);
    Check(stride == aps5_cairo_image_surface_get_stride(nativeSurface), "paired image strides match");
    const std::size_t bytes = static_cast<std::size_t>(stride) * Height;
    Check(std::memcmp(aps5_cairo_image_surface_get_data(guestSurface),
                      aps5_cairo_image_surface_get_data(nativeSurface), bytes) == 0,
          "guest-scaled-font metric raster matches pinned-native metric raster");
    aps5_cairo_surface_destroy(guestSurface);
    aps5_cairo_surface_destroy(nativeSurface);
}

void CompareNativeExample(const Example& example) {
    cairo_surface_t* surface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 300, 120);
    Check(surface != nullptr && aps5_cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS, "surface initialized");
    cairo_t* context = aps5_cairo_create(surface);
    Check(context != nullptr && aps5_cairo_status(context) == CAIRO_STATUS_SUCCESS, "context initialized");
    aps5_cairo_select_font_face(context, example.family, static_cast<cairo_font_slant_t>(example.slant),
                                static_cast<cairo_font_weight_t>(example.weight));
    aps5_cairo_set_font_size(context, example.size);
    aps5_cairo_scale(context, example.scaleX, example.scaleY);
    cairo_scaled_font_t* native = aps5_cairo_get_scaled_font(context);
    Check(native != nullptr, "native scaled font generated by pinned Cairo");
    Check(aps5_cairo_scaled_font_status(native) == CAIRO_STATUS_SUCCESS, "native scaled font is valid");
    auto* guest = reinterpret_cast<CairoScaledFont*>(native);

    Check(cairo_scaled_font_status(guest) == aps5_cairo_scaled_font_status(native),
          "status matches pinned Cairo");
    CairoMatrix guestFontMatrix{}, guestCtm{};
    cairo_matrix_t nativeFontMatrix{}, nativeCtm{};
    cairo_scaled_font_get_font_matrix(guest, &guestFontMatrix);
    aps5_cairo_scaled_font_get_font_matrix(native, &nativeFontMatrix);
    cairo_scaled_font_get_ctm(guest, &guestCtm);
    aps5_cairo_scaled_font_get_ctm(native, &nativeCtm);
    Compare(guestFontMatrix, std::bit_cast<CairoMatrix>(nativeFontMatrix), "font matrix");
    Compare(guestCtm, std::bit_cast<CairoMatrix>(nativeCtm), "ctm");

    CairoFontExtents guestFontExtents{};
    cairo_font_extents_t nativeFontExtents{};
    cairo_scaled_font_extents(guest, &guestFontExtents);
    aps5_cairo_scaled_font_extents(native, &nativeFontExtents);
    Compare(guestFontExtents, std::bit_cast<CairoFontExtents>(nativeFontExtents), "font extents");

    CairoTextExtents guestTextExtents{};
    cairo_text_extents_t nativeTextExtents{};
    cairo_scaled_font_text_extents(guest, example.text, &guestTextExtents);
    aps5_cairo_scaled_font_text_extents(native, example.text, &nativeTextExtents);
    Compare(guestTextExtents, std::bit_cast<CairoTextExtents>(nativeTextExtents), "text extents");
    CompareMetricRasters(guestTextExtents, nativeTextExtents);

    const unsigned int refs = aps5_cairo_scaled_font_get_reference_count(native);
    Check(cairo_scaled_font_reference(guest) == guest, "reference retains pointer identity");
    Check(aps5_cairo_scaled_font_get_reference_count(native) == refs + 1,
          "guest reference increments native refcount");
    cairo_scaled_font_destroy(guest);
    Check(aps5_cairo_scaled_font_get_reference_count(native) == refs,
          "guest destroy decrements native refcount");

    if (example.text && example.text[0] && example.size >= 8 &&
        example.scaleX == 1 && example.scaleY == 1) {
        aps5_cairo_move_to(context, 8, 85);
        aps5_cairo_show_text(context, example.text);
        double x = -1, y = -1;
        aps5_cairo_get_current_point(context, &x, &y);
        Check(std::isfinite(x) && std::isfinite(y), "text cursor finite");
        Check(std::abs(x - (8.0 + nativeTextExtents.x_advance)) < 1e-7,
              "native text ink/advance agrees with scaled-font text metrics");
        aps5_cairo_surface_flush(surface);
        const auto* pixels = aps5_cairo_image_surface_get_data(surface);
        std::size_t nonzero = 0;
        for (std::size_t i = 0; i < 300u * 120u * 4u; ++i)
            nonzero += pixels[i] != 0;
        Check(nonzero > 0, "source-rendered text produced nonempty pixels");
    }

    if (example.text && example.text[0]) {
        CairoTextExtents empty{};
        cairo_scaled_font_text_extents(guest, nullptr, &empty);
        for (double value : {empty.x_bearing, empty.y_bearing, empty.width, empty.height,
                             empty.x_advance, empty.y_advance})
            Check(value == 0, "null text zero extents match pinned Cairo");
    }

    const auto invalid = reinterpret_cast<void*>(std::uintptr_t{1});
    Rejects([&] { cairo_scaled_font_get_font_matrix(guest, reinterpret_cast<CairoMatrix*>(invalid)); });
    Rejects([&] { cairo_scaled_font_get_ctm(guest, nullptr); });
    Rejects([&] { cairo_scaled_font_extents(guest, reinterpret_cast<CairoFontExtents*>(invalid)); });
    Rejects([&] { cairo_scaled_font_text_extents(guest, example.text, nullptr); });
    Rejects([&] { cairo_scaled_font_text_extents(guest, reinterpret_cast<const char*>(invalid),
                                                &guestTextExtents); });
    Check(cairo_scaled_font_status(guest) == CAIRO_STATUS_SUCCESS,
          "bad guest pointers leave native scaled-font state intact");

    cairo_scaled_font_destroy(nullptr);
    Check(cairo_scaled_font_reference(nullptr) == nullptr, "null native-reference is a no-op");
    Rejects([&] { cairo_scaled_font_status(nullptr); });
    Rejects([&] { cairo_scaled_font_get_ctm(nullptr, &guestCtm); });

    aps5_cairo_destroy(context);
    aps5_cairo_surface_destroy(surface);
}

void CheckUnterminated() {
    cairo_surface_t* surface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
    cairo_t* context = aps5_cairo_create(surface);
    aps5_cairo_select_font_face(context, "sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_scaled_font_t* native = aps5_cairo_get_scaled_font(context);
    auto* guest = reinterpret_cast<CairoScaledFont*>(native);
    std::vector<char> unterminated(1024 * 1024, 'x');
    CairoTextExtents extents{};
    Rejects([&] { cairo_scaled_font_text_extents(guest, unterminated.data(), &extents); });
    Check(aps5_cairo_scaled_font_status(native) == CAIRO_STATUS_SUCCESS,
          "unterminated UTF-8 did not reach native scaled-font parser");
    aps5_cairo_destroy(context);
    aps5_cairo_surface_destroy(surface);
}

}

int main() {
    constexpr std::array<Example, 9> examples{{
        {"sans", 0, 0, 12, 1, 1, "Cairo"},
        {"sans", 1, 1, 21, 1.5, 1.25, "AnyPS5"},
        {"sans", 2, 0, 18, 0.75, 1.75, "metrics"},
        {"serif", 0, 1, 20, 1, 1, "WXYZ"},
        {"serif", 1, 0, 15.5, 2, 0.5, "glyphs"},
        {"monospace", 0, 1, 19, 1, 1, "012345"},
        {"", 0, 0, 13, 1.25, 1.25, "fallback"},
        {"sans", 0, 0, 10, 1, 1, ""},
        {"sans", 0, 0, 15, 1, 1, nullptr},
    }};
    for (const Example& example : examples) CompareNativeExample(example);
    CheckUnterminated();
    std::puts("guest Cairo scaled-font native metrics, pixels and guest-pointer tests passed");
}

#include "prx/libcairo/include/Path.hpp"

#include <CairoBackend.h>

#include <array>
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

using SelectFace = void (APS5_VABI*)(CairoContext*, const char*, int, int);
using SetSize = void (APS5_VABI*)(CairoContext*, double);
using ShowText = void (APS5_VABI*)(CairoContext*, const char*);
static_assert(std::is_same_v<decltype(&cairo_select_font_face), SelectFace>);
static_assert(std::is_same_v<decltype(&cairo_set_font_size), SetSize>);
static_assert(std::is_same_v<decltype(&cairo_show_text), ShowText>);

namespace {

struct Case {
    const char* family;
    int slant;
    int weight;
    double size;
    const char* text;
};

void Check(bool result, const char* message) {
    if (!result) {
        std::fprintf(stderr, "Cairo toy-text comparison failed: %s\n", message);
        std::abort();
    }
}

void Compare(const Case& test) {
    constexpr int Width = 256;
    constexpr int Height = 96;
    CairoSurface* guestSurface = cairo_image_surface_create(0, Width, Height);
    cairo_surface_t* nativeSurface = aps5_cairo_image_surface_create(CAIRO_FORMAT_ARGB32, Width, Height);
    Check(guestSurface != nullptr && nativeSurface != nullptr, "surfaces allocated");
    Check(cairo_surface_status(guestSurface) == CAIRO_STATUS_SUCCESS &&
          aps5_cairo_surface_status(nativeSurface) == CAIRO_STATUS_SUCCESS, "surface creation");

    CairoContext* guest = cairo_create(guestSurface);
    cairo_t* native = aps5_cairo_create(nativeSurface);
    Check(guest != nullptr && native != nullptr, "contexts allocated");

    cairo_select_font_face(guest, test.family, test.slant, test.weight);
    aps5_cairo_select_font_face(native, test.family, static_cast<cairo_font_slant_t>(test.slant),
                                static_cast<cairo_font_weight_t>(test.weight));
    cairo_set_font_size(guest, test.size);
    aps5_cairo_set_font_size(native, test.size);

    cairo_move_to(guest, 8.5, 52.25);
    aps5_cairo_move_to(native, 8.5, 52.25);
    cairo_show_text(guest, test.text);
    aps5_cairo_show_text(native, test.text);

    double xGuest = -1000, yGuest = -1000, xNative = -2000, yNative = -2000;
    cairo_get_current_point(guest, &xGuest, &yGuest);
    aps5_cairo_get_current_point(native, &xNative, &yNative);
    Check(std::abs(xGuest - xNative) < 1e-10 && std::abs(yGuest - yNative) < 1e-10,
          "text advances current point exactly as native Cairo");
    Check(aps5_cairo_status(native) == (test.family ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_NULL_POINTER),
          "native font-family status");
    cairo_surface_flush(guestSurface);
    aps5_cairo_surface_flush(nativeSurface);

    const int guestStride = cairo_image_surface_get_stride(guestSurface);
    const int nativeStride = aps5_cairo_image_surface_get_stride(nativeSurface);
    Check(guestStride == nativeStride && guestStride == Width * 4, "image stride");
    const std::size_t bytes = static_cast<std::size_t>(guestStride) * Height;
    const auto* guestPixels = cairo_image_surface_get_data(guestSurface);
    const auto* nativePixels = aps5_cairo_image_surface_get_data(nativeSurface);
    Check(guestPixels && nativePixels, "image data");
    Check(std::memcmp(guestPixels, nativePixels, bytes) == 0,
          "all raster pixels match independently executed pinned Cairo");
    if (test.family && test.text && test.text[0] != '\0' && test.size >= 12) {
        std::size_t nonzero = 0;
        for (std::size_t index = 0; index < bytes; ++index) nonzero += guestPixels[index] != 0;
        Check(nonzero > 0, "nonempty text modifies the image");
    }

    cairo_destroy(guest);
    aps5_cairo_destroy(native);
    cairo_surface_destroy(guestSurface);
    aps5_cairo_surface_destroy(nativeSurface);
}

void CheckRejectedGuestStrings() {
    CairoSurface* surface = cairo_image_surface_create(0, 160, 64);
    CairoContext* context = cairo_create(surface);
    Check(context != nullptr, "context for invalid guest strings");
    const auto invalid = reinterpret_cast<const char*>(std::uintptr_t{1});
    const auto rejects = [](auto callback) {
        errno = EDOM;
        bool rejected = false;
        try {
            callback();
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Check(rejected, "unreadable guest string rejected without host fault");
        Check(errno == EDOM, "invalid string does not alter guest errno");
    };
    rejects([&] { cairo_select_font_face(context, invalid, 0, 0); });
    rejects([&] { cairo_show_text(context, invalid); });

    std::vector<char> unterminated(1024 * 1024, 'x');
    rejects([&] { cairo_show_text(context, unterminated.data()); });
    rejects([&] { cairo_select_font_face(context, unterminated.data(), 0, 0); });

    cairo_select_font_face(context, "sans", 0, 0);
    cairo_set_font_size(context, 18);
    cairo_move_to(context, 8, 38);
    cairo_show_text(context, "recovery");
    cairo_surface_flush(surface);
    const auto* pixels = cairo_image_surface_get_data(surface);
    std::size_t nonzero = 0;
    for (std::size_t i = 0; i < 160u * 64u * 4u; ++i) nonzero += pixels[i] != 0;
    Check(nonzero != 0, "failed guest strings do not poison valid rendering");
    cairo_destroy(context);
    cairo_surface_destroy(surface);
}

} // namespace

int main() {
    constexpr std::array<Case, 14> cases{{
        {"sans", 0, 0, 12, "AnyPS5"},
        {"sans", 1, 1, 22, "Cairo 1.18"},
        {"sans", 2, 0, 17.75, "PS5 text"},
        {"serif", 0, 0, 28, "Hello!"},
        {"serif", 1, 1, 14.5, "italic"},
        {"monospace", 0, 1, 19, "0123456789"},
        {"monospace", 2, 0, 13, "WXYZ"},
        {"", 0, 0, 16, "default"},
        {"missing-font-family-ps5", 0, 0, 20, "fallback"},
        {"sans", 0, 0, 1, "tiny"},
        {"sans", 0, 0, 0, ""},
        {"sans", 0, 0, 23, "two words"},
        {"sans", 0, 0, 18, nullptr},
        {nullptr, 0, 0, 18, "null family"},
    }};
    for (const Case& test : cases) Compare(test);
    CheckRejectedGuestStrings();
    std::puts("guest Cairo toy-text native-source comparisons passed");
}

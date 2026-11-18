#include "prx/libcairo/include/ScaledFont.hpp"

#include <CairoBackend.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>
#include <type_traits>

extern "C" void _cairo_mutex_initialize();

static_assert(std::is_same_v<decltype(&cairo_ft_scaled_font_lock_face),
                             FreeType::GuestFaceRec* (APS5_VABI*)(CairoScaledFont*)>);
static_assert(std::is_same_v<decltype(&cairo_ft_scaled_font_unlock_face), void (APS5_VABI*)(CairoScaledFont*)>);

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "cairo ft scaled font: %s\n", message);
    std::abort();
}

template <class Callback>
bool Throws(Callback&& callback) {
    try {
        callback();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

}

int main() {
    _cairo_mutex_initialize();
    cairo_font_face_t* toy = aps5_cairo_toy_font_face_create("", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_matrix_t matrix;
    aps5_cairo_matrix_init_scale(&matrix, 12, 12);
    cairo_matrix_t identity;
    aps5_cairo_matrix_init_identity(&identity);
    cairo_matrix_t singular;
    aps5_cairo_matrix_init(&singular, 1, 1, 1, 1, 0, 0);
    cairo_font_options_t* options = aps5_cairo_font_options_create();
    cairo_scaled_font_t* native = aps5_cairo_scaled_font_create(toy, &matrix, &identity, options);
    cairo_scaled_font_t* failed = aps5_cairo_scaled_font_create(toy, &singular, &identity, options);
    Require(aps5_cairo_scaled_font_status(native) == CAIRO_STATUS_SUCCESS, "toy scaled font");
    Require(aps5_cairo_scaled_font_status(failed) != CAIRO_STATUS_SUCCESS, "error scaled font");

    for (cairo_scaled_font_t* font : {native, failed}) {
        auto* guest = reinterpret_cast<CairoScaledFont*>(font);
        const auto status = aps5_cairo_scaled_font_status(font);
        errno = EDOM;
        Require(cairo_ft_scaled_font_lock_face(guest) == nullptr, "a non-FreeType font has no face to lock");
        cairo_ft_scaled_font_unlock_face(guest);
        Require(errno == EDOM, "lock and unlock keep errno");
        Require(aps5_cairo_scaled_font_status(font) == status, "a type mismatch is not stored on the font");
    }
    Require(Throws([] { cairo_ft_scaled_font_lock_face(nullptr); }), "null font throws on lock");
    Require(Throws([] { cairo_ft_scaled_font_unlock_face(nullptr); }), "null font throws on unlock");

    aps5_cairo_scaled_font_destroy(failed);
    aps5_cairo_scaled_font_destroy(native);
    aps5_cairo_font_options_destroy(options);
    aps5_cairo_font_face_destroy(toy);
    std::puts("cairo ft scaled font PASS");
    return EXIT_SUCCESS;
}

#include "prx/libcairo/include/ScaledFont.hpp"
#include "prx/libc/include/General.hpp"

#include <CairoBackend.h>

#include <cerrno>
#include <cstddef>
#include <string>

static_assert(sizeof(CairoMatrix) == sizeof(cairo_matrix_t));
static_assert(offsetof(CairoMatrix, yx) == offsetof(cairo_matrix_t, yx));
static_assert(offsetof(CairoMatrix, xy) == offsetof(cairo_matrix_t, xy));
static_assert(offsetof(CairoMatrix, y0) == offsetof(cairo_matrix_t, y0));
static_assert(sizeof(CairoFontExtents) == sizeof(cairo_font_extents_t));
static_assert(offsetof(CairoFontExtents, max_x_advance) == offsetof(cairo_font_extents_t, max_x_advance));
static_assert(offsetof(CairoFontExtents, max_y_advance) == offsetof(cairo_font_extents_t, max_y_advance));
static_assert(sizeof(CairoTextExtents) == sizeof(cairo_text_extents_t));
static_assert(offsetof(CairoTextExtents, x_advance) == offsetof(cairo_text_extents_t, x_advance));
static_assert(offsetof(CairoTextExtents, y_advance) == offsetof(cairo_text_extents_t, y_advance));

namespace {

constexpr std::size_t MaximumGuestText = 1024 * 1024;

cairo_scaled_font_t* Backend(CairoScaledFont* font) {
    if (font == nullptr) NotImplementedAt_nid_no_patch("cairo_scaled_font: null handle", __FILE__, __LINE__);
    return reinterpret_cast<cairo_scaled_font_t*>(font);
}

void CopyResult(void* destination, const void* source, std::size_t size, const char* name) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, size);
    errno = savedErrno;
    if (!copied) NotImplementedAt_nid_no_patch(name, __FILE__, __LINE__);
}

const char* CheckedText(const char* guest, std::string& copy) {
    if (guest == nullptr) return nullptr;
    const int savedErrno = errno;
    const int copied = CopyGuestString_nid_no_patch(guest, MaximumGuestText, copy);
    errno = savedErrno;
    if (copied != 0) NotImplementedAt_nid_no_patch("cairo_scaled_font_text_extents: unreadable or unterminated UTF-8",
                                                   __FILE__, __LINE__);
    return copy.c_str();
}

}

extern "C" {

CairoScaledFont* APS5_VABI cairo_scaled_font_reference(CairoScaledFont* font) {
    return reinterpret_cast<CairoScaledFont*>(
        aps5_cairo_scaled_font_reference(reinterpret_cast<cairo_scaled_font_t*>(font)));
}

void APS5_VABI cairo_scaled_font_destroy(CairoScaledFont* font) {
    aps5_cairo_scaled_font_destroy(reinterpret_cast<cairo_scaled_font_t*>(font));
}

int APS5_VABI cairo_scaled_font_status(CairoScaledFont* font) {
    return aps5_cairo_scaled_font_status(Backend(font));
}

void APS5_VABI cairo_scaled_font_get_font_matrix(CairoScaledFont* font, CairoMatrix* matrix) {
    cairo_matrix_t native{};
    aps5_cairo_scaled_font_get_font_matrix(Backend(font), &native);
    CopyResult(matrix, &native, sizeof(native), "cairo_scaled_font_get_font_matrix: invalid output");
}

void APS5_VABI cairo_scaled_font_get_ctm(CairoScaledFont* font, CairoMatrix* ctm) {
    cairo_matrix_t native{};
    aps5_cairo_scaled_font_get_ctm(Backend(font), &native);
    CopyResult(ctm, &native, sizeof(native), "cairo_scaled_font_get_ctm: invalid output");
}

void APS5_VABI cairo_scaled_font_extents(CairoScaledFont* font, CairoFontExtents* extents) {
    cairo_font_extents_t native{};
    aps5_cairo_scaled_font_extents(Backend(font), &native);
    CopyResult(extents, &native, sizeof(native), "cairo_scaled_font_extents: invalid output");
}

void APS5_VABI cairo_scaled_font_text_extents(CairoScaledFont* font, const char* utf8,
                                              CairoTextExtents* extents) {
    auto* nativeFont = Backend(font);
    std::string text;
    const char* checkedUtf8 = CheckedText(utf8, text);
    cairo_text_extents_t native{};
    aps5_cairo_scaled_font_text_extents(nativeFont, checkedUtf8, &native);
    CopyResult(extents, &native, sizeof(native), "cairo_scaled_font_text_extents: invalid output");
}

}

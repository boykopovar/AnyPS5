#include "prx/libcairo/include/Context.hpp"
#include "prx/libc/include/General.hpp"

#include <CairoBackend.h>

#include <cerrno>
#include <string>

namespace {

constexpr std::size_t MaximumGuestString = 1024 * 1024;

const char* CheckedString(const char* guest, const char* name, std::string& copy) {
    if (!guest) return nullptr;
    const int previousErrno = errno;
    const int result = CopyGuestString_nid_no_patch(guest, MaximumGuestString, copy);
    errno = previousErrno;
    if (result != 0) NotImplementedAt_nid_no_patch(name, __FILE__, __LINE__);
    return copy.c_str();
}

}

extern "C" {

void APS5_VABI cairo_select_font_face(CairoContext* context, const char* family, int slant, int weight) {
    std::string copy;
    aps5_cairo_select_font_face(reinterpret_cast<cairo_t*>(context),
                               CheckedString(family, "cairo_select_font_face: unreadable or unterminated family", copy),
                               static_cast<cairo_font_slant_t>(slant), static_cast<cairo_font_weight_t>(weight));
}

void APS5_VABI cairo_set_font_size(CairoContext* context, double size) {
    aps5_cairo_set_font_size(reinterpret_cast<cairo_t*>(context), size);
}

void APS5_VABI cairo_show_text(CairoContext* context, const char* utf8) {
    std::string copy;
    aps5_cairo_show_text(reinterpret_cast<cairo_t*>(context),
                         CheckedString(utf8, "cairo_show_text: unreadable or unterminated UTF-8", copy));
}

}

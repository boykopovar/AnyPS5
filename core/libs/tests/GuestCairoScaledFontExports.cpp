#include "prx/libcairo/include/ScaledFont.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

static_assert(std::is_same_v<decltype(&cairo_scaled_font_reference), CairoScaledFont* (APS5_VABI*)(CairoScaledFont*)>);
static_assert(std::is_same_v<decltype(&cairo_scaled_font_destroy), void (APS5_VABI*)(CairoScaledFont*)>);

int main() {
    errno = EDOM;
    if (cairo_scaled_font_reference(nullptr) != nullptr || errno != EDOM) {
        std::fputs("scaled font PRX null-reference ABI failed\n", stderr);
        return EXIT_FAILURE;
    }
    cairo_scaled_font_destroy(nullptr);
    if (errno != EDOM) {
        std::fputs("scaled font PRX null-destroy ABI failed\n", stderr);
        return EXIT_FAILURE;
    }
    std::puts("scaled font exported PRX null-lifecycle ABI PASS");
    return EXIT_SUCCESS;
}

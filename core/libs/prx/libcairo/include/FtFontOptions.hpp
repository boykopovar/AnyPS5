#pragma once

#include "prx/libcairo/include/InventoryExports.hpp"

#include "prx/libfontconfig/include/Types.hpp"

extern "C" {

void APS5_VABI cairo_ft_font_options_substitute(const cairo_font_options_t* options, FcPattern* pattern);

}

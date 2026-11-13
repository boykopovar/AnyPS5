#pragma once

#include "SceTypes.hpp"

struct hb_font_t;
struct hb_buffer_t;

namespace HarfBuzz {

struct Feature {
    std::uint32_t tag;
    std::uint32_t value;
    std::uint32_t start;
    std::uint32_t end;
};

}

extern "C" {

void APS5_VABI hb_shape_nid_postfix(hb_font_t* font, hb_buffer_t* buffer, const HarfBuzz::Feature* features, unsigned numFeatures);

}

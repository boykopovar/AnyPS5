#include "prx/libharfbuzz/include/Shaping.hpp"
#include "prx/libc/include/General.hpp"

#include <hb.h>

#include <cstddef>
#include <type_traits>
#include <vector>

static_assert(sizeof(HarfBuzz::Feature) == 16 && alignof(HarfBuzz::Feature) == 4);
static_assert(sizeof(hb_feature_t) == 16 && alignof(hb_feature_t) == 4);
static_assert(offsetof(hb_feature_t, tag) == 0 && offsetof(HarfBuzz::Feature, tag) == 0);
static_assert(offsetof(hb_feature_t, value) == 4 && offsetof(HarfBuzz::Feature, value) == 4);
static_assert(offsetof(hb_feature_t, start) == 8 && offsetof(HarfBuzz::Feature, start) == 8);
static_assert(offsetof(hb_feature_t, end) == 12 && offsetof(HarfBuzz::Feature, end) == 12);
static_assert(std::is_same_v<hb_tag_t, std::uint32_t> && sizeof(unsigned) == 4);

extern "C" {

void APS5_VABI hb_shape_nid_postfix(hb_font_t* font, hb_buffer_t* buffer, const HarfBuzz::Feature* features, unsigned numFeatures) {
    if (!buffer) NotImplemented_nid_no_patch(__func__);
    if (!hb_buffer_get_length(buffer)) return;
    if (!font || (!features && numFeatures)) NotImplemented_nid_no_patch(__func__);
    std::vector<hb_feature_t> nativeFeatures;
    nativeFeatures.reserve(numFeatures);
    for (unsigned i = 0; i < numFeatures; ++i) {
        nativeFeatures.push_back({features[i].tag, features[i].value, features[i].start, features[i].end});
    }
    hb_shape(font, buffer, nativeFeatures.data(), numFeatures);
}

}

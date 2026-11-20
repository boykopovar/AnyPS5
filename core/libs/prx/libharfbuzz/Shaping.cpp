#include "prx/libharfbuzz/include/Shaping.hpp"
#include "prx/libc/include/General.hpp"

#include <hb.h>

#include <algorithm>
#include <array>
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
    std::array<HarfBuzz::Feature, 256> chunk;
    for (unsigned done = 0; done < numFeatures;) {
        const unsigned count = std::min<unsigned>(static_cast<unsigned>(chunk.size()), numFeatures - done);
        if (!GuestCopyIn_nid_no_patch(chunk.data(), features + done, count * sizeof(HarfBuzz::Feature))) NotImplemented_nid_no_patch(__func__);
        for (unsigned index = 0; index < count; ++index) {
            const auto& feature = chunk[index];
            nativeFeatures.push_back({feature.tag, feature.value, feature.start, feature.end});
        }
        done += count;
    }
    hb_shape(font, buffer, nativeFeatures.data(), numFeatures);
}

}

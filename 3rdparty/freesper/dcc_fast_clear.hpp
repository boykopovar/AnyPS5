// Imported from the local freesper snapshot; see PROVENANCE.md.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
namespace prosper::gpu {
inline bool gfx10_dcc_fast_clear_rgba8(uint8_t* dst, size_t texel_count,
                                const uint8_t* metadata, size_t metadata_bytes,
                                uint32_t num_components, bool alpha_is_on_msb,
                                uint8_t* clear_code) {
    if (!metadata || !metadata_bytes || num_components < 1 || num_components > 4 ||
        (!dst && texel_count))
        return false;
    const uint8_t code = metadata[0];
    if (code != 0x00 && code != 0x40 && code != 0x80 && code != 0xc0)
        return false;
    if (!std::all_of(metadata + 1, metadata + metadata_bytes,
                     [=](uint8_t value) { return value == code; }))
        return false;

    // 0x40 / 0x80 give colour and alpha different values (0001 / 1110). Which narrow component is
    // the alpha channel is the descriptor's call, and a one- or two-component surface has no
    // alpha to give it a meaning, so only the codes where colour == alpha (0x00, 0xc0) are
    // materialized there; the others stay refused instead of guessing (#4699 review B1).
    if (num_components < 3 && (code == 0x40 || code == 0x80)) return false;
    const uint8_t color = (code == 0x80 || code == 0xc0) ? 255 : 0;
    uint8_t pixel[4] = {color, color, color, 255};
    // One- and two-component surfaces: the clear colour fills the components that exist, and the
    // absent ones read the sampled-format default (0,0,0,1) like every other narrow decode here.
    if (num_components == 1) {
        pixel[1] = 0;
        pixel[2] = 0;
    } else if (num_components == 2) {
        pixel[2] = 0;
    }
    if (num_components == 4) {
        const uint8_t alpha = (code == 0x40 || code == 0xc0) ? 255 : 0;
        const uint32_t alpha_component = alpha_is_on_msb ? 3u : 0u;
        std::fill(pixel, pixel + 4, color);
        pixel[alpha_component] = alpha;
    }
    for (size_t i = 0; i < texel_count; ++i)
        std::memcpy(dst + i * 4, pixel, sizeof(pixel));
    if (clear_code) *clear_code = code;
    return true;
}

}

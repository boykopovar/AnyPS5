#ifndef CORE_LIBS_PRX_LIBSCEAUDIO3D_MIXER_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIO3D_MIXER_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Audio3dInternal {

struct Source {
    const void* samples;
    std::uint32_t num_samples;
    std::uint32_t channels;
    bool is_float;
    float gain;
    float pan;
};

constexpr float kSurroundFold = 0.7071f;
constexpr std::uint32_t kChannelL = 0;
constexpr std::uint32_t kChannelR = 1;
constexpr std::uint32_t kChannelCenter = 2;
[[maybe_unused]] constexpr std::uint32_t kChannelLfe = 3;
constexpr std::uint32_t kChannelLs = 4;
constexpr std::uint32_t kChannelRs = 5;
constexpr std::uint32_t kChannelLrs = 6;
constexpr std::uint32_t kChannelRrs = 7;

inline float ReadSample(const Source& source, std::uint32_t frame, std::uint32_t channel) {
    const std::size_t index = static_cast<std::size_t>(frame) * source.channels + channel;
    if (source.is_float) return static_cast<const float*>(source.samples)[index];
    return static_cast<float>(static_cast<const std::int16_t*>(source.samples)[index]) / 32768.0f;
}

inline void MixFrame(const Source* sources, std::size_t count, std::uint32_t granularity, std::vector<std::int16_t>& out) {
    std::vector<float> mix(static_cast<std::size_t>(granularity) * 2, 0.0f);
    for (std::size_t source_index = 0; source_index < count; ++source_index) {
        const Source& src = sources[source_index];
        if (src.samples == nullptr || src.num_samples == 0) continue;
        const float pan = std::clamp(src.pan, -1.0f, 1.0f);
        const float gl = (pan <= 0.0f ? 1.0f : 1.0f - pan) * src.gain;
        const float gr = (pan >= 0.0f ? 1.0f : 1.0f + pan) * src.gain;
        for (std::uint32_t frame = 0; frame < granularity && frame < src.num_samples; ++frame) {
            const std::size_t left_index = static_cast<std::size_t>(frame) * 2;
            float left = ReadSample(src, frame, kChannelL);
            float right = src.channels == 1 ? left : ReadSample(src, frame, kChannelR);
            if (src.channels > kChannelCenter) {
                const float center = ReadSample(src, frame, kChannelCenter) * kSurroundFold;
                left += center;
                right += center;
            }
            if (src.channels > kChannelLs) left += ReadSample(src, frame, kChannelLs) * kSurroundFold;
            if (src.channels > kChannelRs) right += ReadSample(src, frame, kChannelRs) * kSurroundFold;
            if (src.channels > kChannelLrs) left += ReadSample(src, frame, kChannelLrs) * kSurroundFold;
            if (src.channels > kChannelRrs) right += ReadSample(src, frame, kChannelRrs) * kSurroundFold;
            mix[left_index] += left * gl;
            mix[left_index + 1] += right * gr;
        }
    }
    out.resize(static_cast<std::size_t>(granularity) * 2);
    for (std::size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<std::int16_t>(std::lround(std::clamp(mix[i], -1.0f, 1.0f) * 32767.0f));
    }
}

}

#endif

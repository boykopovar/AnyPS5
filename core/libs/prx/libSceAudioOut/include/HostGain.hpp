#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_HOSTGAIN_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_INCLUDE_HOSTGAIN_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

class HostGainRamp {
public:
    static constexpr std::uint32_t Unity = 65536;
    static constexpr std::uint32_t DefaultRampFrames = 480;

    explicit HostGainRamp(std::uint32_t rampFrames = DefaultRampFrames) {
        Configure(rampFrames);
    }

    void Configure(std::uint32_t rampFrames) {
        step = rampFrames == 0 ? Unity : std::max<std::uint32_t>(1, Unity / rampFrames);
    }

    std::uint32_t Current() const { return current; }
    std::uint32_t Step() const { return step; }

    bool Bypass(std::uint32_t target) const {
        return current == Unity && target == Unity;
    }

    void Apply(std::int16_t* samples, std::size_t frames, unsigned channels, std::uint32_t target) {
        if (Bypass(target)) return;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const std::int64_t gain = advance(target);
            for (unsigned channel = 0; channel < channels; ++channel) {
                std::int16_t& sample = samples[frame * channels + channel];
                const std::int64_t scaled = (static_cast<std::int64_t>(sample) * gain + Unity / 2) >> 16;
                sample = static_cast<std::int16_t>(std::clamp<std::int64_t>(scaled, std::numeric_limits<std::int16_t>::min(), std::numeric_limits<std::int16_t>::max()));
            }
        }
    }

    void Apply(float* samples, std::size_t frames, unsigned channels, std::uint32_t target) {
        if (Bypass(target)) return;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            const float gain = static_cast<float>(advance(target)) / static_cast<float>(Unity);
            for (unsigned channel = 0; channel < channels; ++channel) samples[frame * channels + channel] *= gain;
        }
    }

private:
    std::uint32_t advance(std::uint32_t target) {
        if (current < target) current = target - current <= step ? target : current + step;
        else if (current > target) current = current - target <= step ? target : current - step;
        return current;
    }

    std::uint32_t current = Unity;
    std::uint32_t step = Unity;
};

inline const void* HostGainFilter(HostGainRamp& ramp, std::uint32_t target, const void* data, bool isFloat, std::size_t frames, unsigned channels, std::vector<std::uint8_t>& scratch) {
    if (ramp.Bypass(target)) return data;
    const std::size_t bytes = frames * channels * (isFloat ? sizeof(float) : sizeof(std::int16_t));
    if (data != scratch.data()) {
        scratch.resize(bytes);
        std::memcpy(scratch.data(), data, bytes);
    }
    if (isFloat) ramp.Apply(reinterpret_cast<float*>(scratch.data()), frames, channels, target);
    else ramp.Apply(reinterpret_cast<std::int16_t*>(scratch.data()), frames, channels, target);
    return scratch.data();
}

#endif

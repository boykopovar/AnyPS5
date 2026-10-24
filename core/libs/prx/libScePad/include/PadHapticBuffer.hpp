#ifndef CORE_LIBS_PRX_LIBSCEPAD_PADHAPTICBUFFER_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_PADHAPTICBUFFER_HPP

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

class PadHapticBuffer {
public:
    static constexpr std::uint32_t Rate = 3000;
    static constexpr std::size_t BlockFrames = 32;
    static constexpr std::size_t MaxFrames = 160;

    void Push(const float* left, const float* right, std::size_t stride, std::uint32_t frames, std::uint32_t rate) {
        if (rate == 0) return;
        for (std::uint32_t frame = 0; frame < frames; ++frame) {
            const auto offset = static_cast<std::size_t>(frame) * stride;
            sum[0] += finite(left[offset]);
            sum[1] += finite(right[offset]);
            ++summed;
            phase += Rate;
            if (phase < rate) continue;
            phase -= rate;
            append({sample(sum[0] / summed), sample(sum[1] / summed)});
            sum = {};
            summed = 0;
        }
    }

    bool Take(std::span<std::int8_t, BlockFrames * 2> block) {
        if (size < BlockFrames) return false;
        for (std::size_t frame = 0; frame < BlockFrames; ++frame) {
            const auto& stereo = frames[(head + frame) % MaxFrames];
            block[frame * 2] = stereo[0];
            block[frame * 2 + 1] = stereo[1];
        }
        head = (head + BlockFrames) % MaxFrames;
        size -= BlockFrames;
        return true;
    }

    void Clear() {
        *this = PadHapticBuffer{};
    }

    std::size_t Frames() const {
        return size;
    }

private:
    static float finite(float value) {
        return std::isfinite(value) ? value : 0.0f;
    }

    static std::int8_t sample(float value) {
        return static_cast<std::int8_t>(std::lround(std::clamp(value, -1.0f, 1.0f) * 127.0f));
    }

    void append(std::array<std::int8_t, 2> stereo) {
        if (size == MaxFrames) {
            head = (head + 1) % MaxFrames;
            --size;
        }
        frames[(head + size) % MaxFrames] = stereo;
        ++size;
    }

    std::array<std::array<std::int8_t, 2>, MaxFrames> frames{};
    std::size_t head = 0;
    std::size_t size = 0;
    std::array<float, 2> sum{};
    std::uint32_t summed = 0;
    std::uint32_t phase = 0;
};

#endif

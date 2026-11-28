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

    void Push(const float* left, const float* right, std::size_t stride, std::uint32_t frames, std::uint32_t rate, unsigned percent = 100) {
        if (rate == 0 || frames == 0 || left == nullptr || right == nullptr) return;
        if (activeRate != 0 && activeRate != rate) {
            sum = {};
            summed = 0;
            phase = 0;
        }
        activeRate = rate;
        const float gain = static_cast<float>(std::min(percent, 100u)) / 100.0f;
        for (std::uint32_t frame = 0; frame < frames; ++frame) {
            const auto offset = static_cast<std::size_t>(frame) * stride;
            sum[0] += finite(left[offset]);
            sum[1] += finite(right[offset]);
            ++summed;
            phase += Rate;
            if (phase < rate) continue;
            const std::array<std::int8_t, 2> stereo{
                sample(sum[0] / summed * gain),
                sample(sum[1] / summed * gain),
            };
            sum = {};
            summed = 0;
            while (phase >= rate) {
                phase -= rate;
                append(stereo);
            }
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

    static std::int8_t sample(double value) {
        return static_cast<std::int8_t>(std::lround(std::clamp(value, -1.0, 1.0) * 127.0));
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
    std::array<double, 2> sum{};
    std::uint32_t summed = 0;
    std::uint32_t activeRate = 0;
    std::uint64_t phase = 0;
};

#endif

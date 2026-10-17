#ifndef CORE_WASM_RUNTIME_PLATFORM_AUDIORING_HPP
#define CORE_WASM_RUNTIME_PLATFORM_AUDIORING_HPP

#include "wasm-runtime/Platform.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace WasmPlatform {

inline constexpr std::uint32_t AudioRingFrames = 8192;
inline constexpr std::uint32_t AudioRings = 4;
inline constexpr float SurroundLevel = 0.70710678f;

struct AudioRing {
    std::atomic<std::uint32_t> active{0};
    std::atomic<std::uint32_t> write{0};
    std::atomic<std::uint32_t> read{0};
    std::atomic<std::uint32_t> underruns{0};
    std::atomic<std::uint32_t> consumed{0};
    std::array<std::uint32_t, 3> reserved{};
    std::array<float, AudioRingFrames * 2> samples{};
};

struct AudioState {
    std::atomic<std::uint32_t> running{0};
    std::array<std::uint32_t, 7> reserved{};
    std::array<AudioRing, AudioRings> rings{};
};

static_assert(offsetof(AudioRing, samples) == 32);
static_assert(sizeof(AudioRing) == 32 + AudioRingFrames * 2 * sizeof(float));
static_assert(offsetof(AudioState, rings) == 32);

inline float SampleAt(const WasmAudio::Samples& samples, std::uint32_t index) {
    if (samples.isFloat) return static_cast<const float*>(samples.data)[index];
    return static_cast<float>(static_cast<const std::int16_t*>(samples.data)[index]) / 32768.0f;
}

inline std::array<float, 2> DownmixFrame(const WasmAudio::Samples& samples, std::uint32_t frame) {
    const auto base = frame * samples.channels;
    const auto at = [&](std::uint32_t channel) { return SampleAt(samples, base + channel); };
    float left = 0.0f;
    float right = 0.0f;
    switch (samples.channels) {
    case 1:
        left = right = at(0);
        break;
    case 2:
        left = at(0);
        right = at(1);
        break;
    case 8:
        left = at(0) + SurroundLevel * (at(2) + at(4) + at(6));
        right = at(1) + SurroundLevel * (at(2) + at(5) + at(7));
        break;
    default:
        throw std::invalid_argument("WasmAudio: " + std::to_string(samples.channels) + " channels cannot be mixed to stereo");
    }
    return {std::clamp(left, -1.0f, 1.0f), std::clamp(right, -1.0f, 1.0f)};
}

inline int OpenRing(AudioState& state) {
    for (std::uint32_t index = 0; index < AudioRings; ++index) {
        auto& ring = state.rings[index];
        if (ring.active.load(std::memory_order_acquire) != 0) continue;
        ring.write.store(ring.read.load(std::memory_order_acquire), std::memory_order_release);
        ring.active.store(1, std::memory_order_release);
        return static_cast<int>(index);
    }
    return -1;
}

inline AudioRing& RingAt(AudioState& state, int stream) {
    if (stream < 0 || static_cast<std::uint32_t>(stream) >= AudioRings || state.rings[static_cast<std::size_t>(stream)].active.load(std::memory_order_acquire) == 0) {
        throw std::out_of_range("WasmAudio: stream " + std::to_string(stream) + " is not open");
    }
    return state.rings[static_cast<std::size_t>(stream)];
}

inline std::uint32_t QueuedFrames(const AudioRing& ring) {
    return ring.write.load(std::memory_order_acquire) - ring.read.load(std::memory_order_acquire);
}

inline std::uint32_t WriteFrames(AudioRing& ring, const WasmAudio::Samples& samples) {
    const auto write = ring.write.load(std::memory_order_relaxed);
    const auto room = AudioRingFrames - (write - ring.read.load(std::memory_order_acquire));
    const auto frames = std::min(samples.frames, room);
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const auto mixed = DownmixFrame(samples, frame);
        const auto slot = ((write + frame) % AudioRingFrames) * 2;
        ring.samples[slot] = mixed[0];
        ring.samples[slot + 1] = mixed[1];
    }
    ring.write.store(write + frames, std::memory_order_release);
    return frames;
}

}  // namespace WasmPlatform

#endif

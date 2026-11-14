#include "prx/libSceAudioOut/src/AudioOut2PadMix.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "audio_out2_pad_haptic_intensity: %s\n", message);
    std::exit(1);
}

constexpr std::uint32_t kFrames = 4;
const float kPad[kFrames * AUDIO_OUT2_PAD_CHANNELS] = {
    0.25f, -0.5f, 0.8f, -0.6f,
    1.0f, 1.0f, -1.0f, 1.0f,
    0.1f, 0.2f, 0.3f, 0.4f,
    0.0f, 0.0f, 0.0f, 0.0f,
};

std::vector<float> Write(const AudioOut2PadLayout& layout, bool explicitPercent, unsigned percent) {
    std::vector<float> out(static_cast<std::size_t>(kFrames) * layout.channels, 9.0f);
    if (explicitPercent) AudioOut2WritePadFrames(kPad, layout, out.data(), kFrames, percent);
    else AudioOut2WritePadFrames(kPad, layout, out.data(), kFrames);
    return out;
}

bool SameBytes(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

bool IsVibration(std::uint32_t channel) {
    return channel == AUDIO_OUT2_PAD_VIBRATION_LEFT || channel == AUDIO_OUT2_PAD_VIBRATION_RIGHT;
}

}

int main() {
    const AudioOut2PadLayout layouts[] = {AudioOut2PadLayoutForDriver("pipewire"), AudioOut2PadLayoutForDriver("pulseaudio")};
    for (const auto& layout : layouts) {
        const auto legacy = Write(layout, false, 0);
        Require(SameBytes(legacy, Write(layout, true, 100)), "100 percent writes the same bytes as the call without a percentage");
        Require(SameBytes(legacy, Write(layout, true, 250)), "a percentage above 100 never amplifies");

        const auto off = Write(layout, true, 0);
        const auto half = Write(layout, true, 50);
        const auto nearlyFull = Write(layout, true, 99);
        for (std::uint32_t frame = 0; frame < kFrames; ++frame) {
            for (std::uint32_t channel = 0; channel < AUDIO_OUT2_PAD_CHANNELS; ++channel) {
                const auto index = static_cast<std::size_t>(frame) * layout.channels + layout.position[channel];
                const float source = kPad[static_cast<std::size_t>(frame) * AUDIO_OUT2_PAD_CHANNELS + channel];
                if (IsVibration(channel)) {
                    Require(off[index] == 0.0f, "0 percent silences the actuator channels");
                    Require(half[index] == source * 0.5f, "50 percent halves the actuator channels");
                    Require(nearlyFull[index] == source * 0.99f, "99 percent scales the actuator channels");
                } else {
                    Require(off[index] == source && half[index] == source, "the controller speaker channels are never scaled");
                }
            }
        }
        for (std::uint32_t frame = 0; frame < kFrames; ++frame) {
            for (std::uint32_t slot = 0; slot < layout.channels; ++slot) {
                bool mapped = false;
                for (std::uint32_t channel = 0; channel < AUDIO_OUT2_PAD_CHANNELS; ++channel) mapped = mapped || layout.position[channel] == slot;
                if (!mapped) Require(off[static_cast<std::size_t>(frame) * layout.channels + slot] == 0.0f, "unmapped device channels stay silent");
            }
        }
    }
    const float broken[AUDIO_OUT2_PAD_CHANNELS] = {0.5f, 0.5f, std::nanf(""), HUGE_VALF};
    for (const auto& layout : layouts) {
        std::vector<float> out(layout.channels, 9.0f);
        AudioOut2WritePadFrames(broken, layout, out.data(), 1, 0);
        Require(out[layout.position[AUDIO_OUT2_PAD_VIBRATION_LEFT]] == 0.0f && out[layout.position[AUDIO_OUT2_PAD_VIBRATION_RIGHT]] == 0.0f, "0 percent silences even a non-finite sample");
    }
    return 0;
}

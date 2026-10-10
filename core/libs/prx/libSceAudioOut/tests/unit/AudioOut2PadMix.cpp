#include "prx/libSceAudioOut/src/AudioOut2PadMix.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::uint32_t frames = 3;
const float unity[2] = {1.0f, 1.0f};

template<std::uint32_t Frames>
std::array<float, Frames * AUDIO_OUT2_PAD_CHANNELS> Silence() {
    return {};
}

void Accumulate(AudioOut2Route route, const float* data, std::uint32_t channels, const float* volume, float* out, std::uint32_t count) {
    for (std::uint32_t frame = 0; frame < count; frame++) {
        AudioOut2AccumulatePadFrame(route, data + static_cast<std::size_t>(frame) * channels, channels, volume,
                                    out + static_cast<std::size_t>(frame) * AUDIO_OUT2_PAD_CHANNELS);
    }
}

std::string Frame(std::uint32_t frame) {
    return " frame " + std::to_string(frame);
}

const Case padRoutes{"RouteForPort_PadTypesWithOneOrTwoChannels_RouteToPad", [] {
    Require(AudioOut2RouteForPort(0x3, 1) == AudioOut2Route::PadSpeaker, "mono type 0x3 is the pad speaker");
    Require(AudioOut2RouteForPort(0x3, 2) == AudioOut2Route::PadSpeaker, "stereo type 0x3 is the pad speaker");
    Require(AudioOut2RouteForPort(0x6, 2) == AudioOut2Route::PadVibration, "stereo type 0x6 is the vibration");
    Require(AudioOut2RouteForPort(0x6, 1) == AudioOut2Route::PadVibration, "mono type 0x6 is the vibration");
}};

const Case mainRoutes{"RouteForPort_OtherTypesOrChannelCounts_StayInMainMix", [] {
    Require(AudioOut2RouteForPort(0x6, 8) == AudioOut2Route::Main, "an 8-channel type 0x6 stays in the main mix");
    Require(AudioOut2RouteForPort(0x3, 0) == AudioOut2Route::Main, "an undecoded format stays in the main mix");
    for (std::uint16_t type : {0x0, 0x1, 0x2, 0x4, 0x5, 0x7, 0x100}) {
        for (std::uint32_t channels : {1u, 2u}) {
            Require(AudioOut2RouteForPort(type, channels) == AudioOut2Route::Main,
                    "type " + std::to_string(type) + " with " + std::to_string(channels) + " channels stays in the main mix");
        }
    }
}};

const Case padDevice{"IsPadAudioDevice_DeviceNames_RecognizesDualSense", [] {
    Require(AudioOut2IsPadAudioDevice("DualSense wireless controller (PS5) Direct DualSense Wireless Controller"), "pulse name");
    Require(AudioOut2IsPadAudioDevice("DualSense Edge Wireless Controller, USB Audio"), "edge alsa name");
    Require(!AudioOut2IsPadAudioDevice("AD102 High Definition Audio Controller Digital Stereo (HDMI)"), "hdmi name");
    Require(!AudioOut2IsPadAudioDevice(nullptr), "null name");
}};

const Case speaker{"AccumulatePadFrame_MonoSpeaker_WritesScaledSpeakerChannels", [] {
    auto out = Silence<frames>();
    const float speakerData[frames] = {0.5f, -0.25f, 0.125f};
    const float half[1] = {0.5f};
    Accumulate(AudioOut2Route::PadSpeaker, speakerData, 1, half, out.data(), frames);
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        const float* pad = &out[frame * AUDIO_OUT2_PAD_CHANNELS];
        RequireEqual(pad[0], speakerData[frame] * 0.5f, "speaker channel 1" + Frame(frame));
        RequireEqual(pad[1], speakerData[frame] * 0.5f, "speaker channel 2" + Frame(frame));
        RequireEqual(pad[2], 0.0f, "actuator channel 3" + Frame(frame));
        RequireEqual(pad[3], 0.0f, "actuator channel 4" + Frame(frame));
    }
}};

const Case vibration{"AccumulatePadFrame_StereoVibration_WritesScaledActuatorChannels", [] {
    auto out = Silence<frames>();
    const float vibrationData[frames * 2] = {0.1f, 0.2f, 0.3f, 0.4f, -0.5f, -0.6f};
    const float gains[2] = {1.0f, 0.5f};
    Accumulate(AudioOut2Route::PadVibration, vibrationData, 2, gains, out.data(), frames);
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        const float* pad = &out[frame * AUDIO_OUT2_PAD_CHANNELS];
        RequireEqual(pad[0], 0.0f, "speaker channel 1" + Frame(frame));
        RequireEqual(pad[1], 0.0f, "speaker channel 2" + Frame(frame));
        RequireEqual(pad[2], vibrationData[frame * 2], "actuator channel 3" + Frame(frame));
        RequireEqual(pad[3], vibrationData[frame * 2 + 1] * 0.5f, "actuator channel 4" + Frame(frame));
    }
}};

const Case clamp{"FinishPadMix_SummedPortsAboveFullScale_Clamp", [] {
    auto out = Silence<frames>();
    const float speakerData[frames] = {0.75f, 0.75f, 0.0f};
    const float vibrationData[frames * 2] = {0.9f, -0.9f, 0.0f, 0.0f, 0.0f, 0.0f};
    Accumulate(AudioOut2Route::PadSpeaker, speakerData, 1, unity, out.data(), frames);
    Accumulate(AudioOut2Route::PadSpeaker, speakerData, 1, unity, out.data(), frames);
    Accumulate(AudioOut2Route::PadVibration, vibrationData, 2, unity, out.data(), frames);
    Accumulate(AudioOut2Route::PadVibration, vibrationData, 2, unity, out.data(), frames);
    AudioOut2FinishPadMix(out.data(), frames);
    RequireEqual(out[0], 1.0f, "speaker sum clamps at full scale, left");
    RequireEqual(out[1], 1.0f, "speaker sum clamps at full scale, right");
    RequireEqual(out[2], 1.0f, "vibration sum clamps at full scale, positive");
    RequireEqual(out[3], -1.0f, "vibration sum clamps at full scale, negative");
    RequireEqual(out[4], 1.0f, "second frame speaker");
    RequireEqual(out[6], 0.0f, "second frame actuator");
    for (std::uint32_t index = 8; index < frames * AUDIO_OUT2_PAD_CHANNELS; index++) {
        RequireEqual(out[index], 0.0f, "silent frame sample " + std::to_string(index));
    }
}};

const Case refused{"AccumulatePadFrame_MainRouteOrEightChannels_ThrowsAndMixesNothing", [] {
    auto out = Silence<1>();
    const float data[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    Testing::RequireThrows<std::invalid_argument>(
        [&] { AudioOut2AccumulatePadFrame(AudioOut2Route::Main, data, 2, unity, out.data()); }, "a main-mix port is refused");
    Testing::RequireThrows<std::invalid_argument>(
        [&] { AudioOut2AccumulatePadFrame(AudioOut2Route::PadVibration, data, 8, unity, out.data()); }, "an 8-channel pad port is refused");
    for (float sample : out) RequireEqual(sample, 0.0f, "a refused port mixes nothing");
}};

const Case layouts{"PadLayoutForDriver_Drivers_SelectQuadOrPulseLayout", [] {
    const auto quad = AudioOut2PadLayoutForDriver("pipewire");
    RequireEqual(quad.channels, 4u, "pipewire channels");
    for (std::uint32_t index = 0; index < 4; index++) RequireEqual(quad.position[index], index, "quad position " + std::to_string(index));
    RequireEqual(AudioOut2PadLayoutForDriver("alsa").channels, 4u, "alsa channels");
    RequireEqual(AudioOut2PadLayoutForDriver(nullptr).channels, 4u, "unknown driver channels");
    const auto pulse = AudioOut2PadLayoutForDriver("pulseaudio");
    RequireEqual(pulse.channels, 6u, "pulse channels");
    RequireEqual(pulse.position[0], 0u, "pulse position 0");
    RequireEqual(pulse.position[1], 1u, "pulse position 1");
    RequireEqual(pulse.position[2], 4u, "pulse position 2");
    RequireEqual(pulse.position[3], 5u, "pulse position 3");
}};

const Case writeFrames{"WritePadFrames_PulseAndQuadLayouts_PlaceChannels", [] {
    const auto quad = AudioOut2PadLayoutForDriver("pipewire");
    const auto pulse = AudioOut2PadLayoutForDriver("pulseaudio");
    const float pad[2 * AUDIO_OUT2_PAD_CHANNELS] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float out[2 * AUDIO_OUT2_PAD_DEVICE_CHANNELS_MAX];
    for (float& sample : out) sample = 9.0f;
    AudioOut2WritePadFrames(pad, pulse, out, 2);
    const float expected[2 * 6] = {0.1f, 0.2f, 0.0f, 0.0f, 0.3f, 0.4f, 0.5f, 0.6f, 0.0f, 0.0f, 0.7f, 0.8f};
    for (int index = 0; index < 12; index++) RequireEqual(out[index], expected[index], "pulse sample " + std::to_string(index));
    AudioOut2WritePadFrames(pad, quad, out, 2);
    for (int index = 0; index < 8; index++) RequireEqual(out[index], pad[index], "quad sample " + std::to_string(index));
}};

} // namespace

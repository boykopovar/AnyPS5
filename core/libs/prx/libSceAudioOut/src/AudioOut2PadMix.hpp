#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUT2PADMIX_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUT2PADMIX_HPP

#include <cstdint>
#include <array>

static constexpr std::uint16_t AUDIO_OUT2_PORT_TYPE_PAD_SPEAKER = 0x3;
static constexpr std::uint16_t AUDIO_OUT2_PORT_TYPE_PAD_VIBRATION = 0x6;

static constexpr std::uint32_t AUDIO_OUT2_PAD_CHANNELS = 4;
static constexpr std::uint32_t AUDIO_OUT2_PAD_SPEAKER_LEFT = 0;
static constexpr std::uint32_t AUDIO_OUT2_PAD_SPEAKER_RIGHT = 1;
static constexpr std::uint32_t AUDIO_OUT2_PAD_VIBRATION_LEFT = 2;
static constexpr std::uint32_t AUDIO_OUT2_PAD_VIBRATION_RIGHT = 3;
static constexpr std::uint32_t AUDIO_OUT2_PAD_DEVICE_CHANNELS_MAX = 6;

enum class AudioOut2Route {
    Main,
    PadSpeaker,
    PadVibration,
};

struct AudioOut2PadLayout {
    std::uint32_t channels = AUDIO_OUT2_PAD_CHANNELS;
    std::uint32_t position[AUDIO_OUT2_PAD_CHANNELS] = {0, 1, 2, 3};
};

AudioOut2Route AudioOut2RouteForPort(std::uint16_t type, std::uint32_t channels);
bool AudioOut2IsPadAudioDevice(const char* name);
void AudioOut2AccumulatePadFrame(AudioOut2Route route, const float* in, std::uint32_t channels, const float* volume, float* pad);
void AudioOut2FinishPadMix(float* out, std::uint32_t frames);
std::array<std::uint8_t, 2> AudioOut2RumbleFromPadFrames(const float* pad, std::uint32_t frames);
AudioOut2PadLayout AudioOut2PadLayoutForDriver(const char* driver);
void AudioOut2WritePadFrames(const float* pad, const AudioOut2PadLayout& layout, float* out, std::uint32_t frames);

#endif

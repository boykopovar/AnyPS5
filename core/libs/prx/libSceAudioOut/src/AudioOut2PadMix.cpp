#include "AudioOut2PadMix.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <stdexcept>
#include <string>

static constexpr float DEFAULT_PAD_SPEAKER_FALLBACK_GAIN = 1.0f;
static constexpr float PAD_SPEAKER_FALLBACK_MIN_DB = -60.0f;
static constexpr float PAD_SPEAKER_FALLBACK_MAX_DB = 0.0f;

static bool WordEquals(const char* first, const char* last, const char* word) {
    const auto length = static_cast<std::size_t>(last - first);
    if (std::strlen(word) != length) return false;
    for (std::size_t index = 0; index < length; index++) {
        if (std::tolower(static_cast<unsigned char>(first[index])) != word[index]) return false;
    }
    return true;
}

static bool IsMutedText(const char* text) {
    while (std::isspace(static_cast<unsigned char>(*text))) text++;
    const char* end = text + std::strlen(text);
    while (end > text && std::isspace(static_cast<unsigned char>(end[-1]))) end--;
    return WordEquals(text, end, "mute") || WordEquals(text, end, "off") || WordEquals(text, end, "-inf");
}

float AudioPadSpeakerFallbackGainFrom(const char* dbText) {
    if (dbText == nullptr) return DEFAULT_PAD_SPEAKER_FALLBACK_GAIN;
    const char* const text = dbText;
    while (std::isspace(static_cast<unsigned char>(*dbText))) dbText++;
    if (IsMutedText(dbText)) return 0.0f;
    char* end = nullptr;
    const float db = std::strtof(dbText, &end);
    while (std::isspace(static_cast<unsigned char>(*end))) end++;
    if (end == dbText || !std::isfinite(db) || *end != '\0' || db < PAD_SPEAKER_FALLBACK_MIN_DB || db > PAD_SPEAKER_FALLBACK_MAX_DB) {
        throw std::runtime_error(std::string(AUDIO_PAD_SPEAKER_FALLBACK_ENV) + '=' + text + ": expected mute, off, -inf or a number of dB from -60 to 0");
    }
    return std::pow(10.0f, db / 20.0f);
}

float AudioPadSpeakerFallbackGain() {
    return AudioPadSpeakerFallbackGainFrom(std::getenv(AUDIO_PAD_SPEAKER_FALLBACK_ENV));
}

AudioOut2Route AudioOut2RouteForPort(std::uint16_t type, std::uint32_t channels) {
    if (type == AUDIO_OUT2_PORT_TYPE_PAD_SPEAKER && (channels == 1 || channels == 2)) return AudioOut2Route::PadSpeaker;
    if (type == AUDIO_OUT2_PORT_TYPE_PAD_VIBRATION && (channels == 1 || channels == 2)) return AudioOut2Route::PadVibration;
    return AudioOut2Route::Main;
}

bool AudioOut2IsPadAudioDevice(const char* name) {
    if (name == nullptr) return false;
    constexpr char needle[] = "dualsense";
    constexpr std::size_t length = sizeof(needle) - 1;
    for (const char* start = name; *start != '\0'; start++) {
        std::size_t matched = 0;
        while (matched < length && start[matched] != '\0' && (start[matched] | 0x20) == needle[matched]) matched++;
        if (matched == length) return true;
    }
    return false;
}

void AudioOut2AccumulatePadFrame(AudioOut2Route route, const float* in, std::uint32_t channels, const float* volume, float* pad) {
    if (route == AudioOut2Route::Main || (channels != 1 && channels != 2)) throw std::invalid_argument("AudioOut2AccumulatePadFrame: the port does not play on the controller");
    const auto left = route == AudioOut2Route::PadSpeaker ? AUDIO_OUT2_PAD_SPEAKER_LEFT : AUDIO_OUT2_PAD_VIBRATION_LEFT;
    const auto right = route == AudioOut2Route::PadSpeaker ? AUDIO_OUT2_PAD_SPEAKER_RIGHT : AUDIO_OUT2_PAD_VIBRATION_RIGHT;
    const auto rightSource = channels == 2 ? 1u : 0u;
    pad[left] += in[0] * volume[0];
    pad[right] += in[rightSource] * volume[rightSource];
}

void AudioOut2FinishPadMix(float* out, std::uint32_t frames) {
    const auto samples = static_cast<std::size_t>(frames) * AUDIO_OUT2_PAD_CHANNELS;
    for (std::size_t index = 0; index < samples; index++) out[index] = std::clamp(out[index], -1.0f, 1.0f);
}

std::array<std::uint8_t, 2> AudioOut2RumbleFromPadFrames(const float* pad, std::uint32_t frames) {
    std::array<std::uint8_t, 2> result{};
    if (pad == nullptr || frames == 0) return result;
    for (std::uint32_t channel = 0; channel < 2; ++channel) {
        double energy = 0.0;
        for (std::uint32_t frame = 0; frame < frames; ++frame) {
            const auto sample = pad[static_cast<std::size_t>(frame) * AUDIO_OUT2_PAD_CHANNELS + AUDIO_OUT2_PAD_VIBRATION_LEFT + channel];
            if (std::isfinite(sample)) energy += static_cast<double>(sample) * sample;
        }
        const auto rms = std::sqrt(energy / frames);
        if (rms >= 0.01) result[channel] = static_cast<std::uint8_t>(std::lround(std::min(1.0, rms * 2.0) * 255));
    }
    return result;
}

AudioOut2PadLayout AudioOut2PadLayoutForDriver(const char* driver) {
    AudioOut2PadLayout layout;
    if (driver != nullptr && std::strcmp(driver, "pulseaudio") == 0) {
        layout.channels = AUDIO_OUT2_PAD_DEVICE_CHANNELS_MAX;
        layout.position[AUDIO_OUT2_PAD_VIBRATION_LEFT] = 4;
        layout.position[AUDIO_OUT2_PAD_VIBRATION_RIGHT] = 5;
    }
    return layout;
}

void AudioOut2WritePadFrames(const float* pad, const AudioOut2PadLayout& layout, float* out, std::uint32_t frames, unsigned vibrationPercent) {
    std::fill(out, out + static_cast<std::size_t>(frames) * layout.channels, 0.0f);
    const bool scaled = vibrationPercent < 100;
    const float gain = static_cast<float>(vibrationPercent) / 100.0f;
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        for (std::uint32_t channel = 0; channel < AUDIO_OUT2_PAD_CHANNELS; channel++) {
            if (scaled && (channel == AUDIO_OUT2_PAD_VIBRATION_LEFT || channel == AUDIO_OUT2_PAD_VIBRATION_RIGHT)) {
                if (vibrationPercent != 0) out[static_cast<std::size_t>(frame) * layout.channels + layout.position[channel]] = pad[static_cast<std::size_t>(frame) * AUDIO_OUT2_PAD_CHANNELS + channel] * gain;
                continue;
            }
            out[static_cast<std::size_t>(frame) * layout.channels + layout.position[channel]] = pad[static_cast<std::size_t>(frame) * AUDIO_OUT2_PAD_CHANNELS + channel];
        }
    }
}

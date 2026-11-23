#include "prx/libSceAudioOut/src/AudioOut2PadMix.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

static void Require(bool value, const char*) {
    if (!value) std::abort();
}

static void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

static void UnsetEnvironment(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

static void RequireGain(const char* text, float expected, const char* message) {
    Require(std::fabs(AudioPadSpeakerFallbackGainFrom(text) - expected) <= 1e-6f, message);
}

static void RequireRejected(const char* text, const char* message) {
    try {
        AudioPadSpeakerFallbackGainFrom(text);
    } catch (const std::exception& error) {
        const std::string what(error.what());
        Require(what.find(std::string(AUDIO_PAD_SPEAKER_FALLBACK_ENV) + '=' + text) != std::string::npos && what.find("expected") != std::string::npos, message);
        return;
    }
    Require(false, message);
}

template<typename TException, typename TAction>
static bool Throws(TAction action) {
    try {
        action();
    } catch (const TException&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

template<std::uint32_t Frames>
static std::array<float, Frames * AUDIO_OUT2_PAD_CHANNELS> Silence() {
    return {};
}

static void Accumulate(AudioOut2Route route, const float* data, std::uint32_t channels, const float* volume, float* out, std::uint32_t frames) {
    for (std::uint32_t frame = 0; frame < frames; frame++) AudioOut2AccumulatePadFrame(route, data + static_cast<std::size_t>(frame) * channels, channels, volume, out + static_cast<std::size_t>(frame) * AUDIO_OUT2_PAD_CHANNELS);
}

int main() {
    {
        const float pad[] = {1.0f, -1.0f, 0.25f, -0.5f, 1.0f, -1.0f, -0.25f, 0.5f};
        const auto rumble = AudioOut2RumbleFromPadFrames(pad, 2);
        Require(rumble[0] == 128 && rumble[1] == 255, "haptic energy drives separate motors, not speaker samples");
        const float speakersOnly[] = {1.0f, 1.0f, 0.0f, 0.0f};
        Require(AudioOut2RumbleFromPadFrames(speakersOnly, 1) == std::array<std::uint8_t, 2>{}, "speaker sound does not rumble");
        Require(AudioOut2RumbleFromPadFrames(nullptr, 0) == std::array<std::uint8_t, 2>{}, "empty input does not rumble");
    }
    Require(AudioOut2RouteForPort(0x3, 1) == AudioOut2Route::PadSpeaker, "mono type 0x3 is the pad speaker");
    Require(AudioOut2RouteForPort(0x3, 2) == AudioOut2Route::PadSpeaker, "stereo type 0x3 is the pad speaker");
    Require(AudioOut2RouteForPort(0x6, 2) == AudioOut2Route::PadVibration, "stereo type 0x6 is the vibration");
    Require(AudioOut2RouteForPort(0x6, 1) == AudioOut2Route::PadVibration, "mono type 0x6 is the vibration");
    Require(AudioOut2RouteForPort(0x6, 8) == AudioOut2Route::Main, "an 8-channel type 0x6 stays in the main mix");
    Require(AudioOut2RouteForPort(0x3, 0) == AudioOut2Route::Main, "an undecoded format stays in the main mix");
    for (std::uint16_t type : {0x0, 0x1, 0x2, 0x4, 0x5, 0x7, 0x100}) {
        Require(AudioOut2RouteForPort(type, 1) == AudioOut2Route::Main, "other port types stay in the main mix");
        Require(AudioOut2RouteForPort(type, 2) == AudioOut2Route::Main, "other port types stay in the main mix");
    }

    Require(AudioOut2IsPadAudioDevice("DualSense wireless controller (PS5) Direct DualSense Wireless Controller"), "pulse name");
    Require(AudioOut2IsPadAudioDevice("DualSense Edge Wireless Controller, USB Audio"), "edge alsa name");
    Require(!AudioOut2IsPadAudioDevice("AD102 High Definition Audio Controller Digital Stereo (HDMI)"), "hdmi name");
    Require(!AudioOut2IsPadAudioDevice(nullptr), "null name");

    constexpr std::uint32_t frames = 3;
    const float unity[2] = {1.0f, 1.0f};

    {
        auto out = Silence<frames>();
        const float speaker[frames] = {0.5f, -0.25f, 0.125f};
        const float half[1] = {0.5f};
        Accumulate(AudioOut2Route::PadSpeaker, speaker, 1, half, out.data(), frames);
        for (std::uint32_t frame = 0; frame < frames; frame++) {
            const float* pad = &out[frame * AUDIO_OUT2_PAD_CHANNELS];
            Require(pad[0] == speaker[frame] * 0.5f && pad[1] == speaker[frame] * 0.5f, "mono speaker on channels 1-2");
            Require(pad[2] == 0.0f && pad[3] == 0.0f, "speaker leaves the actuators alone");
        }
    }

    {
        auto out = Silence<frames>();
        const float vibration[frames * 2] = {0.1f, 0.2f, 0.3f, 0.4f, -0.5f, -0.6f};
        const float gains[2] = {1.0f, 0.5f};
        Accumulate(AudioOut2Route::PadVibration, vibration, 2, gains, out.data(), frames);
        for (std::uint32_t frame = 0; frame < frames; frame++) {
            const float* pad = &out[frame * AUDIO_OUT2_PAD_CHANNELS];
            Require(pad[0] == 0.0f && pad[1] == 0.0f, "vibration leaves the speaker alone");
            Require(pad[2] == vibration[frame * 2] && pad[3] == vibration[frame * 2 + 1] * 0.5f, "vibration on channels 3-4");
        }
    }

    {
        auto out = Silence<frames>();
        const float speaker[frames] = {0.75f, 0.75f, 0.0f};
        const float vibration[frames * 2] = {0.9f, -0.9f, 0.0f, 0.0f, 0.0f, 0.0f};
        Accumulate(AudioOut2Route::PadSpeaker, speaker, 1, unity, out.data(), frames);
        Accumulate(AudioOut2Route::PadSpeaker, speaker, 1, unity, out.data(), frames);
        Accumulate(AudioOut2Route::PadVibration, vibration, 2, unity, out.data(), frames);
        Accumulate(AudioOut2Route::PadVibration, vibration, 2, unity, out.data(), frames);
        AudioOut2FinishPadMix(out.data(), frames);
        Require(out[0] == 1.0f && out[1] == 1.0f, "speaker sum clamps at full scale");
        Require(out[2] == 1.0f && out[3] == -1.0f, "vibration sum clamps at full scale");
        Require(out[4] == 1.0f && out[6] == 0.0f, "second frame");
        for (std::uint32_t index = 8; index < frames * AUDIO_OUT2_PAD_CHANNELS; index++) Require(out[index] == 0.0f, "silent frame stays silent");
    }

    {
        auto out = Silence<1>();
        const float data[8] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
        Require(Throws<std::invalid_argument>([&] { AudioOut2AccumulatePadFrame(AudioOut2Route::Main, data, 2, unity, out.data()); }), "a main-mix port is refused");
        Require(Throws<std::invalid_argument>([&] { AudioOut2AccumulatePadFrame(AudioOut2Route::PadVibration, data, 8, unity, out.data()); }), "an 8-channel pad port is refused");
        for (float sample : out) Require(sample == 0.0f, "a refused port mixes nothing");
    }

    RequireGain(nullptr, 1.0f, "unset text keeps unity");
    RequireGain("0", 1.0f, "0 dB is unity");
    RequireGain("-15", 0.17782794f, "explicit -15");
    RequireGain(" -15 ", 0.17782794f, "surrounding whitespace is ignored");
    RequireGain("mute", 0.0f, "mute");
    RequireGain("off", 0.0f, "off");
    RequireGain(" -inf ", 0.0f, "negative infinity with surrounding whitespace");
    RequireGain("-60", 0.001f, "lower bound");
    RequireRejected("", "an empty value is rejected");
    RequireRejected("   ", "whitespace-only text is rejected");
    RequireRejected("nan", "a non-finite value is rejected");
    RequireRejected("inf", "a non-finite value is rejected");
    RequireRejected("-100", "a value below the range is rejected");
    RequireRejected("5", "a value above the range is rejected");
    RequireRejected("garbage", "invalid input is rejected");
    RequireRejected("-15x", "trailing text is rejected");
    RequireRejected("  -15x", "leading whitespace is reported verbatim");

    UnsetEnvironment(AUDIO_PAD_SPEAKER_FALLBACK_ENV);
    Require(std::fabs(AudioPadSpeakerFallbackGain() - 1.0f) <= 1e-6f, "unset environment keeps unity");
    SetEnvironment(AUDIO_PAD_SPEAKER_FALLBACK_ENV, "-15");
    Require(std::fabs(AudioPadSpeakerFallbackGain() - 0.17782794f) <= 1e-6f, "environment override");
    SetEnvironment(AUDIO_PAD_SPEAKER_FALLBACK_ENV, "mute");
    Require(AudioPadSpeakerFallbackGain() == 0.0f, "muted environment");
    SetEnvironment(AUDIO_PAD_SPEAKER_FALLBACK_ENV, "garbage");
    Require(Throws<std::runtime_error>([] { AudioPadSpeakerFallbackGain(); }), "an invalid environment stops the title");
    UnsetEnvironment(AUDIO_PAD_SPEAKER_FALLBACK_ENV);

    {
        const auto quad = AudioOut2PadLayoutForDriver("pipewire");
        Require(quad.channels == 4 && quad.position[0] == 0 && quad.position[1] == 1 && quad.position[2] == 2 && quad.position[3] == 3, "quad layout");
        Require(AudioOut2PadLayoutForDriver("alsa").channels == 4 && AudioOut2PadLayoutForDriver(nullptr).channels == 4, "alsa and unknown use quad");
        const auto pulse = AudioOut2PadLayoutForDriver("pulseaudio");
        Require(pulse.channels == 6 && pulse.position[0] == 0 && pulse.position[1] == 1 && pulse.position[2] == 4 && pulse.position[3] == 5, "pulse layout");

        const float pad[2 * AUDIO_OUT2_PAD_CHANNELS] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
        float out[2 * AUDIO_OUT2_PAD_DEVICE_CHANNELS_MAX];
        for (float& sample : out) sample = 9.0f;
        AudioOut2WritePadFrames(pad, pulse, out, 2);
        const float expected[2 * 6] = {0.1f, 0.2f, 0.0f, 0.0f, 0.3f, 0.4f, 0.5f, 0.6f, 0.0f, 0.0f, 0.7f, 0.8f};
        for (int index = 0; index < 12; index++) Require(out[index] == expected[index], "pulse frames: speaker front, centre and LFE silent, actuators back");
        AudioOut2WritePadFrames(pad, quad, out, 2);
        for (int index = 0; index < 8; index++) Require(out[index] == pad[index], "quad frames are the pad mix");
    }

    {
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        const float negInf = -inf;

        auto out = Silence<1>();
        const float nonFiniteInput[2] = {nan, inf};
        const float zeroGain[2] = {0.0f, 0.0f};
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, nonFiniteInput, 2, unity, out.data());
        Require(out[0] == 0.0f && out[1] == 0.0f, "non-finite inputs sanitized to zero in accumulation");

        const float infInput[1] = {inf};
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, infInput, 1, zeroGain, out.data());
        Require(out[0] == 0.0f && out[1] == 0.0f, "zero gain with infinity input produces zero");

        const float negInfInput[2] = {negInf, nan};
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadVibration, negInfInput, 2, unity, out.data());
        Require(out[2] == 0.0f && out[3] == 0.0f, "negative infinity and nan vibration sanitized");

        float dirtyPad[AUDIO_OUT2_PAD_CHANNELS] = {nan, inf, negInf, nan};
        const float validSpeaker[1] = {0.5f};
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, validSpeaker, 1, unity, dirtyPad);
        Require(dirtyPad[0] == 0.5f && dirtyPad[1] == 0.5f, "non-finite accumulator sanitized and valid signal preserved");

        float mixedPortPad[AUDIO_OUT2_PAD_CHANNELS] = {0.25f, 0.25f, 0.0f, 0.0f};
        const float nanSpeaker[1] = {nan};
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, nanSpeaker, 1, unity, mixedPortPad);
        Require(mixedPortPad[0] == 0.25f && mixedPortPad[1] == 0.25f, "valid signal preserved alongside non-finite port");

        float dirtyOut[AUDIO_OUT2_PAD_CHANNELS] = {nan, inf, negInf, 2.5f};
        AudioOut2FinishPadMix(dirtyOut, 1);
        Require(dirtyOut[0] == 0.0f && dirtyOut[1] == 0.0f && dirtyOut[2] == 0.0f, "non-finite values sanitized in finish mix");
        Require(dirtyOut[3] == 1.0f, "finite value clamped in finish mix");

        const auto quad = AudioOut2PadLayoutForDriver("pipewire");
        const float nonFinitePad[AUDIO_OUT2_PAD_CHANNELS] = {nan, inf, negInf, 0.75f};
        float writeOut[AUDIO_OUT2_PAD_CHANNELS] = {nan, nan, nan, nan};
        AudioOut2WritePadFrames(nonFinitePad, quad, writeOut, 1, 50);
        Require(writeOut[0] == 0.0f && writeOut[1] == 0.0f && writeOut[2] == 0.0f, "write pad frames sanitizes non-finite pad");
        Require(std::fabs(writeOut[3] - 0.375f) <= 1e-6f, "write pad frames preserves and scales valid vibration");
    }

    {
        auto productOverflow = Silence<1>();
        const float loudPair[2] = {3.0e38f, -3.0e38f};
        const float doubleGain[2] = {2.0f, 2.0f};
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, loudPair, 2, doubleGain, productOverflow.data());
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadVibration, loudPair, 2, doubleGain, productOverflow.data());
        Require(std::isfinite(productOverflow[0]) && productOverflow[0] > 0.0f, "positive product overflow saturates to finite positive max");
        Require(std::isfinite(productOverflow[1]) && productOverflow[1] < 0.0f, "negative product overflow saturates to finite negative max");
        Require(std::isfinite(productOverflow[2]) && productOverflow[2] > 0.0f, "positive vibration product overflow saturates");
        Require(std::isfinite(productOverflow[3]) && productOverflow[3] < 0.0f, "negative vibration product overflow saturates");
        AudioOut2FinishPadMix(productOverflow.data(), 1);
        Require(productOverflow[0] == 1.0f && productOverflow[1] == -1.0f, "speaker product overflow clamps to full scale");
        Require(productOverflow[2] == 1.0f && productOverflow[3] == -1.0f, "vibration product overflow clamps to full scale");

        auto sumOverflow = Silence<1>();
        const float loudSpeaker[2] = {3.0e38f, -3.0e38f};
        const float loudVibration[2] = {-3.0e38f, 3.0e38f};
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, loudSpeaker, 2, unity, sumOverflow.data());
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, loudSpeaker, 2, unity, sumOverflow.data());
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadVibration, loudVibration, 2, unity, sumOverflow.data());
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadVibration, loudVibration, 2, unity, sumOverflow.data());
        Require(std::isfinite(sumOverflow[0]) && sumOverflow[0] > 0.0f, "positive sum overflow saturates");
        Require(std::isfinite(sumOverflow[1]) && sumOverflow[1] < 0.0f, "negative sum overflow saturates");
        AudioOut2FinishPadMix(sumOverflow.data(), 1);
        Require(sumOverflow[0] == 1.0f && sumOverflow[1] == -1.0f, "speaker sum overflow clamps to full scale");
        Require(sumOverflow[2] == -1.0f && sumOverflow[3] == 1.0f, "vibration sum overflow clamps to full scale");
    }

    {
        auto padOut = Silence<1>();
        const float sample[2] = {0.5f, -0.5f};
        Require(Throws<std::invalid_argument>([&] { AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, nullptr, 1, unity, padOut.data()); }), "accumulate rejects null input");
        Require(Throws<std::invalid_argument>([&] { AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, sample, 2, nullptr, padOut.data()); }), "accumulate rejects null volume");
        Require(Throws<std::invalid_argument>([&] { AudioOut2AccumulatePadFrame(AudioOut2Route::PadSpeaker, sample, 2, unity, nullptr); }), "accumulate rejects null pad");

        AudioOut2FinishPadMix(nullptr, 0);
        Require(Throws<std::invalid_argument>([] { AudioOut2FinishPadMix(nullptr, 1); }), "finish mix rejects null output when frames > 0");

        const auto quad = AudioOut2PadLayoutForDriver("pipewire");
        const float validPad[AUDIO_OUT2_PAD_CHANNELS] = {0.1f, 0.2f, 0.3f, 0.4f};
        float writeOut[AUDIO_OUT2_PAD_DEVICE_CHANNELS_MAX] = {};
        AudioOut2WritePadFrames(nullptr, quad, nullptr, 0);
        AudioOut2WritePadFrames(validPad, quad, nullptr, 0);
        AudioOut2WritePadFrames(nullptr, quad, writeOut, 0);
        Require(Throws<std::invalid_argument>([&] { AudioOut2WritePadFrames(nullptr, quad, writeOut, 1); }), "write pad frames rejects null pad when frames > 0");
        Require(Throws<std::invalid_argument>([&] { AudioOut2WritePadFrames(validPad, quad, nullptr, 1); }), "write pad frames rejects null out when frames > 0");

        auto tooFewChannels = quad;
        tooFewChannels.channels = AUDIO_OUT2_PAD_CHANNELS - 1;
        Require(Throws<std::invalid_argument>([&] { AudioOut2WritePadFrames(validPad, tooFewChannels, writeOut, 1); }), "write pad frames rejects fewer than 4 channels");
        Require(Throws<std::invalid_argument>([&] { AudioOut2WritePadFrames(nullptr, tooFewChannels, nullptr, 0); }), "write pad frames rejects invalid channel count even when frames == 0");

        for (std::uint32_t channel = 0; channel < AUDIO_OUT2_PAD_CHANNELS; channel++) {
            auto badPosition = quad;
            badPosition.position[channel] = badPosition.channels;
            Require(Throws<std::invalid_argument>([&] { AudioOut2WritePadFrames(validPad, badPosition, writeOut, 1); }), "write pad frames rejects out-of-range channel position");
        }
    }
    return 0;
}

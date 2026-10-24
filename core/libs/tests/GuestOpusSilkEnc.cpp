#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <source_location>
#include <stdexcept>
#include <opus.h>

extern "C" {
int APS5_VABI sceOpusSilkEncInitialize(std::uint32_t*);
int APS5_VABI sceOpusSilkEncTerminate(const std::uint32_t*);
int APS5_VABI sceOpusSilkEncGetSize(int);
int APS5_VABI sceOpusSilkEncCreate(void*, int, int, int);
int APS5_VABI sceOpusSilkEncCreateEx(const std::uint32_t*, void*, int, int, int);
int APS5_VABI sceOpusSilkEncEncode(void*, const std::int16_t*, int, std::uint8_t*, int);
int APS5_VABI sceOpusSilkEncEncodeFloat(void*, const float*, int, std::uint8_t*, int);
int APS5_VABI sceOpusSilkEncDestroy(void*);
int APS5_VABI sceOpusSilkEncCtl(void*, int, ...);
int APS5_VABI sceOpusSilkEncCancel(void*, unsigned int);
int APS5_VABI sceOpusSilkEncEncodeWithPriority(void*, const std::int16_t*, int, std::uint8_t*, int, unsigned int);
int APS5_VABI sceOpusSilkEncEncodeFloatWithPriority(void*, const float*, int, std::uint8_t*, int, unsigned int);
int APS5_VABI sceOpusSilkEncEncodeWithPriorityAndTimeout(void*, const std::int16_t*, int, std::uint8_t*, int, unsigned int, unsigned int, unsigned int*);
int APS5_VABI sceOpusSilkEncEncodeFloatWithPriorityAndTimeout(void*, const float*, int, std::uint8_t*, int, unsigned int, unsigned int, unsigned int*);
}

namespace {

void Require(bool value, const std::source_location& location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at %s:%d\n", location.file_name(), static_cast<int>(location.line()));
        std::abort();
    }
}

template <typename TFunction>
void RequireThrows(TFunction function) {
    try { function(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

void FillSine(std::int16_t* samples, int frameSize, int channels, int rate, int position) {
    for (int index = 0; index < frameSize; ++index) {
        const int value = static_cast<int>(3000 * std::sin(2 * 3.14159265358979323846 * 2000 * (position + index) / rate));
        for (int channel = 0; channel < channels; ++channel) samples[index * channels + channel] = static_cast<std::int16_t>(value);
    }
}

void RequireTone(const std::int16_t* pcm, int samples, int channels, int rate) {
    for (int channel = 0; channel < channels; ++channel) {
        int peak = 0, crossings = 0;
        for (int sample = 0; sample < samples; ++sample) {
            const int value = pcm[sample * channels + channel];
            peak = std::max(peak, std::abs(value));
            if (sample > 0) {
                const int previous = pcm[(sample - 1) * channels + channel];
                if ((previous <= 0 && value > 0) || (previous >= 0 && value < 0)) ++crossings;
            }
        }
        Require(peak > 30 && peak < 32000);
        const int expected = 2 * samples * 2000 / rate;
        Require(crossings >= expected - expected / 3 && crossings <= expected + expected / 3);
    }
}

} // namespace

int main() {
    constexpr std::uint32_t errorBase = 0x81740000u;
    constexpr int application = OPUS_APPLICATION_RESTRICTED_SILK;
    Require(sceOpusSilkEncInitialize(nullptr) == static_cast<int>(errorBase + 13));
    Require(sceOpusSilkEncDestroy(nullptr) == static_cast<int>(errorBase));
    std::uint32_t context = 0;
    Require(sceOpusSilkEncInitialize(&context) == 0);
    RequireThrows([&] { sceOpusSilkEncInitialize(&context); });
    RequireThrows([&] { sceOpusSilkEncTerminate(nullptr); });

    std::array<std::uint8_t, 65536> invalidState{};
    std::array<std::int16_t, 1920> samples{};
    samples.fill(12345);
    std::array<float, 1920> floatSamples{};
    std::uint8_t packet[1024];
    for (auto& byte : packet) byte = 0x5A;
    Require(sceOpusSilkEncCreate(nullptr, 48000, 2, application) == static_cast<int>(errorBase));
    Require(sceOpusSilkEncCreate(invalidState.data(), 48000, 0, application) == static_cast<int>(errorBase + 3));
    Require(sceOpusSilkEncCreate(invalidState.data(), 0, 0, application) == static_cast<int>(errorBase + 3));
    Require(sceOpusSilkEncCreate(invalidState.data(), 22050, 2, application) == static_cast<int>(errorBase + 2));
    Require(sceOpusSilkEncCreate(invalidState.data(), 48000, 2, 2048) == static_cast<int>(errorBase + 2));
    Require(sceOpusSilkEncCreateEx(nullptr, invalidState.data(), 0, 0, application) == static_cast<int>(errorBase));
    Require(sceOpusSilkEncCreateEx(&context, invalidState.data(), 0, 0, application) == static_cast<int>(errorBase + 3));
    Require(sceOpusSilkEncCreateEx(&context, invalidState.data(), 48000, 0, application) == static_cast<int>(errorBase + 3));
    Require(sceOpusSilkEncCreateEx(&context, invalidState.data(), 48000, 2, 2048) == static_cast<int>(errorBase + 2));
    Require(sceOpusSilkEncEncode(nullptr, nullptr, -1, nullptr, -1) == static_cast<int>(errorBase));
    Require(sceOpusSilkEncEncode(invalidState.data(), nullptr, 960, nullptr, -1) == static_cast<int>(errorBase + 6));
    Require(sceOpusSilkEncEncode(invalidState.data(), samples.data(), -1, packet, -1) == static_cast<int>(errorBase + 7));
    Require(sceOpusSilkEncEncode(invalidState.data(), samples.data(), 960, nullptr, -1) == static_cast<int>(errorBase + 8));
    Require(sceOpusSilkEncEncode(invalidState.data(), samples.data(), 960, packet, -1) == static_cast<int>(errorBase + 9));
    Require(sceOpusSilkEncEncode(invalidState.data(), samples.data(), 960, packet, 8) == static_cast<int>(errorBase + 1));
    Require(packet[0] == 0x5A && packet[1] == 0x5A);
    Require(sceOpusSilkEncEncodeFloat(nullptr, nullptr, -1, nullptr, -1) == static_cast<int>(errorBase));
    Require(sceOpusSilkEncEncodeFloat(invalidState.data(), nullptr, 960, packet, 8) == static_cast<int>(errorBase + 6));
    Require(sceOpusSilkEncEncodeFloat(invalidState.data(), floatSamples.data(), -1, packet, 8) == static_cast<int>(errorBase + 7));

    Require(sceOpusSilkEncGetSize(1) == opus_encoder_get_size(1) && sceOpusSilkEncGetSize(2) == opus_encoder_get_size(2));
    Require(sceOpusSilkEncGetSize(3) == static_cast<int>(errorBase + 3));

    for (int rate : {8000, 12000, 16000, 24000, 48000}) {
        for (int channels : {1, 2}) {
            std::array<std::uint8_t, 65536> state{};
            Require(sceOpusSilkEncCreateEx(&context, state.data(), rate, channels, application) == 0);
            int opusError = OPUS_OK;
            auto* reference = opus_encoder_create(rate, channels, application, &opusError);
            Require(reference && opusError == OPUS_OK);
            const int frameSize = rate / 50;
            for (int frame = 0; frame < 4; ++frame) {
                FillSine(samples.data(), frameSize, channels, rate, frame * frameSize);
                const int bytes = sceOpusSilkEncEncode(state.data(), samples.data(), frameSize, packet, sizeof(packet));
                Require(bytes > 2 && static_cast<int>(packet[0]) >> 3 < 12);
                std::array<std::uint8_t, 1024> expected{};
                const int expectedBytes = opus_encode(reference, samples.data(), frameSize, expected.data(), sizeof(expected));
                Require(expectedBytes == bytes && std::equal(packet, packet + bytes, expected.begin()));
            }
            int decodeError = OPUS_OK;
            auto* decoder = opus_decoder_create(rate, channels, &decodeError);
            Require(decoder && decodeError == OPUS_OK);
            std::array<std::int16_t, 1922> pcm{};
            for (int frame = 0; frame < 4; ++frame) {
                FillSine(samples.data(), frameSize, channels, rate, frame * frameSize);
                const int bytes = sceOpusSilkEncEncode(state.data(), samples.data(), frameSize, packet, sizeof(packet));
                pcm.fill(12345);
                Require(opus_decode(decoder, packet, bytes, pcm.data() + 1, frameSize, 0) == frameSize);
                Require(pcm.front() == 12345 && pcm.back() == 12345);
                RequireTone(pcm.data() + 1, frameSize, channels, rate);
            }
            opus_decoder_destroy(decoder);
            opus_encoder_destroy(reference);
            Require(sceOpusSilkEncDestroy(state.data()) == 0);
        }
    }
    Require(sceOpusSilkEncDestroy(invalidState.data()) == static_cast<int>(errorBase + 1));

    std::array<std::uint8_t, 65536> floatState{};
    Require(sceOpusSilkEncCreateEx(&context, floatState.data(), 48000, 1, application) == 0);
    for (int index = 0; index < 960; ++index)
        floatSamples[static_cast<std::size_t>(index)] = static_cast<float>(0.2 * std::sin(2 * 3.14159265358979323846 * 2000 * index / 48000.0));
    const int floatResult = sceOpusSilkEncEncodeFloat(floatState.data(), floatSamples.data(), 960, packet, static_cast<int>(sizeof(packet)));
    Require(floatResult > 2 && static_cast<int>(packet[0]) >> 3 < 12);
    Require(sceOpusSilkEncDestroy(floatState.data()) == 0);

    std::array<std::uint8_t, 65536> liveState{};
    Require(sceOpusSilkEncCreateEx(&context, liveState.data(), 48000, 2, application) == 0);
    RequireThrows([&] { sceOpusSilkEncCtl(liveState.data(), 1); });
    RequireThrows([&] { sceOpusSilkEncCancel(liveState.data(), 0); });
    RequireThrows([&] { sceOpusSilkEncEncodeWithPriority(liveState.data(), samples.data(), 960, packet, sizeof(packet), 0); });
    RequireThrows([&] { sceOpusSilkEncEncodeFloatWithPriority(liveState.data(), floatSamples.data(), 960, packet, sizeof(packet), 0); });
    RequireThrows([&] { sceOpusSilkEncEncodeWithPriorityAndTimeout(liveState.data(), samples.data(), 960, packet, sizeof(packet), 0, 0, nullptr); });
    RequireThrows([&] { sceOpusSilkEncEncodeFloatWithPriorityAndTimeout(liveState.data(), floatSamples.data(), 960, packet, sizeof(packet), 0, 0, nullptr); });
    RequireThrows([&] { sceOpusSilkEncTerminate(&context); });
    Require(sceOpusSilkEncDestroy(liveState.data()) == 0);
    Require(sceOpusSilkEncTerminate(&context) == 0);
    RequireThrows([&] { sceOpusSilkEncTerminate(&context); });
    RequireThrows([&] { sceOpusSilkEncCreateEx(&context, liveState.data(), 48000, 2, application); });
    Require(sceOpusSilkEncCreate(liveState.data(), 48000, 2, application) == 0);
    Require(sceOpusSilkEncDestroy(liveState.data()) == 0);
}

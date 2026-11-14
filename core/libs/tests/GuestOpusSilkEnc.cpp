#include "prx/libSceOpusSilkEnc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <opus.h>

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

template <std::size_t Size>
OpusSilkEncoder* ToState(std::array<std::uint8_t, Size>& block) {
    return reinterpret_cast<OpusSilkEncoder*>(block.data());
}

std::uint32_t Word(const std::uint8_t* data, std::size_t offset) {
    std::uint32_t word = 0;
    std::memcpy(&word, data + offset, sizeof word);
    return word;
}

struct SetupWord {
    std::size_t offset;
    std::uint32_t value;
};

void RequireState(const std::uint8_t* state, std::size_t stateSize, const SetupWord* words, std::size_t wordCount, std::uint8_t canary) {
    std::array<bool, 1024> covered{};
    for (std::size_t index = 0; index < wordCount; ++index) {
        Require(words[index].offset + 4 <= stateSize);
        Require(Word(state, words[index].offset) == words[index].value);
        for (std::size_t byte = 0; byte < 4; ++byte) covered[words[index].offset + byte] = true;
    }
    for (std::size_t index = 0; index < stateSize; ++index)
        if (!covered[index]) Require(state[index] == 0);
    Require(state[stateSize] == canary);
}

std::uint32_t Bitrate(int rate, int channels) {
    if (rate == 8000) return channels == 1 ? 12000u : 19000u;
    if (rate == 12000) return channels == 1 ? 15000u : 27000u;
    if (rate == 16000) return channels == 1 ? 19000u : 35000u;
    if (rate == 24000) return channels == 1 ? 27000u : 51000u;
    return channels == 1 ? 51000u : 99000u;
}

std::array<SetupWord, 17> SetupWords(std::uint32_t instanceId, int rate, int channels) {
    const auto bandwidth = Bitrate(rate, channels);
    const auto rateWord = static_cast<std::uint32_t>(rate);
    const auto frameWord = static_cast<std::uint32_t>(rate / 50);
    const auto channelWord = static_cast<std::uint32_t>(channels);
    return {{
        {0x00, instanceId},
        {0x08, 8u * channelWord},
        {0x10, 0x800u},
        {0x14, rateWord},
        {0x18, channelWord},
        {0x1C, bandwidth},
        {0x28, 0x44Fu},
        {0x2C, 0x44Fu},
        {0x30, 0x5E0u},
        {0x34, 10u},
        {0x48, 0x3E8u},
        {0x58, frameWord},
        {0xE4, frameWord},
        {0xD8, 0xFFFFFC18u},
        {0xDC, bandwidth},
        {0xE0, 0x10u},
        {0xE8, 0x17B2u},
    }};
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

void SetWord(std::uint8_t* data, std::size_t offset, std::uint32_t value) {
    std::memcpy(data + offset, &value, sizeof value);
}

int EncoderError(std::uint32_t offset) {
    return static_cast<int>(0x81770000u + offset);
}

void RequireEarlyAnswers(int callerApplication) {
    std::array<std::uint8_t, 1024> blank{};
    std::array<std::uint8_t, 1024> wideMarker{};
    SetWord(wideMarker.data(), 0xE8, 0x000117B2u);
    SetWord(wideMarker.data(), 0x14, 48000u);
    std::array<std::uint8_t, 1024> marked{};
    SetWord(marked.data(), 0xE8, 0x17B2u);
    SetWord(marked.data(), 0x14, 16000u);
    std::array<std::int16_t, 5760> pcm{};
    std::array<float, 5760> floatPcm{};
    std::array<std::uint8_t, 1500> packet{};
    std::uint32_t batchId = 0;

    const auto encode = [&](OpusSilkEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
        const int result = sceOpusSilkEncEncodeWithPriority(state, samples, frameSize, data, maxDataBytes, priority);
        Require(sceOpusSilkEncEncodeWithPriorityAndTimeout(state, samples, frameSize, data, maxDataBytes, priority, 0xFFFFFFFFu, &batchId) == result);
        return result;
    };
    const auto encodeFloat = [&](OpusSilkEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
        const int result = sceOpusSilkEncEncodeFloatWithPriority(state, samples, frameSize, data, maxDataBytes, priority);
        Require(sceOpusSilkEncEncodeFloatWithPriorityAndTimeout(state, samples, frameSize, data, maxDataBytes, priority, 0xFFFFFFFFu, &batchId) == result);
        return result;
    };

    Require(encode(nullptr, nullptr, 0, nullptr, 0, 0) == EncoderError(0x01));
    Require(encode(ToState(blank), nullptr, 0, nullptr, 0, 0) == EncoderError(0x02));
    Require(encode(ToState(blank), pcm.data(), 0, nullptr, 0, 0) == EncoderError(0x04));
    Require(encode(ToState(blank), pcm.data(), 0, packet.data(), 0, 0) == EncoderError(0x05));
    for (const std::uint32_t priority : std::array<std::uint32_t, 5>{0u, 19u, 101u, 0x80000000u, 0xFFFFFFFFu}) {
        Require(encode(ToState(blank), pcm.data(), 0, packet.data(), -1, priority) == EncoderError(0x14));
        Require(encode(ToState(marked), pcm.data(), 7, packet.data(), 1500, priority) == EncoderError(0x14));
    }
    for (const std::uint32_t priority : std::array<std::uint32_t, 3>{20u, 60u, 100u}) {
        Require(encode(ToState(blank), pcm.data(), 960, packet.data(), 1500, priority) == EncoderError(0x15));
        Require(encode(ToState(wideMarker), pcm.data(), 960, packet.data(), 1500, priority) == EncoderError(0x15));
        for (const int frameSize : std::array<int, 7>{0, 40, 80, 159, 1920, -160, 2147483647})
            Require(encode(ToState(marked), pcm.data(), frameSize, packet.data(), 1500, priority) == EncoderError(0x03));
    }
    std::array<std::uint8_t, 1024> live{};
    Require(sceOpusSilkEncCreate(ToState(live), 48000, 2, callerApplication) == 0);
    for (const int frameSize : std::array<int, 3>{120, 240, 1000})
        Require(encode(ToState(live), pcm.data(), frameSize, packet.data(), 1500, 50) == EncoderError(0x03));
    Require(encode(ToState(live), pcm.data(), 960, packet.data(), 1500, 10) == EncoderError(0x14));
    Require(sceOpusSilkEncCtl(ToState(live), 3999) == EncoderError(0x07));
    Require(sceOpusSilkEncDestroy(ToState(live)) == 0);

    Require(encodeFloat(nullptr, nullptr, 0, nullptr, 0, 0) == EncoderError(0x01));
    Require(encodeFloat(ToState(blank), nullptr, 0, nullptr, 0, 0) == EncoderError(0x02));
    Require(encodeFloat(ToState(blank), floatPcm.data(), 0, nullptr, 0, 0) == EncoderError(0x03));
    Require(encodeFloat(ToState(marked), floatPcm.data(), 0, packet.data(), 1500, 50) == EncoderError(0x03));
    Require(encodeFloat(ToState(blank), floatPcm.data(), 960, nullptr, 0, 0) == EncoderError(0x15));
    Require(encodeFloat(ToState(wideMarker), floatPcm.data(), 960, packet.data(), 1500, 50) == EncoderError(0x15));

    Require(sceOpusSilkEncCtl(nullptr, 4000) == EncoderError(0x01));
    Require(sceOpusSilkEncCtl(ToState(blank), 4000) == EncoderError(0x15));
    Require(sceOpusSilkEncCtl(ToState(wideMarker), 4000) == EncoderError(0x15));
    for (const int request : std::array<int, 9>{0, -3, 3999, 4032, 4047, 11001, 11004, 2147483647, -2147483647 - 1})
        Require(sceOpusSilkEncCtl(ToState(marked), request) == EncoderError(0x07));

    Require(sceOpusSilkEncCancel(nullptr, 0) == EncoderError(0x01));
    Require(sceOpusSilkEncCancel(nullptr, 0xFFFFFFFFu) == EncoderError(0x01));

    std::uint32_t ajmContext = 0x12345678u;
    Require(sceOpusSilkEncGetAjmContext(nullptr, &ajmContext) == EncoderError(0x01));
    Require(sceOpusSilkEncGetAjmContext(nullptr, nullptr) == EncoderError(0x01));
    Require(sceOpusSilkEncGetAjmContext(ToState(blank), nullptr) == 0);
    Require(sceOpusSilkEncGetAjmContext(ToState(marked), nullptr) == 0);
    Require(sceOpusSilkEncGetAjmContext(reinterpret_cast<const OpusSilkEncoder*>(0x10), nullptr) == 0);
    Require(ajmContext == 0x12345678u);
    Require(sceOpusSilkEncGetAjmContext(ToState(blank), &ajmContext) == 0);
    Require(ajmContext == 0);
    SetWord(marked.data(), 0xF0, 0x6B7C8D9Eu);
    std::array<std::uint32_t, 3> contextWords{0xBAD0C0DEu, 0u, 0xBAD0C0DEu};
    Require(sceOpusSilkEncGetAjmContext(ToState(marked), &contextWords[1]) == 0);
    Require(contextWords == std::array<std::uint32_t, 3>{0xBAD0C0DEu, 0x6B7C8D9Eu, 0xBAD0C0DEu});
    errno = 71;
    RequireThrows([&] { sceOpusSilkEncGetAjmContext(reinterpret_cast<const OpusSilkEncoder*>(0x10), &ajmContext); });
    RequireThrows([&] { sceOpusSilkEncGetAjmContext(ToState(marked), reinterpret_cast<std::uint32_t*>(0x10)); });
    Require(errno == 71);
}

} // namespace

int main() {
    constexpr std::uint32_t errorBase = 0x81770000u;
    constexpr std::size_t stateSize = 752;
    constexpr int callerApplication = OPUS_APPLICATION_VOIP;
    constexpr int codecApplication = OPUS_APPLICATION_RESTRICTED_SILK;
    std::uint32_t instanceId = 1;
    Require(sceOpusSilkEncInitialize(nullptr) == static_cast<int>(errorBase + 0x18));
    errno = 71;
    Require(sceOpusSilkEncInitialize(reinterpret_cast<std::uint32_t*>(0x10)) == static_cast<int>(errorBase + 0x18));
    Require(errno == 71);
    Require(sceOpusSilkEncDestroy(nullptr) == static_cast<int>(errorBase + 0x01));
    std::uint32_t context = 0;
    Require(sceOpusSilkEncInitialize(&context) == 0);
    RequireThrows([&] { sceOpusSilkEncInitialize(&context); });
    RequireThrows([&] { sceOpusSilkEncTerminate(nullptr); });

    std::array<std::uint8_t, 65536> invalidState{};
    std::array<std::int16_t, 5760> samples{};
    samples.fill(12345);
    std::array<float, 5760> floatSamples{};
    std::uint8_t packet[1024];
    for (auto& byte : packet) byte = 0x5A;
    Require(sceOpusSilkEncCreate(nullptr, 48000, 2, callerApplication) == static_cast<int>(errorBase + 0x01));
    Require(sceOpusSilkEncCreate(ToState(invalidState), 48000, 0, callerApplication) == static_cast<int>(errorBase + 0x0C));
    Require(sceOpusSilkEncCreate(ToState(invalidState), 0, 0, callerApplication) == static_cast<int>(errorBase + 0x0F));
    Require(sceOpusSilkEncCreate(ToState(invalidState), 22050, 2, callerApplication) == static_cast<int>(errorBase + 0x0F));
    Require(sceOpusSilkEncCreate(ToState(invalidState), 48000, 2, OPUS_APPLICATION_RESTRICTED_LOWDELAY) == static_cast<int>(errorBase + 0x10));
    Require(sceOpusSilkEncCreateEx(nullptr, ToState(invalidState), 0, 0, callerApplication) == static_cast<int>(errorBase + 0x01));
    Require(sceOpusSilkEncCreateEx(&context, ToState(invalidState), 0, 0, callerApplication) == static_cast<int>(errorBase + 0x0F));
    Require(sceOpusSilkEncCreateEx(&context, ToState(invalidState), 48000, 0, callerApplication) == static_cast<int>(errorBase + 0x0C));
    Require(sceOpusSilkEncCreateEx(&context, ToState(invalidState), 48000, 2, OPUS_APPLICATION_RESTRICTED_LOWDELAY) == static_cast<int>(errorBase + 0x10));
    Require(sceOpusSilkEncGetSize(1) == 752);
    Require(sceOpusSilkEncGetSize(2) == 752);
    Require(sceOpusSilkEncGetSize(3) == static_cast<int>(errorBase + 0x0C));

    std::array<std::uint8_t, 65536> state{};
    state.fill(0x5A);
    Require(sceOpusSilkEncCreateEx(&context, ToState(state), 48000, 2, callerApplication) == 0);
    const auto words = SetupWords(instanceId, 48000, 2);
    RequireState(state.data(), stateSize, words.data(), words.size(), 0x5A);
    ++instanceId;
    Require(sceOpusSilkEncEncode(nullptr, samples.data(), 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x01));
    Require(sceOpusSilkEncEncode(ToState(state), nullptr, 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x02));
    Require(sceOpusSilkEncEncode(ToState(state), nullptr, 0, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x02));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), 0, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), -1, nullptr, sizeof(packet)) == static_cast<int>(errorBase + 0x04));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), -1, packet, 0) == static_cast<int>(errorBase + 0x05));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), -1, nullptr, 0) == static_cast<int>(errorBase + 0x04));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), 1, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), 119, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), 959, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), 5760, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), 120, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), 240, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(packet[0] == 0x5A && packet[1] == 0x5A);
    RequireThrows([&] { sceOpusSilkEncEncode(ToState(state), samples.data(), 960, packet, -1); });
    Require(sceOpusSilkEncEncodeFloat(nullptr, floatSamples.data(), 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x01));
    Require(sceOpusSilkEncEncodeFloat(ToState(state), nullptr, 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x02));
    Require(sceOpusSilkEncEncodeFloat(ToState(state), floatSamples.data(), 0, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusSilkEncEncodeFloat(ToState(invalidState), floatSamples.data(), -1, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x15));
    Require(sceOpusSilkEncEncodeFloat(ToState(invalidState), nullptr, 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x02));
    RequireThrows([&] { sceOpusSilkEncEncodeFloat(ToState(state), floatSamples.data(), -1, packet, sizeof(packet)); });
    RequireThrows([&] { sceOpusSilkEncEncodeFloat(ToState(state), floatSamples.data(), 1100000, packet, sizeof(packet)); });

    auto snapshot = state;
    Require(sceOpusSilkEncDestroy(ToState(state)) == 0);
    for (std::size_t index = 0; index < stateSize; ++index)
        if (index < 0xE8 || index >= 0xEC) Require(state[index] == snapshot[index]);
    Require(Word(state.data(), 0xE8) == 0);
    Require(sceOpusSilkEncDestroy(ToState(state)) == static_cast<int>(errorBase + 0x15));
    Require(sceOpusSilkEncEncode(ToState(state), samples.data(), 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x15));
    Require(sceOpusSilkEncEncodeFloat(ToState(state), floatSamples.data(), 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x15));

    for (int rate : {8000, 12000, 16000, 24000, 48000}) {
        for (int channels : {1, 2}) {
            std::array<std::uint8_t, 65536> matrixState{};
            matrixState.fill(0x5A);
            Require(sceOpusSilkEncCreateEx(&context, ToState(matrixState), rate, channels, callerApplication) == 0);
            const auto matrixWords = SetupWords(instanceId, rate, channels);
            RequireState(matrixState.data(), stateSize, matrixWords.data(), matrixWords.size(), 0x5A);
            ++instanceId;
            int opusError = OPUS_OK;
            auto* reference = opus_encoder_create(rate, channels, codecApplication, &opusError);
            Require(reference && opusError == OPUS_OK);
            const int frameSize = rate / 50;
            for (int frame = 0; frame < 4; ++frame) {
                FillSine(samples.data(), frameSize, channels, rate, frame * frameSize);
                const int bytes = sceOpusSilkEncEncode(ToState(matrixState), samples.data(), frameSize, packet, sizeof(packet));
                Require(bytes > 2 && static_cast<int>(packet[0]) >> 3 < 12);
                std::array<std::uint8_t, 1024> expected{};
                const int expectedBytes = opus_encode(reference, samples.data(), frameSize, expected.data(), sizeof(expected));
                Require(expectedBytes == bytes && std::equal(packet, packet + bytes, expected.begin()));
            }
            int decodeError = OPUS_OK;
            auto* decoder = opus_decoder_create(rate, channels, &decodeError);
            Require(decoder && decodeError == OPUS_OK);
            std::array<std::int16_t, 5762> pcm{};
            for (int frame = 0; frame < 4; ++frame) {
                FillSine(samples.data(), frameSize, channels, rate, frame * frameSize);
                const int bytes = sceOpusSilkEncEncode(ToState(matrixState), samples.data(), frameSize, packet, sizeof(packet));
                pcm.fill(12345);
                Require(opus_decode(decoder, packet, bytes, pcm.data() + 1, frameSize, 0) == frameSize);
                Require(pcm.front() == 12345 && pcm.back() == 12345);
                RequireTone(pcm.data() + 1, frameSize, channels, rate);
            }
            opus_decoder_destroy(decoder);
            opus_encoder_destroy(reference);
            Require(sceOpusSilkEncDestroy(ToState(matrixState)) == 0);
        }
    }
    Require(sceOpusSilkEncDestroy(ToState(invalidState)) == static_cast<int>(errorBase + 0x15));

    std::array<std::uint8_t, 65536> retireState{};
    retireState.fill(0x5A);
    Require(sceOpusSilkEncCreateEx(&context, ToState(retireState), 48000, 1, callerApplication) == 0);
    ++instanceId;
    Require(sceOpusSilkEncCreateEx(&context, ToState(retireState), 48000, 2, callerApplication) == 0);
    const auto retireWords = SetupWords(instanceId, 48000, 2);
    RequireState(retireState.data(), stateSize, retireWords.data(), retireWords.size(), 0x5A);
    ++instanceId;
    Require(sceOpusSilkEncEncode(ToState(retireState), samples.data(), 960, packet, sizeof(packet)) > 2);
    Require(sceOpusSilkEncDestroy(ToState(retireState)) == 0);

    std::array<std::uint8_t, 65536> wideRateState{};
    Require(sceOpusSilkEncCreateEx(&context, ToState(wideRateState), 48000, 1, callerApplication) == 0);
    ++instanceId;
    const std::uint32_t wideRate = 0x40000064u;
    std::memcpy(wideRateState.data() + 0x14, &wideRate, sizeof wideRate);
    for (const int wrappedFrame : {1, 2, 4, 6}) {
        Require(sceOpusSilkEncEncode(ToState(wideRateState), samples.data(), wrappedFrame, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
        Require(sceOpusSilkEncEncodeFloat(ToState(wideRateState), floatSamples.data(), wrappedFrame, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    }
    Require(sceOpusSilkEncDestroy(ToState(wideRateState)) == 0);

    std::array<std::uint8_t, 65536> floatState{};
    floatState.fill(0x5A);
    Require(sceOpusSilkEncCreateEx(&context, ToState(floatState), 48000, 1, callerApplication) == 0);
    const auto floatWords = SetupWords(instanceId, 48000, 1);
    RequireState(floatState.data(), stateSize, floatWords.data(), floatWords.size(), 0x5A);
    ++instanceId;
    for (int index = 0; index < 960; ++index)
        floatSamples[static_cast<std::size_t>(index)] = static_cast<float>(0.2 * std::sin(2 * 3.14159265358979323846 * 2000 * index / 48000.0));
    const int floatResult = sceOpusSilkEncEncodeFloat(ToState(floatState), floatSamples.data(), 960, packet, static_cast<int>(sizeof(packet)));
    Require(floatResult > 2 && static_cast<int>(packet[0]) >> 3 < 12);
    int floatOpusError = OPUS_OK;
    auto* floatReference = opus_encoder_create(48000, 1, codecApplication, &floatOpusError);
    Require(floatReference && floatOpusError == OPUS_OK);
    std::array<std::uint8_t, 1024> floatExpected{};
    const int floatExpectedBytes = opus_encode_float(floatReference, floatSamples.data(), 960, floatExpected.data(), sizeof(floatExpected));
    Require(floatExpectedBytes == floatResult && std::equal(packet, packet + floatResult, floatExpected.begin()));
    opus_encoder_destroy(floatReference);
    Require(sceOpusSilkEncDestroy(ToState(floatState)) == 0);

    std::array<std::uint8_t, 65536> liveState{};
    Require(sceOpusSilkEncCreateEx(&context, ToState(liveState), 48000, 2, callerApplication) == 0);
    ++instanceId;
    Require(sceOpusSilkEncDestroy(ToState(liveState)) == 0);
    Require(sceOpusSilkEncTerminate(&context) == 0);
    RequireThrows([&] { sceOpusSilkEncTerminate(&context); });
    RequireThrows([&] { sceOpusSilkEncCreateEx(&context, ToState(liveState), 48000, 2, callerApplication); });
    Require(sceOpusSilkEncCreate(ToState(liveState), 48000, 1, callerApplication) == 0);
    ++instanceId;
    Require(sceOpusSilkEncCreate(ToState(liveState), 48000, 2, callerApplication) == 0);
    ++instanceId;
    Require(sceOpusSilkEncDestroy(ToState(liveState)) == 0);
    RequireEarlyAnswers(callerApplication);
}

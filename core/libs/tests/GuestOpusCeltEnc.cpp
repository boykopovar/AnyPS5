#include "prx/libSceOpusCeltEnc/include/Api.hpp"
#include <cerrno>
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
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
OpusCeltEncoder* ToState(std::array<std::uint8_t, Size>& block) {
    return reinterpret_cast<OpusCeltEncoder*>(block.data());
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
    if (rate == 8000) return channels == 1 ? 11000u : 19000u;
    if (rate == 12000) return channels == 1 ? 15000u : 27000u;
    if (rate == 16000) return channels == 1 ? 19000u : 35000u;
    if (rate == 24000) return channels == 1 ? 27000u : 51000u;
    return channels == 1 ? 51000u : 99000u;
}

std::array<SetupWord, 21> SetupWords(std::uint32_t instanceId, int rate, int channels) {
    const auto bandwidth = Bitrate(rate, channels);
    const auto rateWord = static_cast<std::uint32_t>(rate);
    const auto frameWord = static_cast<std::uint32_t>(rate / 50);
    const auto channelWord = static_cast<std::uint32_t>(channels);
    return {{
        {0x00, instanceId},
        {0x08, 8u * channelWord},
        {0x0C, 2u},
        {0x10, channelWord},
        {0x14, rateWord},
        {0x1C, bandwidth},
        {0x20, 2u},
        {0x24, 0x451u},
        {0x28, 0x5E0u},
        {0x2C, 10u},
        {0x40, frameWord},
        {0xA8, 0x803u},
        {0xAC, 0x451u},
        {0xB0, 0x451u},
        {0xB4, 1u},
        {0xB8, 1u},
        {0xBC, 0xFFFFFC18u},
        {0xC0, bandwidth},
        {0xC4, 0x10u},
        {0xC8, frameWord},
        {0xCC, 0x17B2u},
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

template <std::size_t Size>
OpusCeltMultistreamEncoder* ToMultistreamState(std::array<std::uint8_t, Size>& block) {
    return reinterpret_cast<OpusCeltMultistreamEncoder*>(block.data());
}

void SetWord(std::uint8_t* data, std::size_t offset, std::uint32_t value) {
    std::memcpy(data + offset, &value, sizeof value);
}

int EncoderError(std::uint32_t offset) {
    return static_cast<int>(0x813F0000u + offset);
}

void RequireEarlyAnswers(int callerApplication) {
    std::array<std::uint8_t, 1024> blank{};
    std::array<std::uint8_t, 1024> wideMarker{};
    SetWord(wideMarker.data(), 0xCC, 0x000117B2u);
    SetWord(wideMarker.data(), 0x14, 48000u);
    std::array<std::uint8_t, 1024> marked{};
    SetWord(marked.data(), 0xCC, 0x17B2u);
    SetWord(marked.data(), 0x14, 16000u);
    std::array<std::int16_t, 5760> pcm{};
    std::array<float, 5760> floatPcm{};
    std::array<std::uint8_t, 1500> packet{};

    Require(sceOpusCeltEncEncodeWithPriority(nullptr, nullptr, 0, nullptr, 0, 0) == EncoderError(0x01));
    Require(sceOpusCeltEncEncodeWithPriority(ToState(blank), nullptr, 0, nullptr, 0, 0) == EncoderError(0x02));
    Require(sceOpusCeltEncEncodeWithPriority(ToState(blank), pcm.data(), 0, nullptr, 0, 0) == EncoderError(0x04));
    Require(sceOpusCeltEncEncodeWithPriority(ToState(blank), pcm.data(), 0, packet.data(), 0, 0) == EncoderError(0x05));
    for (const std::uint32_t priority : std::array<std::uint32_t, 5>{0u, 19u, 101u, 0x80000000u, 0xFFFFFFFFu}) {
        Require(sceOpusCeltEncEncodeWithPriority(ToState(blank), pcm.data(), 0, packet.data(), -1, priority) == EncoderError(0x15));
        Require(sceOpusCeltEncEncodeWithPriority(ToState(marked), pcm.data(), 7, packet.data(), 1500, priority) == EncoderError(0x15));
    }
    for (const std::uint32_t priority : std::array<std::uint32_t, 3>{20u, 60u, 100u}) {
        Require(sceOpusCeltEncEncodeWithPriority(ToState(blank), pcm.data(), 960, packet.data(), 1500, priority) == EncoderError(0x16));
        Require(sceOpusCeltEncEncodeWithPriority(ToState(wideMarker), pcm.data(), 960, packet.data(), 1500, priority) == EncoderError(0x16));
        for (const int frameSize : std::array<int, 6>{0, 39, 41, 1920, -40, 2147483647})
            Require(sceOpusCeltEncEncodeWithPriority(ToState(marked), pcm.data(), frameSize, packet.data(), 1500, priority) == EncoderError(0x03));
    }
    std::array<std::uint8_t, 1024> live{};
    Require(sceOpusCeltEncCreate(ToState(live), 48000, 2, callerApplication) == 0);
    Require(sceOpusCeltEncEncodeWithPriority(ToState(live), pcm.data(), 1000, packet.data(), 1500, 50) == EncoderError(0x03));
    Require(sceOpusCeltEncEncodeWithPriority(ToState(live), pcm.data(), 960, packet.data(), 1500, 10) == EncoderError(0x15));
    Require(sceOpusCeltEncCtl(ToState(live), 3999) == EncoderError(0x08));
    Require(sceOpusCeltEncDestroy(ToState(live)) == 0);

    Require(sceOpusCeltEncCtl(nullptr, 4000) == EncoderError(0x01));
    Require(sceOpusCeltEncCtl(ToState(blank), 4000) == EncoderError(0x16));
    Require(sceOpusCeltEncCtl(ToState(wideMarker), 4000) == EncoderError(0x16));
    for (const int request : std::array<int, 6>{0, 3999, 4048, -3, 2147483647, -2147483647 - 1})
        Require(sceOpusCeltEncCtl(ToState(marked), request) == EncoderError(0x08));

    Require(sceOpusCeltEncEncodeFloatWithPriority(nullptr, nullptr, 0, nullptr, 0, 0) == EncoderError(0x01));
    Require(sceOpusCeltEncEncodeFloatWithPriority(ToState(blank), nullptr, 0, nullptr, 0, 0) == EncoderError(0x02));
    Require(sceOpusCeltEncEncodeFloatWithPriority(ToState(blank), floatPcm.data(), 0, nullptr, 0, 0) == EncoderError(0x03));
    Require(sceOpusCeltEncEncodeFloatWithPriority(ToState(marked), floatPcm.data(), 0, packet.data(), 1500, 50) == EncoderError(0x03));
    Require(sceOpusCeltEncEncodeFloatWithPriority(ToState(blank), floatPcm.data(), 960, nullptr, 0, 0) == EncoderError(0x16));
    Require(sceOpusCeltEncEncodeFloatWithPriority(ToState(wideMarker), floatPcm.data(), 960, packet.data(), 1500, 50) == EncoderError(0x16));

    std::uint32_t ajmContext = 0x12345678u;
    Require(sceOpusCeltEncGetAjmContext(nullptr, &ajmContext) == EncoderError(0x01));
    Require(sceOpusCeltEncGetAjmContext(nullptr, nullptr) == EncoderError(0x01));
    Require(sceOpusCeltEncGetAjmContext(ToState(blank), nullptr) == 0);
    Require(sceOpusCeltEncGetAjmContext(ToState(marked), nullptr) == 0);
    Require(sceOpusCeltEncGetAjmContext(reinterpret_cast<const OpusCeltEncoder*>(0x10), nullptr) == 0);
    Require(ajmContext == 0x12345678u);
    Require(sceOpusCeltEncGetAjmContext(ToState(blank), &ajmContext) == 0);
    Require(ajmContext == 0);
    SetWord(marked.data(), 0xD4, 0x0A1B2C3Du);
    std::array<std::uint32_t, 3> contextWords{0xBAD0C0DEu, 0u, 0xBAD0C0DEu};
    Require(sceOpusCeltEncGetAjmContext(ToState(marked), &contextWords[1]) == 0);
    Require(contextWords == std::array<std::uint32_t, 3>{0xBAD0C0DEu, 0x0A1B2C3Du, 0xBAD0C0DEu});
    errno = 71;
    RequireThrows([&] { sceOpusCeltEncGetAjmContext(reinterpret_cast<const OpusCeltEncoder*>(0x10), &ajmContext); });
    RequireThrows([&] { sceOpusCeltEncGetAjmContext(ToState(marked), reinterpret_cast<std::uint32_t*>(0x10)); });
    Require(errno == 71);

    std::array<std::uint8_t, 1024> multistream{};
    SetWord(multistream.data(), 0x98, 0x17B2u);
    std::array<std::uint8_t, 1024> wideMultistream{};
    SetWord(wideMultistream.data(), 0x98, 0x000117B2u);
    Require(sceOpusCeltMultistreamEncCtl(nullptr, 4000) == EncoderError(0x01));
    Require(sceOpusCeltMultistreamEncCtl(ToMultistreamState(blank), 4000) == EncoderError(0x16));
    Require(sceOpusCeltMultistreamEncCtl(ToMultistreamState(marked), 4000) == EncoderError(0x16));
    Require(sceOpusCeltMultistreamEncCtl(ToMultistreamState(wideMultistream), 4000) == EncoderError(0x16));
    for (const int request : std::array<int, 6>{0, 3999, 4048, -2, 2147483647, -2147483647 - 1})
        Require(sceOpusCeltMultistreamEncCtl(ToMultistreamState(multistream), request) == EncoderError(0x08));

    const std::uint32_t context = 1;
    constexpr int lowDelay = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
    const std::array<std::uint8_t, 12> identity{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    Require(sceOpusCeltMultistreamEncCreateEx(nullptr, ToMultistreamState(blank), 48000, 2, 1, identity.data(), lowDelay) == EncoderError(0x01));
    Require(sceOpusCeltMultistreamEncCreateEx(&context, nullptr, 48000, 2, 1, identity.data(), lowDelay) == EncoderError(0x01));
    Require(sceOpusCeltMultistreamEncCreate(nullptr, 48000, 2, 1, identity.data(), lowDelay) == EncoderError(0x01));
    for (const int rate : std::array<int, 5>{0, 7999, 44100, 96000, -48000}) {
        Require(sceOpusCeltMultistreamEncCreateEx(&context, ToMultistreamState(blank), rate, 0, 13, nullptr, 0) == EncoderError(0x10));
        Require(sceOpusCeltMultistreamEncCreate(ToMultistreamState(blank), rate, 2, 1, identity.data(), lowDelay) == EncoderError(0x10));
    }
    for (const int streams : std::array<int, 4>{0, 13, -1, 2147483647}) {
        Require(sceOpusCeltMultistreamEncCreateEx(&context, ToMultistreamState(blank), 48000, streams, 0, nullptr, 0) == EncoderError(0x1B));
        Require(sceOpusCeltMultistreamEncCreate(ToMultistreamState(blank), 8000, streams, 0, identity.data(), lowDelay) == EncoderError(0x1B));
    }
    for (const auto& counts : std::array<std::array<int, 2>, 3>{{{2, 3}, {1, -1}, {12, 13}}}) {
        Require(sceOpusCeltMultistreamEncCreateEx(&context, ToMultistreamState(blank), 24000, counts[0], counts[1], nullptr, 0) == EncoderError(0x1C));
        Require(sceOpusCeltMultistreamEncCreate(ToMultistreamState(blank), 12000, counts[0], counts[1], identity.data(), lowDelay) == EncoderError(0x1C));
    }
    for (const auto& counts : std::array<std::array<int, 2>, 3>{{{8, 5}, {12, 1}, {7, 6}}}) {
        Require(sceOpusCeltMultistreamEncCreateEx(&context, ToMultistreamState(blank), 16000, counts[0], counts[1], nullptr, 0) == EncoderError(0x0D));
        Require(sceOpusCeltMultistreamEncCreate(ToMultistreamState(blank), 48000, counts[0], counts[1], identity.data(), lowDelay) == EncoderError(0x0D));
    }
    const std::array<std::array<std::uint8_t, 3>, 4> badMappings{{{0, 0, 2}, {0, 1, 3}, {1, 2, 2}, {255, 1, 0}}};
    for (const auto& mapping : badMappings) {
        Require(sceOpusCeltMultistreamEncCreateEx(&context, ToMultistreamState(blank), 48000, 2, 1, mapping.data(), 0) == EncoderError(0x1D));
        Require(sceOpusCeltMultistreamEncCreate(ToMultistreamState(blank), 48000, 2, 1, mapping.data(), lowDelay) == EncoderError(0x1D));
    }
    std::array<std::uint8_t, 12> lastMissing = identity;
    lastMissing[5] = 12;
    Require(sceOpusCeltMultistreamEncCreateEx(&context, ToMultistreamState(blank), 48000, 6, 6, lastMissing.data(), lowDelay) == EncoderError(0x1D));
    const std::array<std::uint8_t, 3> reversed{2, 1, 0};
    for (const int application : std::array<int, 4>{0, OPUS_APPLICATION_VOIP, OPUS_APPLICATION_AUDIO, OPUS_APPLICATION_RESTRICTED_CELT}) {
        Require(sceOpusCeltMultistreamEncCreateEx(&context, ToMultistreamState(blank), 48000, 2, 1, reversed.data(), application) == EncoderError(0x11));
        Require(sceOpusCeltMultistreamEncCreate(ToMultistreamState(blank), 24000, 12, 0, identity.data(), application) == EncoderError(0x11));
    }
}

} // namespace

int main() {
    constexpr std::uint32_t errorBase = 0x813F0000u;
    constexpr std::size_t stateSize = 728;
    constexpr int callerApplication = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
    constexpr int codecApplication = OPUS_APPLICATION_RESTRICTED_CELT;
    std::uint32_t instanceId = 1;
    Require(sceOpusCeltEncInitialize(nullptr) == static_cast<int>(errorBase + 0x19));
    errno = 71;
    Require(sceOpusCeltEncInitialize(reinterpret_cast<std::uint32_t*>(0x10)) == static_cast<int>(errorBase + 0x19));
    Require(errno == 71);
    Require(sceOpusCeltEncDestroy(nullptr) == static_cast<int>(errorBase + 0x01));
    std::uint32_t context = 0;
    Require(sceOpusCeltEncInitialize(&context) == 0);
    RequireThrows([&] { sceOpusCeltEncInitialize(&context); });
    RequireThrows([&] { sceOpusCeltEncTerminate(nullptr); });

    std::array<std::uint8_t, 65536> invalidState{};
    std::array<std::int16_t, 5760> samples{};
    samples.fill(12345);
    std::array<float, 5760> floatSamples{};
    std::uint8_t packet[1024];
    for (auto& byte : packet) byte = 0x5A;
    Require(sceOpusCeltEncCreate(nullptr, 48000, 2, callerApplication) == static_cast<int>(errorBase + 0x01));
    Require(sceOpusCeltEncCreate(ToState(invalidState), 48000, 0, callerApplication) == static_cast<int>(errorBase + 0x0D));
    Require(sceOpusCeltEncCreate(ToState(invalidState), 0, 0, callerApplication) == static_cast<int>(errorBase + 0x10));
    Require(sceOpusCeltEncCreate(ToState(invalidState), 22050, 2, callerApplication) == static_cast<int>(errorBase + 0x10));
    Require(sceOpusCeltEncCreate(ToState(invalidState), 48000, 2, OPUS_APPLICATION_VOIP) == static_cast<int>(errorBase + 0x11));
    Require(sceOpusCeltEncCreateEx(nullptr, ToState(invalidState), 0, 0, callerApplication) == static_cast<int>(errorBase + 0x01));
    Require(sceOpusCeltEncCreateEx(&context, ToState(invalidState), 0, 0, callerApplication) == static_cast<int>(errorBase + 0x10));
    Require(sceOpusCeltEncCreateEx(&context, ToState(invalidState), 48000, 0, callerApplication) == static_cast<int>(errorBase + 0x0D));
    Require(sceOpusCeltEncCreateEx(&context, ToState(invalidState), 48000, 2, OPUS_APPLICATION_VOIP) == static_cast<int>(errorBase + 0x11));
    Require(sceOpusCeltEncGetSize(1) == 728);
    Require(sceOpusCeltEncGetSize(2) == 728);
    Require(sceOpusCeltEncGetSize(3) == static_cast<int>(errorBase + 0x0D));

    std::array<std::uint8_t, 65536> state{};
    state.fill(0x5A);
    Require(sceOpusCeltEncCreateEx(&context, ToState(state), 48000, 2, callerApplication) == 0);
    const auto words = SetupWords(instanceId, 48000, 2);
    RequireState(state.data(), stateSize, words.data(), words.size(), 0x5A);
    ++instanceId;
    Require(sceOpusCeltEncEncode(nullptr, samples.data(), 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x01));
    Require(sceOpusCeltEncEncode(ToState(state), nullptr, 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x02));
    Require(sceOpusCeltEncEncode(ToState(state), nullptr, 0, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x02));
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), 0, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), -1, nullptr, sizeof(packet)) == static_cast<int>(errorBase + 0x04));
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), -1, packet, 0) == static_cast<int>(errorBase + 0x05));
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), -1, nullptr, 0) == static_cast<int>(errorBase + 0x04));
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), 1, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), 119, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), 959, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), 5760, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(packet[0] == 0x5A && packet[1] == 0x5A);
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), 120, packet, sizeof(packet)) > 2);
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), 240, packet, sizeof(packet)) > 2);
    RequireThrows([&] { sceOpusCeltEncEncode(ToState(state), samples.data(), 960, packet, -1); });
    Require(sceOpusCeltEncEncodeFloat(nullptr, floatSamples.data(), 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x01));
    Require(sceOpusCeltEncEncodeFloat(ToState(state), nullptr, 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x02));
    Require(sceOpusCeltEncEncodeFloat(ToState(state), floatSamples.data(), 0, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    Require(sceOpusCeltEncEncodeFloat(ToState(invalidState), floatSamples.data(), -1, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x16));
    Require(sceOpusCeltEncEncodeFloat(ToState(invalidState), nullptr, 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x02));
    RequireThrows([&] { sceOpusCeltEncEncodeFloat(ToState(state), floatSamples.data(), -1, packet, sizeof(packet)); });
    RequireThrows([&] { sceOpusCeltEncEncodeFloat(ToState(state), floatSamples.data(), 1100000, packet, sizeof(packet)); });

    auto snapshot = state;
    Require(sceOpusCeltEncDestroy(ToState(state)) == 0);
    for (std::size_t index = 0; index < stateSize; ++index)
        if (index < 0xCC || index >= 0xD0) Require(state[index] == snapshot[index]);
    Require(Word(state.data(), 0xCC) == 0);
    Require(sceOpusCeltEncDestroy(ToState(state)) == static_cast<int>(errorBase + 0x16));
    Require(sceOpusCeltEncEncode(ToState(state), samples.data(), 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x16));
    Require(sceOpusCeltEncEncodeFloat(ToState(state), floatSamples.data(), 960, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x16));

    for (int rate : {8000, 12000, 16000, 24000, 48000}) {
        for (int channels : {1, 2}) {
            std::array<std::uint8_t, 65536> matrixState{};
            matrixState.fill(0x5A);
            Require(sceOpusCeltEncCreateEx(&context, ToState(matrixState), rate, channels, callerApplication) == 0);
            const auto matrixWords = SetupWords(instanceId, rate, channels);
            RequireState(matrixState.data(), stateSize, matrixWords.data(), matrixWords.size(), 0x5A);
            ++instanceId;
            int opusError = OPUS_OK;
            auto* reference = opus_encoder_create(rate, channels, codecApplication, &opusError);
            Require(reference && opusError == OPUS_OK);
            const int frameSize = rate / 50;
            for (int frame = 0; frame < 4; ++frame) {
                FillSine(samples.data(), frameSize, channels, rate, frame * frameSize);
                const int bytes = sceOpusCeltEncEncode(ToState(matrixState), samples.data(), frameSize, packet, sizeof(packet));
                Require(bytes > 2 && static_cast<int>(packet[0]) >> 3 >= 16);
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
                const int bytes = sceOpusCeltEncEncode(ToState(matrixState), samples.data(), frameSize, packet, sizeof(packet));
                pcm.fill(12345);
                Require(opus_decode(decoder, packet, bytes, pcm.data() + 1, frameSize, 0) == frameSize);
                Require(pcm.front() == 12345 && pcm.back() == 12345);
                RequireTone(pcm.data() + 1, frameSize, channels, rate);
            }
            opus_decoder_destroy(decoder);
            opus_encoder_destroy(reference);
            Require(sceOpusCeltEncDestroy(ToState(matrixState)) == 0);
        }
    }
    Require(sceOpusCeltEncDestroy(ToState(invalidState)) == static_cast<int>(errorBase + 0x16));

    std::array<std::uint8_t, 65536> retireState{};
    retireState.fill(0x5A);
    Require(sceOpusCeltEncCreateEx(&context, ToState(retireState), 48000, 1, callerApplication) == 0);
    ++instanceId;
    Require(sceOpusCeltEncCreateEx(&context, ToState(retireState), 48000, 2, callerApplication) == 0);
    const auto retireWords = SetupWords(instanceId, 48000, 2);
    RequireState(retireState.data(), stateSize, retireWords.data(), retireWords.size(), 0x5A);
    ++instanceId;
    Require(sceOpusCeltEncEncode(ToState(retireState), samples.data(), 960, packet, sizeof(packet)) > 2);
    Require(sceOpusCeltEncDestroy(ToState(retireState)) == 0);

    std::array<std::uint8_t, 65536> wideRateState{};
    Require(sceOpusCeltEncCreateEx(&context, ToState(wideRateState), 48000, 1, callerApplication) == 0);
    ++instanceId;
    const std::uint32_t wideRate = 0x40000064u;
    std::memcpy(wideRateState.data() + 0x14, &wideRate, sizeof wideRate);
    for (const int wrappedFrame : {1, 2, 4, 6}) {
        Require(sceOpusCeltEncEncode(ToState(wideRateState), samples.data(), wrappedFrame, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
        Require(sceOpusCeltEncEncodeFloat(ToState(wideRateState), floatSamples.data(), wrappedFrame, packet, sizeof(packet)) == static_cast<int>(errorBase + 0x03));
    }
    Require(sceOpusCeltEncDestroy(ToState(wideRateState)) == 0);

    std::array<std::uint8_t, 65536> floatState{};
    floatState.fill(0x5A);
    Require(sceOpusCeltEncCreateEx(&context, ToState(floatState), 48000, 1, callerApplication) == 0);
    const auto floatWords = SetupWords(instanceId, 48000, 1);
    RequireState(floatState.data(), stateSize, floatWords.data(), floatWords.size(), 0x5A);
    ++instanceId;
    for (int index = 0; index < 960; ++index)
        floatSamples[static_cast<std::size_t>(index)] = static_cast<float>(0.2 * std::sin(2 * 3.14159265358979323846 * 2000 * index / 48000.0));
    const int floatResult = sceOpusCeltEncEncodeFloat(ToState(floatState), floatSamples.data(), 960, packet, static_cast<int>(sizeof(packet)));
    Require(floatResult > 2 && static_cast<int>(packet[0]) >> 3 >= 16);
    int floatOpusError = OPUS_OK;
    auto* floatReference = opus_encoder_create(48000, 1, codecApplication, &floatOpusError);
    Require(floatReference && floatOpusError == OPUS_OK);
    std::array<std::uint8_t, 1024> floatExpected{};
    const int floatExpectedBytes = opus_encode_float(floatReference, floatSamples.data(), 960, floatExpected.data(), sizeof(floatExpected));
    Require(floatExpectedBytes == floatResult && std::equal(packet, packet + floatResult, floatExpected.begin()));
    opus_encoder_destroy(floatReference);
    Require(sceOpusCeltEncDestroy(ToState(floatState)) == 0);

    std::array<std::uint8_t, 65536> liveState{};
    Require(sceOpusCeltEncCreateEx(&context, ToState(liveState), 48000, 2, callerApplication) == 0);
    ++instanceId;
    Require(sceOpusCeltEncDestroy(ToState(liveState)) == 0);
    Require(sceOpusCeltEncTerminate(&context) == 0);
    RequireThrows([&] { sceOpusCeltEncTerminate(&context); });
    RequireThrows([&] { sceOpusCeltEncCreateEx(&context, ToState(liveState), 48000, 2, callerApplication); });
    Require(sceOpusCeltEncCreate(ToState(liveState), 48000, 1, callerApplication) == 0);
    ++instanceId;
    Require(sceOpusCeltEncCreate(ToState(liveState), 48000, 2, callerApplication) == 0);
    ++instanceId;
    Require(sceOpusCeltEncDestroy(ToState(liveState)) == 0);
    RequireEarlyAnswers(callerApplication);
}

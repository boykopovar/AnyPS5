#include "prx/libSceOpusCeltDec/libSceOpusCeltDec.hpp"
#include "prx/libSceOpusDec/libSceOpusDec.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
void RequireThrows(TFunction function) {
    try { function(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

const std::uint8_t CELT_PACKET[] = {
    0xFC, 0xB5, 0x1F, 0xBC, 0xC8, 0xCA, 0x0B, 0xC2, 0x18, 0x1A, 0xFB, 0xDB, 0x3E, 0x2B, 0x6B, 0x0A, 0x67, 0x71, 0x25, 0x2F, 0x51, 0x57, 0x4F, 0x78,
    0xB3, 0x1F, 0xE9, 0xF3, 0x99, 0x51, 0xE9, 0x70, 0x72, 0x23, 0x99, 0x3E, 0x26, 0xA3, 0xF1, 0x8F, 0x02, 0xB3, 0xAA, 0xBA, 0xAB, 0x7E, 0xA5, 0x6A,
    0xBA, 0x33, 0xCA, 0xE9, 0x43, 0x4D, 0x41, 0xC9, 0x90, 0x3C, 0xD0, 0x5F, 0x55, 0xC1, 0x54, 0xA8, 0xD0, 0x6F, 0x71, 0x4F, 0xCB, 0x95, 0xDF, 0x09,
    0xC9, 0xB6, 0xAA, 0xA1, 0x8D, 0xD4, 0x2B, 0xC0, 0xF7, 0x23, 0xDD, 0xF5, 0x54, 0xEF, 0x90, 0xC4, 0xD3, 0x47, 0xC1, 0x91, 0x2D, 0xA8, 0x3D, 0x53,
    0xF4, 0x7D, 0xFF, 0xBD, 0x47, 0x0E, 0xD2, 0x95, 0x25, 0x42, 0x66, 0x02, 0xD7, 0x80, 0x00, 0x67, 0x59, 0x13, 0x60, 0x2F, 0x6C,
};

struct Api {
    std::uint32_t errorBase;
    int (APS5_VABI* initialize)(std::uint32_t*);
    int (APS5_VABI* terminate)(const std::uint32_t*);
    int (APS5_VABI* createEx)(const std::uint32_t*, void*, int, int);
    int (APS5_VABI* decode)(void*, const std::uint8_t*, int, std::int16_t*, int);
    int (APS5_VABI* decodeWithPriority)(void*, const std::uint8_t*, int, std::int16_t*, int, int);
    int (APS5_VABI* decodeFloat)(void*, const std::uint8_t*, int, float*, int);
    int (APS5_VABI* decodeFloatWithPriority)(void*, const std::uint8_t*, int, float*, int, int);
    int (APS5_VABI* destroy)(void*);
};

float Converted(std::int16_t sample) { return sample > 0 ? sample / 32767.0f : sample / 32768.0f; }

void RequireValidation(const Api& api) {
    std::array<std::uint8_t, 640> invalidState{};
    std::int16_t output = 12345;
    float floatOutput = 2.0f;
    const std::uint8_t packet = 0;
    const auto error = [&](std::uint32_t offset) { return static_cast<int>(api.errorBase + offset); };
    Require(api.decodeWithPriority(nullptr, nullptr, -1, nullptr, -1, 0) == error(0));
    Require(api.decodeWithPriority(invalidState.data(), nullptr, -1, nullptr, -1, 0) == error(6));
    Require(api.decodeWithPriority(invalidState.data(), &packet, -1, nullptr, -1, 0) == error(7));
    Require(api.decodeWithPriority(invalidState.data(), &packet, 1, nullptr, -1, 0) == error(8));
    Require(api.decodeWithPriority(invalidState.data(), &packet, 1, &output, -1, 0) == error(9));
    for (int priority : {-1, 0, 19, 101, 255})
        Require(api.decodeWithPriority(invalidState.data(), &packet, 1, &output, 2, priority) == error(10));
    for (int priority : {20, 40, 100})
        Require(api.decodeWithPriority(invalidState.data(), &packet, 1, &output, 2, priority) == error(1));
    Require(api.decodeFloatWithPriority(nullptr, nullptr, -1, nullptr, -1, 0) == error(0));
    Require(api.decodeFloatWithPriority(invalidState.data(), &packet, 1, &floatOutput, -1, 0) == error(9));
    Require(api.decodeFloatWithPriority(invalidState.data(), &packet, 1, &floatOutput, 4, 19) == error(10));
    Require(api.decodeFloatWithPriority(invalidState.data(), &packet, 1, &floatOutput, 4, 100) == error(1));
    Require(api.decodeFloat(invalidState.data(), &packet, 1, nullptr, 4) == error(8));
    Require(api.decodeFloat(invalidState.data(), &packet, 1, &floatOutput, 4) == error(1));
    Require(output == 12345 && floatOutput == 2.0f);
}

void RequireDecoding(const Api& api) {
    std::uint32_t context = 0;
    Require(api.initialize(&context) == 0);
    std::array<std::array<std::uint8_t, 640>, 4> states{};
    for (auto& state : states) Require(api.createEx(&context, state.data(), 48000, 2) == 0);
    std::array<std::int16_t, 1922> reference{}, prioritized{};
    std::array<float, 1922> floats{}, prioritizedFloats{};
    for (int frame = 0; frame < 3; ++frame) {
        reference.fill(12345);
        prioritized.fill(12345);
        floats.fill(2.0f);
        prioritizedFloats.fill(2.0f);
        Require(api.decode(states[0].data(), CELT_PACKET, sizeof(CELT_PACKET), reference.data() + 1, 3840) == 3840);
        Require(api.decodeWithPriority(states[1].data(), CELT_PACKET, sizeof(CELT_PACKET), prioritized.data() + 1, 3841, 100) == 3840);
        Require(api.decodeFloat(states[2].data(), CELT_PACKET, sizeof(CELT_PACKET), floats.data() + 1, 7683) == 7680);
        Require(api.decodeFloatWithPriority(states[3].data(), CELT_PACKET, sizeof(CELT_PACKET), prioritizedFloats.data() + 1, 7680, 20) == 7680);
        Require(prioritized == reference);
        Require(std::any_of(reference.begin() + 1, reference.end() - 1, [](auto value) { return value != 0; }));
        Require(floats.front() == 2.0f && floats.back() == 2.0f && floats == prioritizedFloats);
        for (std::size_t index = 1; index <= 1920; ++index) Require(floats[index] == Converted(reference[index]));
    }
    floats.fill(2.0f);
    RequireThrows([&] { api.decodeFloat(states[2].data(), CELT_PACKET, sizeof(CELT_PACKET), floats.data() + 1, 7679); });
    Require(std::all_of(floats.begin(), floats.end(), [](auto value) { return value == 2.0f; }));
    for (auto& state : states) Require(api.destroy(state.data()) == 0);
    Require(api.terminate(&context) == 0);
}

void RequireTimeoutVariants() {
    std::uint32_t context = 0;
    Require(sceOpusDecInitialize(&context) == 0);
    std::array<std::array<std::uint8_t, 640>, 2> states{};
    for (auto& state : states) Require(sceOpusDecCreateEx(&context, state.data(), 48000, 2) == 0);
    std::array<std::uint8_t, 640> invalidState{};
    std::int16_t output = 12345;
    const std::uint8_t packet = 0;
    std::uint32_t batch = 0xA5A5A5A5u;
    Require(sceOpusDecDecodeWithPriorityAndTimeout(invalidState.data(), &packet, 1, &output, 2, 19, 0, &batch) == static_cast<int>(0x8174000Au));
    Require(sceOpusDecDecodeWithPriorityAndTimeout(invalidState.data(), &packet, 1, &output, 2, 20, 0xFFFFFFFFu, &batch) == static_cast<int>(0x81740001u));
    Require(sceOpusDecDecodeFloatWithPriorityAndTimeout(nullptr, &packet, 1, nullptr, 2, 20, 0, &batch) == static_cast<int>(0x81740000u));
    Require(sceOpusDecDecodeFloatWithPriorityAndTimeout(invalidState.data(), &packet, -1, nullptr, 2, 20, 1000, &batch) == static_cast<int>(0x81740007u));
    std::array<std::int16_t, 1920> reference{}, pcm{};
    std::array<float, 1920> floats{};
    for (int frame = 0; frame < 2; ++frame) {
        Require(sceOpusDecDecode(states[0].data(), CELT_PACKET, sizeof(CELT_PACKET), reference.data(), 3840) == 3840);
        Require(sceOpusDecDecodeWithPriorityAndTimeout(states[1].data(), CELT_PACKET, sizeof(CELT_PACKET), pcm.data(), 3840, 40, 0xFFFFFFFFu, &batch) == 3840);
        Require(pcm == reference);
    }
    pcm.fill(12345);
    floats.fill(2.0f);
    for (std::uint32_t timeout : {0u, 1u, 1000u, 0xFFFFFFFEu}) {
        RequireThrows([&] { sceOpusDecDecodeWithPriorityAndTimeout(states[1].data(), CELT_PACKET, sizeof(CELT_PACKET), pcm.data(), 3840, 40, timeout, &batch); });
        RequireThrows([&] { sceOpusDecDecodeFloatWithPriorityAndTimeout(states[1].data(), CELT_PACKET, sizeof(CELT_PACKET), floats.data(), 7680, 40, timeout, &batch); });
    }
    Require(batch == 0xA5A5A5A5u);
    Require(std::all_of(pcm.begin(), pcm.end(), [](auto value) { return value == 12345; }));
    Require(std::all_of(floats.begin(), floats.end(), [](auto value) { return value == 2.0f; }));
    Require(sceOpusDecDecode(states[0].data(), CELT_PACKET, sizeof(CELT_PACKET), reference.data(), 3840) == 3840);
    Require(sceOpusDecDecodeFloatWithPriorityAndTimeout(states[1].data(), CELT_PACKET, sizeof(CELT_PACKET), floats.data(), 7680, 40, 0xFFFFFFFFu, nullptr) == 7680);
    for (std::size_t index = 0; index < floats.size(); ++index) Require(floats[index] == Converted(reference[index]));
    for (auto& state : states) Require(sceOpusDecDestroy(state.data()) == 0);
    Require(sceOpusDecTerminate(&context) == 0);
}

} // namespace

int main() {
    const Api opus{0x81740000u, sceOpusDecInitialize, sceOpusDecTerminate, sceOpusDecCreateEx, sceOpusDecDecode,
                   sceOpusDecDecodeWithPriority, sceOpusDecDecodeFloat, sceOpusDecDecodeFloatWithPriority, sceOpusDecDestroy};
    const Api celt{0x81400000u, sceOpusCeltDecInitialize, sceOpusCeltDecTerminate, sceOpusCeltDecCreateEx, sceOpusCeltDecDecode,
                   sceOpusCeltDecDecodeWithPriority, sceOpusCeltDecDecodeFloat, sceOpusCeltDecDecodeFloatWithPriority, sceOpusCeltDecDestroy};
    for (const auto* api : {&opus, &celt}) {
        RequireValidation(*api);
        RequireDecoding(*api);
    }
    RequireTimeoutVariants();
}

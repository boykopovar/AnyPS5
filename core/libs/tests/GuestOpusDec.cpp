#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

extern "C" {
int APS5_VABI sceOpusDecInitialize(std::uint32_t*);
int APS5_VABI sceOpusDecTerminate(std::uint32_t*);
int APS5_VABI sceOpusDecGetSize(int);
int APS5_VABI sceOpusDecCreateEx(std::uint32_t*, void*, int, int);
int APS5_VABI sceOpusDecDecode(void*, const std::uint8_t*, int, std::int16_t*, int);
int APS5_VABI sceOpusDecDestroy(void*);
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
void RequireThrows(TFunction function) {
    try { function(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

const std::uint8_t OPUS_STEREO[] = {
    0x46, 0x00, 0x7C, 0x87, 0xFC, 0xB1, 0x1D, 0xC0, 0xE3, 0x07, 0xD5, 0x9C, 0x4D, 0x91, 0x62, 0x6B, 0x73, 0x86, 0x05, 0xE8, 0xDB, 0xC6, 0x23, 0x6D,
    0x5E, 0x6F, 0x73, 0xF8, 0xF2, 0x47, 0xD9, 0x5E, 0xEA, 0xB3, 0xD2, 0x74, 0x6C, 0xE0, 0xCE, 0xE2, 0x3C, 0xFA, 0x59, 0x4A, 0xBA, 0x8F, 0x76, 0x0F,
    0x15, 0xE1, 0x3E, 0x60, 0xDF, 0x80, 0xC5, 0x44, 0xB1, 0x1B, 0xEF, 0x43, 0x03, 0x4F, 0xF2, 0xDE, 0xE3, 0xAD, 0x8B, 0x20, 0xFF, 0x68, 0x0C, 0x0A,
    0x3F, 0x00, 0x7C, 0x87, 0xFD, 0x45, 0xBD, 0x12, 0x00, 0xA5, 0xB1, 0xA0, 0x0A, 0x62, 0x24, 0x6F, 0x63, 0x70, 0xA5, 0x35, 0x13, 0x03, 0x74, 0x0D,
    0x83, 0xC9, 0x74, 0x1B, 0x79, 0xED, 0xA3, 0x09, 0x83, 0xA5, 0x02, 0xC4, 0x60, 0x49, 0xC9, 0xB6, 0x9A, 0x60, 0xAD, 0x6C, 0x77, 0x84, 0xE5, 0xC2,
    0xCE, 0x2E, 0x71, 0x07, 0x7E, 0x40, 0xCB, 0xDC, 0x13, 0xDF, 0xCF, 0x84, 0x5D, 0x55, 0x3B, 0x6B, 0x6F, 0x44, 0x00, 0x7C, 0x88, 0x01, 0xE8, 0xA1,
    0x2A, 0x12, 0x7D, 0x5E, 0xF6, 0x74, 0x78, 0xB1, 0x27, 0x2B, 0x8C, 0xF0, 0x7F, 0x95, 0x51, 0x71, 0x28, 0x18, 0xEA, 0x66, 0xEB, 0xCA, 0xAC, 0xDF,
    0x6C, 0x9C, 0xFD, 0xED, 0x88, 0x8E, 0x66, 0xD7, 0xDA, 0x36, 0xD7, 0xEB, 0x5F, 0xA6, 0x05, 0x72, 0xDA, 0x52, 0x14, 0x7F, 0xF5, 0x84, 0x54, 0xA1,
    0xA9, 0xF1, 0xAA, 0xA8, 0x73, 0x9A, 0xCC, 0x91, 0x83, 0x92, 0xD0, 0x7F, 0x3B, 0x6B, 0x65, 0x43, 0x00, 0x7C, 0x88, 0x01, 0xE8, 0xA1, 0x2A, 0x12,
    0x7D, 0x5F, 0x04, 0xF8, 0xF5, 0x1F, 0xA0, 0x85, 0x89, 0xED, 0xBA, 0x7C, 0x5F, 0x63, 0x2A, 0x9E, 0x05, 0xE8, 0x63, 0xD1, 0x73, 0x0C, 0xAF, 0xC8,
    0xC9, 0x22, 0xF7, 0x11, 0x1D, 0x8B, 0xA9, 0x9A, 0x90, 0x77, 0xF2, 0x86, 0x49, 0x15, 0x6E, 0xD6, 0x34, 0x1F, 0x50, 0x15, 0xEC, 0x85, 0xC7, 0xF5,
    0xA7, 0xFA, 0x99, 0xF9, 0x47, 0x32, 0x07, 0xAF, 0x24, 0xBF, 0x14, 0xC5,
};

} // namespace

int main() {
    std::uint32_t context = 0, otherContext = 0;
    Require(sceOpusDecInitialize(&context) == 0);
    Require(sceOpusDecInitialize(&otherContext) == 0 && context != otherContext);
    RequireThrows([&] { sceOpusDecInitialize(&context); });
    Require(sceOpusDecGetSize(1) == 640 && sceOpusDecGetSize(2) == 640);
    std::array<std::uint8_t, 640> state{}, otherState{}, monoState{};
    Require(sceOpusDecCreateEx(&context, state.data(), 48000, 2) == 0);
    Require(sceOpusDecCreateEx(&otherContext, otherState.data(), 48000, 2) == 0);
    Require(sceOpusDecCreateEx(&context, monoState.data(), 48000, 1) == 0);
    RequireThrows([&] { sceOpusDecTerminate(&context); });
    RequireThrows([&] { sceOpusDecCreateEx(&context, state.data(), 48000, 2); });
    RequireThrows([&] { sceOpusDecGetSize(3); });
    std::array<std::uint8_t, 640> unsupported{};
    for (int rate : {8000, 12000, 16000, 24000})
        RequireThrows([&] { sceOpusDecCreateEx(&context, unsupported.data(), rate, 2); });
    std::array<std::int16_t, 1922> pcm{}, reference{};
    pcm.fill(12345);
    RequireThrows([&] { sceOpusDecDecode(state.data(), OPUS_STEREO + 2, 70, pcm.data() + 1, 3839); });
    Require(std::all_of(pcm.begin(), pcm.end(), [](auto value) { return value == 12345; }));
    const std::uint8_t malformed[] = {0x7f};
    RequireThrows([&] { sceOpusDecDecode(state.data(), malformed, 1, pcm.data() + 1, 3840); });
    RequireThrows([&] { sceOpusDecDecode(state.data(), nullptr, 0, pcm.data() + 1, 3840); });
    Require(std::all_of(pcm.begin(), pcm.end(), [](auto value) { return value == 12345; }));
    std::size_t offset = 0;
    for (int frame = 0; frame < 3; ++frame) {
        const int bytes = OPUS_STEREO[offset] | (OPUS_STEREO[offset + 1] << 8);
        const auto* packet = OPUS_STEREO + offset + 2;
        Require(sceOpusDecDecode(state.data(), packet, bytes, pcm.data() + 1, 3841) == 3840);
        Require(pcm.front() == 12345 && pcm.back() == 12345);
        Require(sceOpusDecDecode(otherState.data(), packet, bytes, reference.data() + 1, 3840) == 3840);
        Require(std::equal(pcm.begin() + 1, pcm.end() - 1, reference.begin() + 1));
        if (frame == 1) {
            Require(sceOpusDecCreateEx(&context, unsupported.data(), 48000, 2) == 0);
            std::array<std::int16_t, 1920> fresh{};
            Require(sceOpusDecDecode(unsupported.data(), packet, bytes, fresh.data(), 3840) == 3840);
            Require(!std::equal(fresh.begin(), fresh.end(), pcm.begin() + 1));
            Require(sceOpusDecDestroy(unsupported.data()) == 0);
        }
        if (frame > 0) {
            int peak = 0;
            for (auto it = pcm.begin() + 1; it != pcm.end() - 1; ++it) peak = std::max(peak, std::abs(static_cast<int>(*it)));
            Require(peak > 900 && peak < 1200);
        }
        std::array<std::int16_t, 962> mono{};
        mono.front() = mono.back() = 12345;
        Require(sceOpusDecDecode(monoState.data(), packet, bytes, mono.data() + 1, 1921) == 1920);
        Require(mono.front() == 12345 && mono.back() == 12345);
        Require(std::any_of(mono.begin() + 1, mono.end() - 1, [](auto value) { return value != 0; }));
        offset += bytes + 2;
    }
    Require(sceOpusDecDestroy(state.data()) == 0);
    RequireThrows([&] { sceOpusDecDestroy(state.data()); });
    RequireThrows([&] { sceOpusDecDecode(state.data(), OPUS_STEREO + 2, 70, pcm.data(), 3840); });
    Require(sceOpusDecCreateEx(&context, state.data(), 48000, 2) == 0);
    Require(sceOpusDecDecode(state.data(), OPUS_STEREO + 2, 70, pcm.data(), 3840) == 3840);
    std::array<std::int16_t, 3842> rejected;
    rejected.fill(12345);
    const std::uint8_t invalidFraming[] = {0x7d, 1};
    RequireThrows([&] { sceOpusDecDecode(state.data(), invalidFraming, 2, rejected.data() + 1, 7680); });
    Require(std::all_of(rejected.begin(), rejected.end(), [](auto value) { return value == 12345; }));
    Require(sceOpusDecDestroy(state.data()) == 0);
    Require(sceOpusDecDestroy(monoState.data()) == 0);
    Require(sceOpusDecTerminate(&context) == 0);
    RequireThrows([&] { sceOpusDecTerminate(&context); });
    RequireThrows([&] { sceOpusDecCreateEx(&context, state.data(), 48000, 2); });
    Require(sceOpusDecDecode(otherState.data(), OPUS_STEREO + offset + 2, OPUS_STEREO[offset], reference.data(), 3840) == 3840);
    Require(sceOpusDecDestroy(otherState.data()) == 0);
    Require(sceOpusDecTerminate(&otherContext) == 0);
}

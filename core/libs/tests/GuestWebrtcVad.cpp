#include "prx/libwebrtc/libwebrtc.hpp"
#include <fvad.h>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <limits>
#include <source_location>

static void Require(bool condition, const std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "VAD check failed at line %u\n", location.line());
        std::abort();
    }
}

int main() {
    using Samples = std::array<std::int16_t, 1440>;
    Samples silence{};
    Samples signal{};
    std::uint32_t state = 1;
    for (std::size_t i = 0; i < signal.size(); ++i) {
        state = state * 1664525u + 1013904223u;
        const auto value = static_cast<std::uint16_t>(state >> 16);
        signal[i] = static_cast<std::int16_t>(value <= 32767 ? value : static_cast<int>(value) - 65536);
    }
    WebRtcVad_Free_nid_postfix(nullptr);
    Require(WebRtcVad_Init_nid_postfix(nullptr) == -1);
    Require(WebRtcVad_set_mode_nid_postfix(nullptr, 0) == -1);
    Require(WebRtcVad_Process_nid_postfix(nullptr, 8000, silence.data(), 80) == -1);
    auto* handle = WebRtcVad_Create_nid_postfix();
    Require(handle != nullptr);
    Require(WebRtcVad_set_mode_nid_postfix(handle, 0) == -1);
    Require(WebRtcVad_Process_nid_postfix(handle, 8000, silence.data(), 80) == -1);
    Require(WebRtcVad_Init_nid_postfix(handle) == 0);
    for (int mode : {-1, 4, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        Require(WebRtcVad_set_mode_nid_postfix(handle, mode) == -1);
    for (int rate : {0, -8000, 11025, 22050, 44100, 96000, std::numeric_limits<int>::max()})
        Require(WebRtcVad_Process_nid_postfix(handle, rate, silence.data(), 80) == -1);
    Require(WebRtcVad_Process_nid_postfix(handle, 8000, nullptr, 80) == -1);
    Fvad* reference = fvad_new();
    Require(reference != nullptr);
    for (int mode = 0; mode < 4; ++mode) {
        for (int rate : {8000, 16000, 32000, 48000}) {
            const auto block = static_cast<std::size_t>(rate / 100);
            for (std::size_t length : {std::size_t{0}, block - 1, block + 1, 4 * block, std::numeric_limits<std::size_t>::max()})
                Require(WebRtcVad_Process_nid_postfix(handle, rate, silence.data(), length) == -1);
            for (std::size_t duration = 1; duration <= 3; ++duration) {
                fvad_reset(reference);
                Require(fvad_set_mode(reference, mode) == 0);
                Require(fvad_set_sample_rate(reference, rate) == 0);
                Require(WebRtcVad_Init_nid_postfix(handle) == 0);
                Require(WebRtcVad_set_mode_nid_postfix(handle, mode) == 0);
                Require(WebRtcVad_Process_nid_postfix(handle, rate, silence.data(), block * duration) == 0);
                Require(fvad_process(reference, silence.data(), block * duration) == 0);
                for (int frame = 0; frame < 10; ++frame)
                    Require(WebRtcVad_Process_nid_postfix(handle, rate, signal.data(), block * duration) == fvad_process(reference, signal.data(), block * duration));
                Require(WebRtcVad_Init_nid_postfix(handle) == 0);
                Require(WebRtcVad_Process_nid_postfix(handle, rate, silence.data(), block * duration) == 0);
            }
        }
    }
    fvad_free(reference);
    auto* second = WebRtcVad_Create_nid_postfix();
    Require(second != nullptr && second != handle);
    Require(WebRtcVad_Init_nid_postfix(second) == 0);
    Require(WebRtcVad_Process_nid_postfix(handle, 8000, signal.data(), 80) == 1);
    Require(WebRtcVad_Process_nid_postfix(second, 8000, silence.data(), 80) == 0);
    WebRtcVad_Free_nid_postfix(second);
    WebRtcVad_Free_nid_postfix(handle);
}

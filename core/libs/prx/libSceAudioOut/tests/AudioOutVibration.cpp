#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <vector>

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Case {
    std::string_view name;
    std::uint32_t format = 1;
    bool batch = false;
    std::array<int, 2> volume{32768, 32768};
    std::array<std::uint8_t, 2> expected{128, 255};
    bool silence = false;
    bool drain = false;
    bool ex = false;
};

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc == 2 ? argv[1] : "--s16";
        const Case cases[] = {
            {"--s16"}, {"--f32", 4}, {"--batch_s16", 1, true}, {"--batch_f32", 4, true},
            {"--mono_s16", 0, false, {32768, 32768}, {128, 128}},
            {"--mono_f32", 3, false, {32768, 32768}, {128, 128}},
            {"--gain_s16", 1, false, {16384, 8192}, {64, 64}},
            {"--gain_f32", 4, true, {16384, 8192}, {64, 64}},
            {"--muted", 4, false, {0, 0}, {0, 0}},
            {"--silence", 1, true, {32768, 32768}, {0, 0}, true},
            {"--drain", 4, false, {32768, 32768}, {0, 0}, false, true},
            {"--ex", 1, true, {32768, 32768}, {0, 0}, false, false, true},
            {"--pad_control"}
        };
        const Case* selected = nullptr;
        for (const auto& entry : cases) if (entry.name == mode) selected = &entry;
        Require(argc <= 2 && selected != nullptr, "invalid test arguments");
        const auto& test = *selected;
        Pad::Initialize();
        Pad::SetVibrationMode(1);
        std::uint32_t seen = 0;
        PadOutputState output{};
        static_cast<void>(PadFetchOutput_nid_postfix(&seen, &output));
        struct Port {
            int handle = -1;
            ~Port() { if (handle > 0) sceAudioOutClose(handle); }
        } port;
        if (mode == "--pad_control") {
            PadSubmitAudioHaptics_nid_postfix(128, 255);
        } else {
            const std::uint32_t frames = test.ex ? 256u : 48000u;
            const bool floating = test.format == 3 || test.format == 4;
            const std::uint32_t channels = test.format == 0 || test.format == 3 ? 1u : 2u;
            std::vector<std::int16_t> integers(frames * 2);
            std::vector<float> floats(frames * 2);
            for (std::uint32_t frame = 0; frame < frames && !test.silence; ++frame) {
                integers[frame * channels] = 8192;
                floats[frame * channels] = 0.25f;
                if (channels == 2) {
                    integers[frame * channels + 1] = -16384;
                    floats[frame * channels + 1] = -0.5f;
                }
            }
            port.handle = test.ex ? sceAudioOutExOpen(0x10000000, 0) : sceAudioOutOpen(0x10000000, 10, 0, frames, 48000, test.format);
            Require(port.handle > 0, "legacy vibration port did not open");
            auto volume = test.volume;
            Require(sceAudioOutSetVolume(port.handle, 3, volume.data()) == 0, "legacy vibration volume failed");
            const void* samples = test.drain ? nullptr : floating ? static_cast<const void*>(floats.data()) : static_cast<const void*>(integers.data());
            AudioOutOutputParam param{};
            param.handle = port.handle;
            param.ptr = samples;
            const int result = test.batch ? sceAudioOutOutputs(&param, 1) : sceAudioOutOutput(port.handle, samples);
            Require(result == static_cast<int>(frames), "legacy vibration output returned the wrong frame count");
        }
        const bool changed = PadFetchOutput_nid_postfix(&seen, &output);
        Require(changed == (test.expected[0] != 0 || test.expected[1] != 0), "legacy vibration output published the wrong controller state");
        Require(output.audioVibrationLarge == test.expected[0] && output.audioVibrationSmall == test.expected[1], "legacy vibration PCM did not reach the existing motor bridge");
        Require(output.vibrationLarge == 0 && output.vibrationSmall == 0, "audio vibration replaced regular motor state");
        std::puts("legacy vibration bridge test passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

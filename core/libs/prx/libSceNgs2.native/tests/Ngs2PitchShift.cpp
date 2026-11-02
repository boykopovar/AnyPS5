#include "Ngs2Test.hpp"

#include <array>
#include <cmath>
#include <stdexcept>

static constexpr std::uint32_t RenderGrain = 256;

struct Signal {
    std::uint32_t frame = 0;
};

static int APS5_VABI Input(Ngs2UserFxProcessContext* context) {
    auto& signal = *reinterpret_cast<Signal*>(context->user_data0);
    for (std::uint32_t i = 0; i < context->num_grain_samples; ++i) {
        for (std::uint32_t c = 0; c < context->num_channels; ++c) {
            context->channel_data[c][i] = static_cast<float>(static_cast<int>((signal.frame + i + c * 97) % 101) - 50) * .005f;
        }
    }
    signal.frame += context->num_grain_samples;
    return SCE_NGS2_OK;
}

static void TestPitchShift(std::uint32_t moduleIndex, bool checkBypassRejection = false) {
    const auto firstBuffer = usedBuffers;
    const auto system = CreateSystem();
    Require(sceNgs2SystemSetGrainSamples(system, RenderGrain) == SCE_NGS2_OK);
    const auto master = Mastering(system, 2);
    Ngs2CustomSubmixerRackOption option{};
    auto& custom = option.custom_rack_option;
    auto& common = custom.rack_option;
    common.size = sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 2;
    common.max_matrices = 1;
    common.max_ports = 1;
    custom.num_buffers = 1;
    custom.num_modules = moduleIndex + 1;
    custom.state_size = 32;
    option.max_channels = 2;
    option.max_inputs = 1;
    for (std::uint32_t i = 0; i <= moduleIndex; ++i) custom.module[i].module_id = SCE_NGS2_CUSTOM_MODULE_ID_PITCH_SHIFT;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &query) == SCE_NGS2_OK);
    const auto storage = Buffer(query);
    std::uintptr_t rack = 0;
    Require(TestRackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &storage, &rack) == SCE_NGS2_OK);
    const auto voice = Voice(rack);
    const auto setup = [&] {
        Control(voice, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
        Patch(voice, master);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    };
    setup();
    for (const auto index : {moduleIndex + 1, 31u, 255u}) {
        Ngs2CustomVoicePitchShiftParam invalid{};
        invalid.header = {sizeof(invalid), 0, SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT | index};
        invalid.cents = 700;
        Require(sceNgs2VoiceControl(voice, &invalid.header) == SCE_NGS2_ERROR_INVALID_PARAM_ID);
    }
    std::array<Ngs2CustomVoicePitchShiftParam, 2> invalidChain{};
    invalidChain[0].header = {sizeof(invalidChain[0]), sizeof(invalidChain[0]), SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT | 31};
    invalidChain[1].header.id = SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT;
    Require(sceNgs2VoiceControl(voice, &invalidChain[0].header) == SCE_NGS2_ERROR_INVALID_PARAM_ID);

    const auto source = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Signal signal;
    Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
            Ngs2SubmixerVoiceUserFxParam{{}, Input, reinterpret_cast<std::uintptr_t>(&signal), 0, 0});
    Patch(source, voice);
    Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    std::array<float, RenderGrain * 2> samples{};
    const Ngs2RenderBufferInfo output{samples.data(), sizeof(samples), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    const auto param = SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT | moduleIndex;
    const std::array<std::uint32_t, 8> checkpoints{20, 30, 40, 42, 44, 52, 70, 80};
    const float expected[][4]{
        {-.0280017573f, -.0863388255f, .160010189f, .0728731006f},
        {.105002567f, -.25f, -.243838981f, 0},
        {.0749999955f, -.0149999997f, -.179999992f, .240712553f},
        {.109999999f, .0799999982f, -.144999996f, -.174999997f},
        {0, 0, -.0859375f, -.109375f},
        {-.219999999f, -.24499999f, .0065624998f, .00109374989f},
        {.0949999988f, .0749999955f, -.159999996f, -.179999992f},
        {.140000001f, .119999997f, -.114999995f, -.13499999f},
    };
    for (std::uint32_t grain = 0; grain < 90; ++grain) {
        if (grain == 12) Control(voice, param | 0x20, Ngs2CustomVoicePitchShiftParam{{}, 700, 0, 0, 0});
        if (grain == 22) Control(voice, param, Ngs2CustomVoicePitchShiftParam{{}, -700, 0, 2, 0});
        if (grain == 32) Control(voice, param, Ngs2CustomVoicePitchShiftParam{{}, 0, 0, 0, 0});
        if (grain == 42) Event(voice, SCE_NGS2_VOICE_EVENT_PAUSE);
        if (grain == 44) Event(voice, SCE_NGS2_VOICE_EVENT_RESUME);
        if (grain == 52) Event(voice, SCE_NGS2_VOICE_EVENT_STOP);
        if (grain == 54) Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        if (grain == 62) {
            Event(voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
            Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        }
        if (grain == 72) setup();
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        for (std::uint32_t i = 0; i < samples.size(); ++i) {
            Require(std::isfinite(samples[i]));
            if (grain < 12 || grain >= 72 || (grain >= 22 && grain < 32 && (i & 1) != 0)) {
                const auto frame = grain * RenderGrain + i / 2;
                const auto channel = i & 1;
                const float dry = static_cast<float>(static_cast<int>((frame + channel * 97) % 101) - 50) * .005f;
                Require(samples[i] == dry);
            }
            if ((grain >= 12 && grain <= 18) || grain == 43 || grain == 53 ||
                (grain >= 54 && grain <= 60) || (grain >= 62 && grain <= 68)) Require(samples[i] == 0.0f);
        }
        for (std::size_t i = 0; i < checkpoints.size(); ++i) {
            if (grain != checkpoints[i]) continue;
            for (std::size_t j = 0; j < 4; ++j) Require(std::abs(samples[j < 2 ? j : j + 98] - expected[i][j]) < 1e-6f);
        }
    }
    Control(voice, param, Ngs2CustomVoicePitchShiftParam{{}, 700, 0, 0, 0});
    for (std::uint32_t i = 0; i < 10; ++i) Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    std::uintptr_t otherVoice = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, 1, &otherVoice) == SCE_NGS2_OK);
    Control(otherVoice, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
    Control(otherVoice, param, Ngs2CustomVoicePitchShiftParam{{}, -700, 0, 1, 0});
    Patch(voice, 0);
    Patch(source, otherVoice);
    Patch(otherVoice, master);
    Event(otherVoice, SCE_NGS2_VOICE_EVENT_PLAY);
    for (std::uint32_t i = 0; i < 7; ++i) {
        const auto firstFrame = signal.frame;
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        for (std::uint32_t frame = 0; frame < RenderGrain; ++frame) {
            const float dry = static_cast<float>(static_cast<int>((firstFrame + frame) % 101) - 50) * .005f;
            Require(samples[frame * 2] == dry && samples[frame * 2 + 1] == 0.0f);
        }
    }
    if (checkBypassRejection) {
        Patch(otherVoice, 0);
        Patch(source, voice);
        setup();
        Control(voice, param, Ngs2CustomVoicePitchShiftParam{{}, 700, 0, 2, 0});
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        Control(voice, param, Ngs2CustomVoicePitchShiftParam{{}, 700, 0, 0, 0});
        bool rejected = false;
        try {
            sceNgs2SystemRender(system, &output, 1);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Require(rejected);
        Event(voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        for (const float sample : samples) Require(sample == 0.0f);
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

int main() {
    TestPitchShift(0, true);
    TestPitchShift(1);
    return 0;
}

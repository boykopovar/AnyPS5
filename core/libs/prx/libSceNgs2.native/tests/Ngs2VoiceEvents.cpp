#include "Ngs2Test.hpp"

#include <cstdint>
#include <vector>

static const Ngs2SamplerVoiceSetupParam SamplerSetup{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}};

static Ngs2VoiceEventParam PlayParam() {
    return {{sizeof(Ngs2VoiceEventParam), 0, SCE_NGS2_VOICE_PARAM_EVENT}, SCE_NGS2_VOICE_EVENT_PLAY};
}

static uintptr_t Sampler(uintptr_t system) {
    const auto master = Mastering(system, 1);
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Patch(voice, master);
    return voice;
}

static void TestPlayWithoutSetup() {
    const auto system = CreateSystem();
    const auto voice = Sampler(system);
    const auto play = PlayParam();
    const Ngs2VoiceCommand playCommand{2, 0, 4, 0, {.u = SCE_NGS2_VOICE_EVENT_PLAY}};
    Event(voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, nullptr, 0, 0, nullptr});
    Require(sceNgs2VoiceControl(voice, &play.header) == SCE_NGS2_ERROR_INVALID_VOICE_ACCESS);
    Require(sceNgs2VoiceRunCommands(voice, &playCommand, 1) == SCE_NGS2_ERROR_INVALID_VOICE_ACCESS);

    std::vector<float> out(Grain, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    for (int grain = 0; grain < 3; grain++) {
        Require(Flags(voice) == 0);
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        for (float sample : out) Require(sample == 0.0f);
    }
    Require(Flags(voice) == 0);

    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, SamplerSetup);
    Require(sceNgs2VoiceRunCommands(voice, &playCommand, 1) == SCE_NGS2_OK);
    Require(Flags(voice) == SCE_NGS2_VOICE_STATE_FLAG_INUSE);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestEmptyFormatKeepsTheSetup() {
    const auto system = CreateSystem();
    const auto voice = Sampler(system);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, SamplerSetup);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    Require(Flags(voice) == SCE_NGS2_VOICE_STATE_FLAG_INUSE);

    const auto bare = Sampler(system);
    Control(bare, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{});
    const auto play = PlayParam();
    Require(sceNgs2VoiceControl(bare, &play.header) == SCE_NGS2_ERROR_INVALID_VOICE_ACCESS);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestPlayAfterSetupInOneList() {
    const auto system = CreateSystem();
    const auto voice = Sampler(system);
    struct List {
        Ngs2SamplerVoiceSetupParam setup;
        Ngs2VoiceEventParam play;
    };
    List list{SamplerSetup, PlayParam()};
    list.setup.header = {sizeof(list.setup), static_cast<std::int16_t>(sizeof(list.setup)), SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP};
    Require(sceNgs2VoiceControl(voice, &list.setup.header) == SCE_NGS2_OK);
    Require(Flags(voice) == SCE_NGS2_VOICE_STATE_FLAG_INUSE);

    const auto other = Sampler(system);
    struct Reversed {
        Ngs2VoiceEventParam play;
        Ngs2SamplerVoiceSetupParam setup;
    };
    Reversed reversed{PlayParam(), SamplerSetup};
    reversed.play.header = {sizeof(reversed.play), static_cast<std::int16_t>(sizeof(reversed.play)), SCE_NGS2_VOICE_PARAM_EVENT};
    reversed.setup.header = {sizeof(reversed.setup), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP};
    Require(sceNgs2VoiceControl(other, &reversed.play.header) == SCE_NGS2_ERROR_INVALID_VOICE_ACCESS);
    Require(Flags(other) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestNeverSetUpVoicesOfOtherRacks() {
    const auto system = CreateSystem();
    const auto play = PlayParam();
    for (const auto rackId : {SCE_NGS2_RACK_ID_SUBMIXER, SCE_NGS2_RACK_ID_REVERB, SCE_NGS2_RACK_ID_MASTERING}) {
        const auto voice = Voice(CreateRack(system, rackId));
        Require(sceNgs2VoiceControl(voice, &play.header) == SCE_NGS2_ERROR_INVALID_VOICE_ACCESS);
        Require(Flags(voice) == 0);
    }
    const auto master = Mastering(system, 1);
    Require(Flags(master) == SCE_NGS2_VOICE_STATE_FLAG_INUSE);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

int main() {
    TestPlayWithoutSetup();
    TestEmptyFormatKeepsTheSetup();
    TestPlayAfterSetupInOneList();
    TestNeverSetUpVoicesOfOtherRacks();
    return 0;
}

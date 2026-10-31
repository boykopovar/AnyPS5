#include "Ngs2Test.hpp"
#include "../src/Ngs2Internal.hpp"

int main() {
    const auto system = CreateSystem();
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto handle = Voice(rack);
    Control(handle, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    Ngs2SamplerVoiceFilterParam parameter{{sizeof(parameter), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_FILTER}, 0, 1, 1, 2, 1000.0f, 0.75f, 0.5f, {}};
    Require(sceNgs2VoiceControl(handle, &parameter.header) == SCE_NGS2_OK);
    const auto& voice = *reinterpret_cast<const Ngs2Voice*>(handle);
    Require(voice.rack->system == nullptr && voice.filters.size() == 1);
    const auto& filter = voice.filters.front();
    Require(filter.enabled && filter.pending && filter.bypassMask == 2);
    Require(filter.parameter.frequency == 1000.0f && filter.parameter.q == 0.75f && filter.parameter.level == 0.5f);
    parameter.frequency = 2000.0f;
    parameter.q = 1.0f;
    Require(filter.parameter.frequency == 1000.0f && filter.parameter.q == 0.75f);
    Require(sceNgs2VoiceControl(handle, &parameter.header) == SCE_NGS2_OK);
    Require(filter.parameter.frequency == 2000.0f && filter.parameter.q == 1.0f && filter.pending);
    parameter.type = 0;
    Require(sceNgs2VoiceControl(handle, &parameter.header) == SCE_NGS2_OK);
    Require(!filter.enabled && !filter.pending && filter.history.empty());
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    std::puts("NGS2 detached filter parameter ownership passed");
    return 0;
}

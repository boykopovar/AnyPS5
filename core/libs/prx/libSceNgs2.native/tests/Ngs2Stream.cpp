#include "Ngs2Test.hpp"

#include <cstdint>
#include <stdexcept>
#include <vector>

static std::vector<std::int16_t> RenderI16(uintptr_t system) {
    std::vector<std::int16_t> out(Grain, -1);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(std::int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    return out;
}

static uintptr_t RackVoice(uintptr_t rack, std::uint32_t index) {
    uintptr_t voice = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, index, &voice) == SCE_NGS2_OK && voice != 0);
    return voice;
}

static uintptr_t StreamVoice(uintptr_t rack, std::uint32_t index, uintptr_t master) {
    const auto voice = RackVoice(rack, index);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    Patch(voice, master);
    return voice;
}

static void AddBlock(uintptr_t voice, const void* data, std::uint64_t bytes, std::uint32_t samples, std::uint32_t flags) {
    const Ngs2WaveformBlock block{0, bytes, 0, 0, samples, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, data, flags, 1, &block});
}

template <typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static void TestStreamEndsAtDeclaredSamples() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = StreamVoice(rack, 0, master);
    const std::vector<std::int16_t> first(6, 16384);
    const std::vector<std::int16_t> second(8, 8192);
    const std::vector<std::int16_t> last(6, 4096);

    AddBlock(voice, first.data(), first.size() * 2, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 6 ? 16384 : 0));
    Require(Flags(voice) == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING));
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == 6 && state.decoded_data_size == 12 && state.waveform_data == first.data() + 6);

    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == 0);
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == 6 && state.decoded_data_size == 12);

    AddBlock(voice, second.data(), second.size() * 2, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK && state.waveform_data == second.data());
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == 8192);
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == 14 && state.decoded_data_size == 28 && state.waveform_data == second.data() + 8);

    AddBlock(voice, last.data(), last.size() * 2, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 6 ? 4096 : 0));
    Require(Flags(voice) == 0);
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == 20 && state.decoded_data_size == 40 && state.waveform_data == last.data() + 6);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestStreamClosedBeforeDeclaredSamples() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = StreamVoice(rack, 0, master);
    const std::vector<std::int16_t> first(6, 16384);
    const std::vector<std::int16_t> closing(4, 8192);

    AddBlock(voice, first.data(), first.size() * 2, 1000, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    AddBlock(voice, closing.data(), closing.size() * 2, 1000, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 6 ? 16384 : 8192));
    Require(Flags(voice) == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING));
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 2 ? 8192 : 0));
    Require(Flags(voice) == 0);
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == 10 && state.decoded_data_size == 20);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestStreamRejections() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const std::vector<std::int16_t> pcm(6, 1000);

    const auto empty = StreamVoice(rack, 0, master);
    Require(Throws([&] { AddBlock(empty, pcm.data(), pcm.size() * 2, 6, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND); }));

    const auto complete = StreamVoice(rack, 1, master);
    AddBlock(complete, pcm.data(), pcm.size() * 2, 6, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Require(Throws([&] { AddBlock(complete, pcm.data(), pcm.size() * 2, 6, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND); }));

    const auto repeating = StreamVoice(rack, 2, master);
    const Ngs2WaveformBlock repeated{0, pcm.size() * 2, 1, 0, 20, 0, 0};
    Require(Throws([&] { Control(repeating, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 1, &repeated}); }));

    const auto skipping = StreamVoice(rack, 3, master);
    const Ngs2WaveformBlock skipped{0, pcm.size() * 2, 0, 1, 20, 0, 0};
    Require(Throws([&] { Control(skipping, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 1, &skipped}); }));

    const auto partial = StreamVoice(rack, 4, master);
    AddBlock(partial, pcm.data(), pcm.size() * 2, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Require(Throws([&] { AddBlock(partial, pcm.data(), 3, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND); }));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

int main() {
    TestStreamEndsAtDeclaredSamples();
    TestStreamClosedBeforeDeclaredSamples();
    TestStreamRejections();
    return 0;
}

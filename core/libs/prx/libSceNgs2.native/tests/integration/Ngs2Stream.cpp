#include "Ngs2Test.hpp"

#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace {

using namespace Ngs2Testing;
using Testing::Case;
using Testing::RequireEqual;

std::vector<std::int16_t> RenderI16(uintptr_t system) {
    std::vector<std::int16_t> out(Grain, -1);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(std::int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    RequireEqual(sceNgs2SystemRender(system, &info, 1), SCE_NGS2_OK, "render");
    return out;
}

uintptr_t StreamVoice(uintptr_t rack, std::uint32_t index, uintptr_t master) {
    const auto voice = RackVoice(rack, index);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    Patch(voice, master);
    return voice;
}

void AddBlock(uintptr_t voice, const void* data, std::uint64_t bytes, std::uint32_t samples, std::uint32_t flags) {
    const Ngs2WaveformBlock block{0, bytes, 0, 0, samples, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, data, flags, 1, &block});
}

void RequireSamples(const std::vector<std::int16_t>& out, std::uint32_t split, std::int16_t before, std::int16_t after,
                    const char* message) {
    for (std::uint32_t i = 0; i < Grain; i++) {
        RequireEqual(out[i], i < split ? before : after, std::string(message) + " sample " + std::to_string(i));
    }
}

Ngs2SamplerVoiceState SamplerState(uintptr_t voice) {
    Ngs2SamplerVoiceState state{};
    RequireEqual(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)), SCE_NGS2_OK, "get sampler state");
    return state;
}

class StreamFixture {
public:
    StreamFixture() : system(ngs2.CreateSystem()), master(ngs2.Mastering(system, 1)),
                      rack(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER)) {}

    Ngs2Fixture ngs2;
    const uintptr_t system;
    const uintptr_t master;
    const uintptr_t rack;
};

constexpr std::uint32_t playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING;

const Case starvedStream{"Stream_ContinueBlockExhausted_KeepsPlayingSilently", [] {
    StreamFixture fixture;
    const auto voice = StreamVoice(fixture.rack, 0, fixture.master);
    const std::vector<std::int16_t> first(6, 16384);
    AddBlock(voice, first.data(), first.size() * 2, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireSamples(RenderI16(fixture.system), 6, 16384, 0, "first grain");
    RequireEqual(Flags(voice), playing, "still playing");
    auto state = SamplerState(voice);
    RequireEqual(state.num_decoded_samples, 6u, "decoded samples");
    RequireEqual(state.decoded_data_size, 12u, "decoded bytes");
    RequireEqual(static_cast<const void*>(state.waveform_data), static_cast<const void*>(first.data() + 6), "waveform position");

    RequireSamples(RenderI16(fixture.system), 0, 0, 0, "starved grain");
    state = SamplerState(voice);
    RequireEqual(state.num_decoded_samples, 6u, "decoded samples while starved");
    RequireEqual(state.decoded_data_size, 12u, "decoded bytes while starved");
}};

const Case appendedStream{"Stream_AppendedBlocks_EndAtDeclaredSampleCount", [] {
    StreamFixture fixture;
    const auto voice = StreamVoice(fixture.rack, 0, fixture.master);
    const std::vector<std::int16_t> first(6, 16384);
    const std::vector<std::int16_t> second(8, 8192);
    const std::vector<std::int16_t> last(6, 4096);
    AddBlock(voice, first.data(), first.size() * 2, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    RenderI16(fixture.system);
    RenderI16(fixture.system);

    AddBlock(voice, second.data(), second.size() * 2, 20,
             SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    RequireEqual(static_cast<const void*>(SamplerState(voice).waveform_data), static_cast<const void*>(second.data()),
                 "waveform position after append");
    RequireSamples(RenderI16(fixture.system), 0, 0, 8192, "second block");
    auto state = SamplerState(voice);
    RequireEqual(state.num_decoded_samples, 14u, "decoded samples after second block");
    RequireEqual(state.decoded_data_size, 28u, "decoded bytes after second block");
    RequireEqual(static_cast<const void*>(state.waveform_data), static_cast<const void*>(second.data() + 8),
                 "waveform position after second block");

    AddBlock(voice, last.data(), last.size() * 2, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    RequireSamples(RenderI16(fixture.system), 6, 4096, 0, "last block");
    RequireEqual(Flags(voice), 0u, "stopped at the declared sample count");
    state = SamplerState(voice);
    RequireEqual(state.num_decoded_samples, 20u, "decoded samples at end");
    RequireEqual(state.decoded_data_size, 40u, "decoded bytes at end");
    RequireEqual(static_cast<const void*>(state.waveform_data), static_cast<const void*>(last.data() + 6),
                 "waveform position at end");
}};

const Case closedStream{"Stream_ClosedBeforeDeclaredSamples_StopsAtDataEnd", [] {
    StreamFixture fixture;
    const auto voice = StreamVoice(fixture.rack, 0, fixture.master);
    const std::vector<std::int16_t> first(6, 16384);
    const std::vector<std::int16_t> closing(4, 8192);
    AddBlock(voice, first.data(), first.size() * 2, 1000, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    AddBlock(voice, closing.data(), closing.size() * 2, 1000, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    RequireSamples(RenderI16(fixture.system), 6, 16384, 8192, "first grain");
    RequireEqual(Flags(voice), playing, "still playing");
    RequireSamples(RenderI16(fixture.system), 2, 8192, 0, "second grain");
    RequireEqual(Flags(voice), 0u, "stopped");
    const auto state = SamplerState(voice);
    RequireEqual(state.num_decoded_samples, 10u, "decoded samples");
    RequireEqual(state.decoded_data_size, 20u, "decoded bytes");
}};

template<typename TOperation>
void RequireRejected(const TOperation& operation, const char* message) {
    try {
        operation();
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::exception&) {
        return;
    }
    Testing::Fail(std::string(message) + ": did not throw");
}

const Case appendWithoutStream{"Stream_AppendWithoutOpenStream_IsRejected", [] {
    StreamFixture fixture;
    const std::vector<std::int16_t> pcm(6, 1000);
    const auto voice = StreamVoice(fixture.rack, 0, fixture.master);
    RequireRejected([&] { AddBlock(voice, pcm.data(), pcm.size() * 2, 6, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND); },
                    "append to an empty voice");
}};

const Case appendBeyondDeclared{"Stream_AppendAfterDeclaredSamplesQueued_IsRejected", [] {
    StreamFixture fixture;
    const std::vector<std::int16_t> pcm(6, 1000);
    const auto voice = StreamVoice(fixture.rack, 0, fixture.master);
    AddBlock(voice, pcm.data(), pcm.size() * 2, 6, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    RequireRejected([&] {
        AddBlock(voice, pcm.data(), pcm.size() * 2, 6, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    }, "append to a complete stream");
}};

const Case repeatingStream{"Stream_ContinueBlockWithRepeatsOrSkip_IsRejected", [] {
    StreamFixture fixture;
    const std::vector<std::int16_t> pcm(6, 1000);
    const auto repeating = StreamVoice(fixture.rack, 0, fixture.master);
    const Ngs2WaveformBlock repeated{0, pcm.size() * 2, 1, 0, 20, 0, 0};
    RequireRejected([&] {
        Control(repeating, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
                Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 1, &repeated});
    }, "continue block with repeats");
    const auto skipping = StreamVoice(fixture.rack, 1, fixture.master);
    const Ngs2WaveformBlock skipped{0, pcm.size() * 2, 0, 1, 20, 0, 0};
    RequireRejected([&] {
        Control(skipping, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
                Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 1, &skipped});
    }, "continue block with skipped samples");
}};

const Case partialSample{"Stream_AppendPartialSample_IsRejected", [] {
    StreamFixture fixture;
    const std::vector<std::int16_t> pcm(6, 1000);
    const auto voice = StreamVoice(fixture.rack, 0, fixture.master);
    AddBlock(voice, pcm.data(), pcm.size() * 2, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    RequireRejected([&] {
        AddBlock(voice, pcm.data(), 3, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    }, "append of 3 bytes of 16-bit samples");
}};

} // namespace

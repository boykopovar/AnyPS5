#include "Ngs2Test.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include <cstdlib>

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

static constexpr int FullWaveformBlocks = static_cast<int>(0x804A8054u);

static int AddRc(uintptr_t voice, const std::vector<std::int16_t>& pcm, std::uint32_t blocks, std::uint32_t flags) {
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0};
    const Ngs2SamplerVoiceWaveformBlocksParam param{{sizeof(Ngs2SamplerVoiceWaveformBlocksParam), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS}, pcm.data(), flags, blocks, &block};
    return sceNgs2VoiceControl(voice, &param.header);
}

static void TestStreamEndsAtDeclaredSamples() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = StreamVoice(rack, 0, master);
    const std::vector<std::int16_t> first(Grain - 2, 16384);
    const std::vector<std::int16_t> second(Grain, 8192);
    const std::vector<std::int16_t> last(Grain - 2, 4096);

    AddBlock(voice, first.data(), first.size() * 2, 3 * Grain - 4, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i >= 3 ? 16383 : 0));
    Require(Flags(voice) == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING | SCE_NGS2_VOICE_STATE_FLAG_EMPTY));
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == first.size() && state.decoded_data_size == first.size() * 2 && state.waveform_data == first.data() + first.size());

    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i == 0 ? 16383 : 0));
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == first.size() && state.decoded_data_size == first.size() * 2);

    AddBlock(voice, second.data(), second.size() * 2, 3 * Grain - 4, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK && state.waveform_data == first.data() + first.size());
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < Grain - 1 ? 8191 : 0));
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == first.size() + second.size() && state.decoded_data_size == (first.size() + second.size()) * 2 && state.waveform_data == second.data() + second.size());

    AddBlock(voice, last.data(), last.size() * 2, 3 * Grain - 4, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < Grain - 3 ? 4095 : 0));
    Require(Flags(voice) == 0);
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == 0 && state.decoded_data_size == 0 && state.waveform_data == nullptr);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestStreamClosedBeforeDeclaredSamples() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = StreamVoice(rack, 0, master);
    const std::vector<std::int16_t> first(Grain - 2, 16384);
    const std::vector<std::int16_t> closing(4, 8192);

    AddBlock(voice, first.data(), first.size() * 2, 1000, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    AddBlock(voice, closing.data(), closing.size() * 2, 1000, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 3 ? 0 : 16383));
    Require(Flags(voice) == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING));
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i == 0 ? 16383 : i < 5 ? 8191 : 0));
    Require(Flags(voice) == 0);
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == 0 && state.decoded_data_size == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestStreamRejections() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const std::vector<std::int16_t> pcm(6, 1000);

    const auto complete = StreamVoice(rack, 1, master);
    AddBlock(complete, pcm.data(), pcm.size() * 2, 6, 0);
    const Ngs2WaveformBlock closedBlock{0, pcm.size() * 2, 0, 0, 6, 0, 0};
    const Ngs2SamplerVoiceWaveformBlocksParam closed{{sizeof(closed), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS}, pcm.data(), 3, 1, &closedBlock};
    Require(sceNgs2VoiceControl(complete, &closed.header) == static_cast<int>(0x804A8054u));

    const auto partial = StreamVoice(rack, 4, master);
    AddBlock(partial, pcm.data(), pcm.size() * 2, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Require(Throws([&] { AddBlock(partial, pcm.data(), 3, 20, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND); }));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static const std::int16_t Unused = 0;

static void TestSilenceBlock() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = StreamVoice(rack, 0, master);

    AddBlock(voice, &Unused, 0, Grain, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_SILENCE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == 0);
    Require(Flags(voice) == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING));
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == Grain && state.decoded_data_size == Grain * 2);

    const auto withData = StreamVoice(rack, 1, master);
    const std::vector<std::int16_t> pcm(Grain, 1234);
    AddBlock(withData, pcm.data(), pcm.size() * 2, Grain, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_SILENCE);
    Event(withData, SCE_NGS2_VOICE_EVENT_PLAY);
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 3 ? 0 : 1233));
    Require(Flags(withData) == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING));
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; ++i) Require(out[i] == (i < 3 ? 1233 : 0));
    Require(Flags(withData) == 0);

    const auto dataless = StreamVoice(rack, 2, master);
    Require(Throws([&] { AddBlock(dataless, &Unused, 0, Grain, 0); }));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static Ngs2SamplerRackOption BlockLimitOption(std::uint32_t blocks) {
    Ngs2SamplerRackOption option{};
    option.rack_option.size = sizeof(option);
    option.rack_option.max_grain_samples = 512;
    option.rack_option.max_voices = 4;
    option.rack_option.max_matrices = 1;
    option.rack_option.max_ports = 8;
    option.max_channel_works = 32;
    option.max_codec_caches = 4;
    option.max_waveform_blocks = blocks;
    option.max_envelope_points = 4;
    option.max_filters = 8;
    option.num_peak_meter_blocks = 8;
    return option;
}

static Ngs2CustomSamplerRackOption CustomBlockLimitOption(std::uint32_t blocks, std::uint32_t buffers) {
    Ngs2CustomSamplerRackOption custom{};
    custom.custom_rack_option.rack_option = BlockLimitOption(blocks).rack_option;
    custom.custom_rack_option.rack_option.size = sizeof(custom);
    custom.custom_rack_option.state_size = 48;
    custom.custom_rack_option.num_buffers = buffers;
    custom.custom_rack_option.num_modules = 1;
    custom.custom_rack_option.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_FILTER;
    custom.max_channel_works = 32;
    custom.max_waveform_blocks = blocks;
    custom.max_codec_caches = 32;
    return custom;
}

static std::int32_t APS5_VABI HeapAlloc(Ngs2ContextBufferInfo* info) {
    info->host_buffer = std::calloc(1, info->host_buffer_size + 64);
    return info->host_buffer != nullptr ? SCE_NGS2_OK : -1;
}

static std::int32_t APS5_VABI HeapFree(Ngs2ContextBufferInfo* info) {
    std::free(info->host_buffer);
    return SCE_NGS2_OK;
}

static void RequireBlockLimit(std::uint32_t rackId, const Ngs2RackOption& rackOption, std::uint32_t blocks) {
    constexpr int invalid = static_cast<int>(0x804A8053u);
    const int expected = blocks == 0 || blocks > 32 ? invalid : SCE_NGS2_OK;
    const auto system = CreateSystem();
    const Ngs2BufferAllocator allocator{HeapAlloc, HeapFree, 0};
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(rackId, &rackOption, &query) == expected);
    if (expected != SCE_NGS2_OK) query.host_buffer_size = sizeof(std::uint64_t) * 4096;
    const auto buffer = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, rackId, &rackOption, &buffer, &rack) == expected && (rack != 0) == (expected == SCE_NGS2_OK));
    if (rack != 0) Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    rack = 0;
    Require(sceNgs2RackCreateWithAllocator(system, rackId, &rackOption, &allocator, &rack) == expected && (rack != 0) == (expected == SCE_NGS2_OK));
    if (rack != 0) Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRackWaveformBlockLimit() {
    for (std::uint32_t blocks : {0u, 1u, 2u, 4u, 16u, 17u, 32u, 33u, 64u, 70u}) {
        auto sampler = BlockLimitOption(blocks);
        RequireBlockLimit(SCE_NGS2_RACK_ID_SAMPLER, sampler.rack_option, blocks);
        auto custom = CustomBlockLimitOption(blocks, 1);
        RequireBlockLimit(SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, custom.custom_rack_option.rack_option, blocks);
    }
}

static void TestRackWaveformBlockLimitComesFirst() {
    constexpr int invalid = static_cast<int>(0x804A8053u);
    const auto system = CreateSystem();
    for (std::uint32_t blocks : {0u, 33u}) {
        auto sampler = BlockLimitOption(blocks);
        sampler.num_peak_meter_blocks = 0;
        std::vector<std::uint64_t> storage(4096);
        Ngs2ContextBufferInfo buffer{};
        buffer.host_buffer = storage.data();
        buffer.host_buffer_size = storage.size() * sizeof(std::uint64_t);
        uintptr_t rack = 0;
        Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_SAMPLER, &sampler.rack_option, &buffer, &rack) == invalid);
        auto custom = CustomBlockLimitOption(blocks, 0);
        Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &custom.custom_rack_option.rack_option, &buffer, &rack) == invalid);
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestNoBlocksForClearedVoice() {
    const auto system = CreateSystem();
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = RackVoice(rack, 0);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {}});
    const Ngs2WaveformBlock unused{};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, &Unused, 0, 0, &unused});
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, &Unused, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET, 0, &unused});
    const float level = 1.0f;
    Control(voice, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 0, &level});
    Require(Throws([&] { Control(voice, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 1, 0, &level}); }));
    Require(Flags(voice) == 0);
    const std::vector<std::int16_t> pcm(4, 16384);
    Require(AddRc(voice, pcm, 1, 0) == FullWaveformBlocks);
    Require(AddRc(voice, pcm, 0, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND) == FullWaveformBlocks);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static bool Silent(const std::vector<std::int16_t>& out) {
    return std::all_of(out.begin(), out.end(), [](std::int16_t sample) { return sample == 0; });
}

static Ngs2SamplerVoiceState SamplerState(uintptr_t voice) {
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    return state;
}

struct BlockEnds {
    std::vector<std::uint32_t> sizes;
};

static void APS5_VABI RecordBlockEnd(const Ngs2VoiceCallbackInfo* info) {
    Require(info->flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
    reinterpret_cast<BlockEnds*>(info->callback_data)->sizes.push_back(info->block_size);
}

static void TestNeverSetUpVoiceClosure() {
    constexpr std::uint32_t append = SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND;
    const auto system = CreateSystem();
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const std::vector<std::int16_t> pcm(Grain, 4096);

    const auto fresh = RackVoice(rack, 0);
    Require(AddRc(fresh, pcm, 0, 3) == SCE_NGS2_OK && AddRc(fresh, pcm, 0, 0x10) == SCE_NGS2_OK);
    for (std::uint32_t flags : {0x0u, 0x1u, 0x2u, 0x3u, 0x8u, 0xAu, 0x10u, 0x20u}) Require(AddRc(fresh, pcm, 0, flags) == FullWaveformBlocks);
    for (std::uint32_t flags : {0x4u, 0xCu, 0x5u, 0x8u, 0x6u, 0x7u, 0x20u}) Require(AddRc(fresh, pcm, 0, flags) == SCE_NGS2_OK);
    Require(AddRc(fresh, pcm, 0, 1) == FullWaveformBlocks);

    const auto bare = RackVoice(rack, 1);
    Require(AddRc(bare, pcm, 0, 0) == SCE_NGS2_OK);
    Require(AddRc(bare, pcm, 1, 0) == FullWaveformBlocks && AddRc(bare, pcm, 1, 1) == FullWaveformBlocks);
    Require(AddRc(bare, pcm, 0, append) == FullWaveformBlocks);

    for (std::uint32_t flags : {0x2u, 0x3u, 0x6u, 0x7u, 0xCu}) {
        const auto open = RackVoice(rack, 2 + flags);
        Require(AddRc(open, pcm, 0, flags) == SCE_NGS2_OK);
        Require(AddRc(open, pcm, 0, 0) == ((flags & 1) != 0 ? SCE_NGS2_OK : FullWaveformBlocks));
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestIgnoredFlagBits() {
    constexpr std::uint32_t playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING;
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const std::vector<std::int16_t> pcm(Grain, 16384);
    std::uint32_t next = 0;
    const auto fresh = [&] { return StreamVoice(rack, next++, master); };

    for (std::uint32_t flags : {0x8u, 0x9u, 0xAu, 0xCu, 0x18u, 0x20u, 0x30u, 0x40u, 0x80u, 0x100u, 0x200u, 0x80000000u}) {
        const auto voice = fresh();
        Require(AddRc(voice, pcm, 0, flags) == SCE_NGS2_OK);
        Require(AddRc(voice, pcm, 1, 0) == ((flags & 1) != 0 ? SCE_NGS2_OK : FullWaveformBlocks));
    }

    for (std::uint32_t flags : {0x8u, 0x9u, 0xCu, 0x18u, 0x30u, 0x40u, 0x80u, 0x100u, 0x200u, 0x80000000u}) {
        const auto voice = fresh();
        Require(AddRc(voice, pcm, 1, flags) == SCE_NGS2_OK);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        auto out = RenderI16(system);
        for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 3 ? 0 : 16383));
        Require(Flags(voice) == playing);
        out = RenderI16(system);
        for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 3 ? 16383 : 0));
        Require(Flags(voice) == ((flags & 1) != 0 ? playing | SCE_NGS2_VOICE_STATE_FLAG_EMPTY : 0));
        Require(AddRc(voice, pcm, 1, 0) == ((flags & 1) != 0 ? SCE_NGS2_OK : FullWaveformBlocks));
        Event(voice, SCE_NGS2_VOICE_EVENT_KILL);
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestResetWithoutBlocksLikeResetBit() {
    constexpr std::uint32_t playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING;
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const std::vector<std::int16_t> pcm(Grain * 2, 4096);
    std::uint32_t next = 0;
    for (std::uint32_t flags : {0x4u, 0x6u, 0xCu, 0x5u, 0x7u}) {
        const auto voice = StreamVoice(rack, next++, master);
        Require(AddRc(voice, pcm, 1, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE) == SCE_NGS2_OK);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        Require(RenderI16(system)[3] == 4095);
        Require(AddRc(voice, pcm, 0, flags) == SCE_NGS2_OK);
        const auto out = RenderI16(system);
        for (std::uint32_t i = 3; i < Grain; i++) Require(out[i] == 0);
        Require(Flags(voice) == ((flags & 1) != 0 ? playing | SCE_NGS2_VOICE_STATE_FLAG_EMPTY : 0));
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

struct PcmType {
    std::uint32_t type;
    std::uint32_t bytes;
};

static constexpr PcmType PcmTypes[] = {
    {SCE_NGS2_WAVEFORM_TYPE_PCM_U8, 1},  {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I24L, 3},
    {SCE_NGS2_WAVEFORM_TYPE_PCM_I32L, 4}, {SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 4}, {SCE_NGS2_WAVEFORM_TYPE_PCM_F64L, 8},
};

static uintptr_t TypedVoice(uintptr_t rack, std::uint32_t index, uintptr_t master, std::uint32_t type) {
    const auto voice = RackVoice(rack, index);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {type, 1, 48000, 0, 0, 0}});
    Patch(voice, master);
    return voice;
}

static int AddFrames(uintptr_t voice, const std::vector<std::uint8_t>& data, std::uint32_t flags) {
    const Ngs2WaveformBlock block{0, data.size(), 0, 0, Grain, 0, 0};
    const Ngs2SamplerVoiceWaveformBlocksParam param{{sizeof(Ngs2SamplerVoiceWaveformBlocksParam), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS}, data.data(), flags, 1, &block};
    return sceNgs2VoiceControl(voice, &param.header);
}

static void TestLeadingAppendPiece() {
    constexpr std::uint32_t playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING;
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    std::uint32_t next = 0;
    const auto watched = [&](BlockEnds& ends, std::uint32_t type) {
        const auto voice = TypedVoice(rack, next++, master, type);
        Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, RecordBlockEnd, reinterpret_cast<std::uintptr_t>(&ends), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
        return voice;
    };

    for (const auto& format : PcmTypes) {
        const std::vector<std::uint8_t> piece(Grain * format.bytes, 0x40);
        const std::uint32_t pieceBytes = piece.size();
        for (std::uint32_t flags : {0x2u, 0x12u, 0x3u}) {
            BlockEnds ends;
            const auto voice = watched(ends, format.type);
            Require(AddFrames(voice, piece, flags) == SCE_NGS2_OK);
            Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
            for (int render = 0; render < 3; ++render) {
                Require(Silent(RenderI16(system)));
                Require(Flags(voice) == ((flags & 1) != 0 ? playing | SCE_NGS2_VOICE_STATE_FLAG_EMPTY : 0));
                const auto state = SamplerState(voice);
                Require(state.num_decoded_samples == 0 && state.decoded_data_size == ((flags & 1) != 0 ? pieceBytes : 0));
                Require(ends.sizes == std::vector<std::uint32_t>{pieceBytes});
            }
            Require(AddFrames(voice, piece, 0) == ((flags & 1) != 0 ? SCE_NGS2_OK : FullWaveformBlocks));
            Event(voice, SCE_NGS2_VOICE_EVENT_KILL);
        }
    }

    const std::vector<std::int16_t> piece(Grain, 16384);
    const std::vector<std::int16_t> block(Grain, 8192);
    const std::uint64_t pieceBytes = piece.size() * sizeof(std::int16_t);
    BlockEnds ends;
    const auto queued = watched(ends, SCE_NGS2_WAVEFORM_TYPE_PCM_I16L);
    Require(AddRc(queued, piece, 1, 3) == SCE_NGS2_OK && AddRc(queued, block, 1, 0) == SCE_NGS2_OK);
    Event(queued, SCE_NGS2_VOICE_EVENT_PLAY);
    auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 3 ? 0 : 8191));
    Require(Flags(queued) == playing);
    const auto state = SamplerState(queued);
    Require(state.num_decoded_samples == Grain && state.decoded_data_size == 2 * pieceBytes);
    Require(ends.sizes == (std::vector<std::uint32_t>{static_cast<std::uint32_t>(pieceBytes), static_cast<std::uint32_t>(pieceBytes)}));
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 3 ? 8191 : 0));
    Require(Flags(queued) == 0);
    Require(ends.sizes == (std::vector<std::uint32_t>{static_cast<std::uint32_t>(pieceBytes), static_cast<std::uint32_t>(pieceBytes)}));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestLeadingAppendPieceAfterBlocks() {
    constexpr std::uint32_t playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING;
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const std::vector<std::int16_t> piece(Grain, 16384);
    const std::vector<std::int16_t> block(Grain, 8192);
    const std::uint32_t pieceBytes = piece.size() * sizeof(std::int16_t);
    std::uint32_t next = 0;
    for (bool reset : {true, false}) {
        for (bool played : {false, true}) {
            for (std::uint32_t flags : {0x2u, 0x3u}) {
                if (!reset && !played) continue;
                BlockEnds ends;
                const auto voice = StreamVoice(rack, next++, master);
                Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, RecordBlockEnd, reinterpret_cast<std::uintptr_t>(&ends), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
                Require(AddRc(voice, block, 1, played && reset ? 0 : SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE) == SCE_NGS2_OK);
                if (played) {
                    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
                    RenderI16(system);
                    RenderI16(system);
                    Require(Flags(voice) == (reset ? 0u : playing | SCE_NGS2_VOICE_STATE_FLAG_EMPTY) && ends.sizes == std::vector<std::uint32_t>{pieceBytes});
                }
                if (reset) Require(AddRc(voice, block, 0, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE) == SCE_NGS2_OK);
                Require(AddRc(voice, piece, 1, flags) == SCE_NGS2_OK);
                if (reset) Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
                for (int render = 0; render < 3; ++render) {
                    const auto out = RenderI16(system);
                    for (std::uint32_t i = 3; i < Grain; i++) Require(out[i] == 0);
                    Require(Flags(voice) == ((flags & 1) != 0 ? playing | SCE_NGS2_VOICE_STATE_FLAG_EMPTY : 0));
                    if (reset) {
                        const auto state = SamplerState(voice);
                        Require(state.num_decoded_samples == 0 && state.decoded_data_size == ((flags & 1) != 0 ? pieceBytes : 0));
                        Require(ends.sizes == std::vector<std::uint32_t>(played ? 2 : 1, pieceBytes));
                    }
                }
                Require(AddRc(voice, block, 1, 0) == ((flags & 1) != 0 ? SCE_NGS2_OK : FullWaveformBlocks));
                Event(voice, SCE_NGS2_VOICE_EVENT_KILL);
            }
        }
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestDrainedStreamReportsEmpty() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = StreamVoice(rack, 0, master);
    const std::vector<std::int16_t> pcm(4, 16384);
    constexpr std::uint32_t playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING;

    AddBlock(voice, pcm.data(), pcm.size() * 2, 4, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    RenderI16(system);
    Require(Flags(voice) == (playing | SCE_NGS2_VOICE_STATE_FLAG_EMPTY));

    const std::vector<std::int16_t> longer(Grain * 2, 8192);
    AddBlock(voice, longer.data(), longer.size() * 2, Grain * 2, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
    const auto out = RenderI16(system);
    Require(out[0] == 8191 && out[3] == 8191);
    Require(Flags(voice) == playing);

    AddBlock(voice, pcm.data(), pcm.size() * 2, 4, 0);
    RenderI16(system);
    RenderI16(system);
    Require((Flags(voice) & SCE_NGS2_VOICE_STATE_FLAG_EMPTY) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

int main() {
    TestStreamEndsAtDeclaredSamples();
    TestStreamClosedBeforeDeclaredSamples();
    TestStreamRejections();
    TestRackWaveformBlockLimit();
    TestRackWaveformBlockLimitComesFirst();
    TestSilenceBlock();
    TestDrainedStreamReportsEmpty();
    TestNoBlocksForClearedVoice();
    TestNeverSetUpVoiceClosure();
    TestIgnoredFlagBits();
    TestResetWithoutBlocksLikeResetBit();
    TestLeadingAppendPiece();
    TestLeadingAppendPieceAfterBlocks();
    return 0;
}

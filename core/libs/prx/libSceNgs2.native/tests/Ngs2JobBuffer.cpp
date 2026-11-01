#include <algorithm>
#include "Ngs2JobTest.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <bit>
#include <utility>
#include <stdexcept>

enum class Update { Play, Start, Pause, Kill, Volume, Matrix, Combined, Pitch, Gain, PlayStop, StopPlay, PauseResume, KillPlay, PlayKill, PlayPause, StopResume, LongPause, Mute, SelectMatrix, Over, Shift, ExternalShift, UnchangedShift };
struct BufferedState {
    std::uintptr_t voice;
    std::uintptr_t master;
    Update update;
    bool commands;
    bool custom;
    unsigned tick = 0;
};

static void CommonControl(BufferedState& state, std::uint32_t id, float level) {
    if (state.commands) {
        Ngs2VoiceCommand command{};
        command.id = id == SCE_NGS2_VOICE_PARAM_PORT_VOLUME ? 6 : 5;
        command.type = command.id == 6 ? 1 : 0x11;
        command.count = 1;
        if (command.id == 6) command.value.f = level;
        else command.value.levels = &level;
        Require(sceNgs2VoiceRunCommands(state.voice, &command, 1) == SCE_NGS2_OK);
    } else if (id == SCE_NGS2_VOICE_PARAM_PORT_VOLUME)
        Control(state.voice, id, Ngs2VoicePortVolumeParam{{}, 0, level});
    else Control(state.voice, id, Ngs2VoiceMatrixLevelsParam{{}, 0, 1, &level});
}

static void WorkerEvent(BufferedState& state, std::uint32_t event) {
    if (!state.commands) Event(state.voice, event);
    else {
        Ngs2VoiceCommand command{};
        command.id = 2;
        command.type = 4;
        command.value.u = event;
        Require(sceNgs2VoiceRunCommands(state.voice, &command, 1) == SCE_NGS2_OK);
    }
}

static int APS5_VABI UpdatePeer(Ngs2UserFxProcessContext* context) {
    auto& state = *reinterpret_cast<BufferedState*>(context->user_data0);
    const bool shift = state.update >= Update::Shift;
    if (state.tick == (shift ? 10u : 0u)) {
        switch (state.update) {
            case Update::Play:
            case Update::Start: WorkerEvent(state, SCE_NGS2_VOICE_EVENT_PLAY); break;
            case Update::LongPause:
            case Update::Pause: WorkerEvent(state, SCE_NGS2_VOICE_EVENT_PAUSE); break;
            case Update::Kill: WorkerEvent(state, SCE_NGS2_VOICE_EVENT_KILL); break;
            case Update::Mute:
            case Update::Volume:
            case Update::Combined: {
                CommonControl(state, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, state.update == Update::Mute ? 0.0f : 0.25f);
                Ngs2VoicePortInfo info{};
                Require(sceNgs2VoiceGetPortInfo(state.voice, 0, &info, sizeof(info)) == SCE_NGS2_OK);
                Require(info.volume == (state.update == Update::Mute ? 0.0f : 0.25f));
                if (state.update != Update::Combined) break;
                [[fallthrough]];
            }
            case Update::Matrix: {
                CommonControl(state, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, 0.5f);
                struct { Ngs2VoiceMatrixInfo info; std::uint32_t guard; } result;
                std::memset(&result, 0xa5, sizeof(result));
                Require(sceNgs2VoiceGetMatrixInfo(state.voice, 0, &result.info, sizeof(result)) == SCE_NGS2_OK);
                Require(result.info.num_levels == 1 && result.info.levels[0] == 0.5f && result.guard == 0xa5a5a5a5);
                const auto* unused = reinterpret_cast<const unsigned char*>(result.info.levels + 1);
                const auto* end = reinterpret_cast<const unsigned char*>(&result.info) + sizeof(result.info);
                Require(std::all_of(unused, end, [](auto byte) { return byte == 0xa5; }));
                break;
            }
            case Update::Over: {
                const auto volume = [&] {
                    Ngs2VoicePortInfo info{};
                    Require(sceNgs2VoiceGetPortInfo(state.voice, 0, &info, sizeof(info)) == SCE_NGS2_OK);
                    return info.volume;
                };
                const auto send = [&](float level, std::uint8_t type) {
                    if (!state.commands) return CommonControl(state, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, level);
                    Ngs2VoiceCommand command{};
                    command.id = 6;
                    command.type = type;
                    command.count = 1;
                    command.value.f = level;
                    Require(sceNgs2VoiceRunCommands(state.voice, &command, 1) == SCE_NGS2_OK);
                };
                if (state.commands) {
                    send(2.0f, 3);
                    Require(volume() == 2.0f);
                }
                send(-1.0f, 1);
                Require(std::bit_cast<std::uint32_t>(volume()) == 0);
                send(std::bit_cast<float>(0x7fc00000u), 1);
                Require(std::bit_cast<std::uint32_t>(volume()) == 0x7fc00000u);
                send(-0.0f, 1);
                Require(std::bit_cast<std::uint32_t>(volume()) == 0x80000000u);
                send(33.0f, 1);
                Require(volume() == 32.0f);
                break;
            }
            case Update::Shift:
                Control(state.voice, SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT | 0x20, Ngs2CustomVoicePitchShiftParam{{}, -1200, 0, 0, 0});
                break;
            case Update::ExternalShift:
            case Update::UnchangedShift: break;
            case Update::SelectMatrix: {
                if (state.commands) {
                    Ngs2VoiceCommand command{};
                    command.id = 7; command.type = 3; command.value.i = 0;
                    Require(sceNgs2VoiceRunCommands(state.voice, &command, 1) == SCE_NGS2_OK);
                } else Control(state.voice, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
                Ngs2VoicePortInfo info{};
                Require(sceNgs2VoiceGetPortInfo(state.voice, 0, &info, sizeof(info)) == SCE_NGS2_OK && info.matrix_id == 0);
                break;
            }
            case Update::Pitch:
                Control(state.voice, state.custom ? SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_PITCH : SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH,
                        Ngs2SamplerVoicePitchParam{{}, 2.0f, 0});
                break;
            case Update::PlayStop:
            case Update::StopPlay:
            case Update::PauseResume:
            case Update::KillPlay:
            case Update::PlayKill:
            case Update::PlayPause:
            case Update::StopResume: {
                const auto first = state.update == Update::StopPlay || state.update == Update::StopResume ? SCE_NGS2_VOICE_EVENT_STOP :
                    state.update == Update::PauseResume ? SCE_NGS2_VOICE_EVENT_PAUSE :
                    state.update == Update::KillPlay ? SCE_NGS2_VOICE_EVENT_KILL : SCE_NGS2_VOICE_EVENT_PLAY;
                const auto second = state.update == Update::PlayStop ? SCE_NGS2_VOICE_EVENT_STOP :
                    state.update == Update::PlayKill ? SCE_NGS2_VOICE_EVENT_KILL :
                    state.update == Update::PlayPause ? SCE_NGS2_VOICE_EVENT_PAUSE :
                    state.update == Update::PauseResume || state.update == Update::StopResume ? SCE_NGS2_VOICE_EVENT_RESUME : SCE_NGS2_VOICE_EVENT_PLAY;
                WorkerEvent(state, first);
                WorkerEvent(state, second);
                break;
            }
            case Update::Gain:
                Control(state.master, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 0.25f, 0.5f});
                break;
        }
        if (state.update == Update::Play || state.update == Update::Start) {
            std::uint32_t flags = 0;
            Require(sceNgs2VoiceGetStateFlags(state.voice, &flags) == SCE_NGS2_OK && flags == 1);
        }
    }
    if ((state.tick == 2 && state.update == Update::Pause) || (state.tick == 5 && state.update == Update::LongPause)) WorkerEvent(state, SCE_NGS2_VOICE_EVENT_RESUME);
    ++state.tick;
    std::fill_n(context->channel_data[0], context->num_grain_samples, 0.0f);
    return SCE_NGS2_OK;
}

static void TestBuffer(Update update, bool custom, bool commands, unsigned order, unsigned grainFrames, std::vector<float>* captured = nullptr) {
    const auto firstBuffer = usedBuffers;
    Manager manager{{}, order};
    Ngs2JobSchedulerOption scheduler{};
    scheduler.size = sizeof(scheduler);
    scheduler.user_data = reinterpret_cast<std::uintptr_t>(&manager);
    scheduler.query_handler = Query;
    scheduler.init_handler = Initialize;
    scheduler.destroy_handler = Destroy;
    scheduler.submit_handler = Submit;
    scheduler.flush_handler = Complete;
    scheduler.join_handler = Complete;
    scheduler.num_workers = 2;
    scheduler.max_jobs = 16;
    Ngs2SystemOption option{};
    option.size = sizeof(option);
    option.max_grain_samples = 512;
    option.num_grain_samples = grainFrames;
    option.sample_rate = 48000;
    option.max_voice_channels = 8;
    option.job_scheduler_options[0] = reinterpret_cast<std::uintptr_t>(&scheduler);
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t system = 0;
    Require(sceNgs2SystemCreate(&option, &buffer, &system) == SCE_NGS2_OK);
    const auto master = Mastering(system, 1);
    const auto sender = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 1, 0});
    const auto voice = Sampler(system, custom);
    Control(voice, custom ? SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_SETUP : SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    std::array<std::int16_t, 4096> pcm{};
    pcm.fill(1000);
    const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, pcm.size(), 0, 0};
    Control(voice, custom ? SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS : SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    const bool shift = update >= Update::Shift;
    if (shift) Control(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT, Ngs2CustomVoicePitchShiftParam{{}, 1200, 0, 0, 0});
    if (update == Update::Matrix || update == Update::Combined || update == Update::SelectMatrix) {
        const float level = update == Update::SelectMatrix ? 0.5f : 1.0f;
        Control(voice, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 1, &level});
        if (update != Update::SelectMatrix) Control(voice, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    }
    BufferedState state{voice, master, update, commands, custom};
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
            Ngs2SubmixerVoiceUserFxParam{{}, UpdatePeer, reinterpret_cast<std::uintptr_t>(&state), 0, 0});
    Patch(voice, master);
    Patch(sender, master);
    if (update != Update::Start) Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    Event(sender, SCE_NGS2_VOICE_EVENT_PLAY);
    std::array<float, 257> samples{};
    const Ngs2RenderBufferInfo output{samples.data(), sizeof(samples), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    const unsigned grains = shift ? 16 : update == Update::LongPause ? 8 : 5;
    const bool restart = update == Update::Play || update == Update::Start || update == Update::StopPlay || update == Update::KillPlay || update == Update::PlayStop || update == Update::PlayPause;
    const bool killed = update == Update::Kill || update == Update::PlayKill;
    const bool stopped = update == Update::PlayStop || update == Update::StopResume;
    const bool paused = update == Update::Pause || update == Update::LongPause;
    const unsigned resumeGrain = update == Update::LongPause ? 6 : 3;
    for (unsigned grain = 0; grain < grains; ++grain) {
        samples.fill(-1234.0f);
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK && manager.batches.empty());
        std::uint32_t flags = 0;
        Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_OK);
        const auto expectedFlags = (killed || stopped) && grain > 0 ? 0u :
            restart && grain == 0 ? 1u : update == Update::PlayPause && grain > 0 ? 7u :
            paused && grain == 1 ? 5u : paused && grain >= 2 && grain < resumeGrain ? 7u : 3u;
        Require(flags == expectedFlags);
        if (!custom) {
            Ngs2SamplerVoiceState result{};
            Require(sceNgs2VoiceGetState(voice, &result.voice_state, sizeof(result)) == SCE_NGS2_OK);
            const std::uint64_t decoded = (killed || stopped) && grain > 0 ? 0 :
                update == Update::PlayPause ? (grain == 0 ? 0 : grainFrames) :
                restart ? grain * grainFrames :
                update == Update::Pitch ? (1 + 2 * grain) * grainFrames :
                update == Update::LongPause ? (grain < 3 ? grain + 1 : grain < resumeGrain ? 3 : grain - resumeGrain + 4) * grainFrames : (grain + 1) * grainFrames;
            Require(result.voice_state.state_flags == flags && result.num_decoded_samples == decoded);
        }
        if (captured) captured->insert(captured->end(), samples.begin(), samples.begin() + grainFrames);
        for (unsigned i = 0; !shift && i < grainFrames; ++i) {
            if (update == Update::Over) {
                if (grain != 1) Require(samples[i] == (grain == 0 && i < 3 ? 0.0f : 1000.0f / 32768.0f) * (grain == 0 ? 1.0f : 32.0f));
                continue;
            }
            float expected = 1000.0f / 32768.0f;
            const bool restarting = grain == 0 || (restart && grain == 1);
            if (restarting && i < 3) expected = 0;
            if (killed || (update == Update::KillPlay && !custom && grain == 0) || (update == Update::Start && grain == 0) || (update == Update::PlayPause && grain > 0)) expected = 0;
            if (stopped && grain > 0) expected *= grain == 1 && i < 64 ? 1.0f - static_cast<float>(i) / 64.0f : 0.0f;
            if (paused) {
                if (grain == 1) expected *= i < 64 ? 1.0f - static_cast<float>(i) / 64.0f : 0.0f;
                if (grain >= 2 && grain < resumeGrain) expected = 0;
                if (grain == resumeGrain && i < 64) expected *= static_cast<float>(i) / 64.0f;
            }
            const float target = update == Update::Mute ? 0.0f : update == Update::Volume ? 0.25f : update == Update::Matrix || update == Update::SelectMatrix ? 0.5f : update == Update::Combined ? 0.125f : 1.0f;
            expected *= grain == 0 ? 1.0f : grain == 1 ? 1.0f + (target - 1.0f) * static_cast<float>(i) / grainFrames : target;
            if (update == Update::Gain && grain > 0) expected *= 0.25f;
            if (std::fabs(samples[i] - expected) > 1.0e-7f) std::fprintf(stderr,
                "update=%u custom=%d commands=%d order=%u frames=%u grain=%u sample=%u actual=%g expected=%g\n",
                static_cast<unsigned>(update), custom, commands, order, grainFrames, grain, i, samples[i], expected);
            Require(std::fabs(samples[i] - expected) <= 1.0e-7f);
        }
        for (unsigned i = grainFrames; i < samples.size(); ++i) Require(samples[i] == -1234.0f);
        if (grain == 10 && update == Update::ExternalShift)
            Control(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT | 0x20, Ngs2CustomVoicePitchShiftParam{{}, -1200, 0, 0, 0});
    }
    Require(state.tick == grains && sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

enum class SetupOrder { SetupOnly, PlaySetup, SetupPlay };
struct SetupState {
    std::uintptr_t target;
    SetupOrder order;
    unsigned tick = 0;
};

static int APS5_VABI SetupPeer(Ngs2UserFxProcessContext* context) {
    auto& state = *reinterpret_cast<SetupState*>(context->user_data0);
    if (state.tick++ == 0) {
        bool rejected = false;
        try {
            Control(state.target, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 0, 0});
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        Require(rejected);
        if (state.order == SetupOrder::PlaySetup) Event(state.target, SCE_NGS2_VOICE_EVENT_PLAY);
        Control(state.target, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 1, 0});
        if (state.order == SetupOrder::SetupPlay) Event(state.target, SCE_NGS2_VOICE_EVENT_PLAY);
    }
    std::fill_n(context->channel_data[0], context->num_grain_samples, 0.0f);
    return SCE_NGS2_OK;
}

static void TestBufferedSetup(SetupOrder order) {
    const auto firstBuffer = usedBuffers;
    Ngs2SystemOption option{};
    option.size = sizeof(option);
    option.max_grain_samples = 512;
    option.num_grain_samples = 64;
    option.sample_rate = 48000;
    option.max_voice_channels = 8;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t system = 0;
    Require(sceNgs2SystemCreate(&option, &buffer, &system) == SCE_NGS2_OK);
    const auto master = Mastering(system, 1);
    const auto sender = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    const auto target = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    for (const auto voice : {sender, target}) Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 1, 0});
    const auto voice = Sampler(system, false);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    std::array<std::int16_t, 4096> pcm{};
    pcm.fill(1000);
    const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, pcm.size(), 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    SetupState state{target, order};
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX, Ngs2SubmixerVoiceUserFxParam{{}, SetupPeer, reinterpret_cast<std::uintptr_t>(&state), 0, 0});
    Patch(voice, target);
    Patch(target, master);
    Patch(sender, master);
    for (const auto source : {voice, sender, target}) Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    std::array<float, 65> samples{};
    const Ngs2RenderBufferInfo output{samples.data(), sizeof(samples), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    for (unsigned grain = 0; grain < 3; ++grain) {
        samples.fill(-1234.0f);
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        for (unsigned i = 0; i < 64; ++i) Require(samples[i] == (grain == 0 && i >= 3 ? 1000.0f / 32768.0f : 0.0f));
        Require(samples[64] == -1234.0f);
        if (grain == 0) continue;
        Require(Flags(target) == (order == SetupOrder::SetupPlay ? static_cast<std::uint32_t>(SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING) : 0u));
        Ngs2VoicePortInfo info{};
        Require(sceNgs2VoiceGetPortInfo(target, 0, &info, sizeof(info)) == SCE_NGS2_OK && info.dest_handle == 0);
    }
    Require(state.tick == 3 && sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

struct SamplerSetupState {
    std::uintptr_t voice;
    std::uintptr_t mixer;
    std::uintptr_t master;
    const std::int16_t* pcm;
    unsigned tick = 0;
    unsigned blockEnds = 0;
};

static void APS5_VABI CountBlockEnd(const Ngs2VoiceCallbackInfo* info) {
    auto& state = *reinterpret_cast<SamplerSetupState*>(info->callback_data);
    Require(info->flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
    ++state.blockEnds;
}

static int APS5_VABI SetupSamplerPeer(Ngs2UserFxProcessContext* context) {
    auto& state = *reinterpret_cast<SamplerSetupState*>(context->user_data0);
    if (state.tick++ == 0) {
        Control(state.voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
        Control(state.voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
                Ngs2VoiceCallbackParam{{}, CountBlockEnd, reinterpret_cast<std::uintptr_t>(&state), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
        Patch(state.voice, state.master);
        Ngs2VoicePortInfo info{};
        Require(sceNgs2VoiceGetPortInfo(state.voice, 0, &info, sizeof(info)) == SCE_NGS2_OK && info.dest_handle == state.master);
        std::array<Ngs2WaveformBlock, 1> blocks{{{0, 256, 0, 0, 128, 0, 0}}};
        Control(state.voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, state.pcm, 0, 1, blocks.data()});
        blocks.fill({});
        const Ngs2WaveformBlock extra{0, 256, 0, 0, 128, 0, 0};
        Ngs2SamplerVoiceWaveformBlocksParam closed{{sizeof(closed), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS}, state.pcm, 0, 1, &extra};
        Require(sceNgs2VoiceControl(state.voice, &closed.header) == static_cast<int>(0x804A8054u));
        Event(state.voice, SCE_NGS2_VOICE_EVENT_PLAY);
        Control(state.mixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 1, 0});
        Event(state.mixer, SCE_NGS2_VOICE_EVENT_PLAY);
    }
    std::fill_n(context->channel_data[0], context->num_grain_samples, 0.0f);
    return SCE_NGS2_OK;
}

static void TestBufferedSamplerSetup() {
    const auto firstBuffer = usedBuffers;
    Ngs2SystemOption option{};
    option.size = sizeof(option);
    option.max_grain_samples = 512;
    option.num_grain_samples = 64;
    option.sample_rate = 48000;
    option.max_voice_channels = 8;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t system = 0;
    Require(sceNgs2SystemCreate(&option, &buffer, &system) == SCE_NGS2_OK);
    const auto master = Mastering(system, 1);
    const auto sender = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 1, 0});
    const auto mixer = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    const auto voice = Sampler(system, false);
    std::array<std::int16_t, 128> pcm{};
    pcm.fill(1000);
    SamplerSetupState state{voice, mixer, master, pcm.data()};
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX, Ngs2SubmixerVoiceUserFxParam{{}, SetupSamplerPeer, reinterpret_cast<std::uintptr_t>(&state), 0, 0});
    Patch(sender, master);
    Event(sender, SCE_NGS2_VOICE_EVENT_PLAY);
    std::array<float, 65> samples{};
    const Ngs2RenderBufferInfo output{samples.data(), sizeof(samples), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    for (unsigned grain = 0; grain < 6; ++grain) {
        samples.fill(-1234.0f);
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        for (unsigned i = 0; grain < 3 && i < 64; ++i) Require(samples[i] == (grain == 2 || (grain == 1 && i >= 3) ? 1000.0f / 32768.0f : 0.0f));
        Require(samples[64] == -1234.0f);
        Require(Flags(mixer) == (grain == 0 ? static_cast<std::uint32_t>(SCE_NGS2_VOICE_STATE_FLAG_INUSE) : static_cast<std::uint32_t>(SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING)));
    }
    Require(state.tick == 6 && state.blockEnds == 1 && sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

struct PeerAction {
    std::function<void(unsigned)> action;
    unsigned tick = 0;
};

static int APS5_VABI RunPeerAction(Ngs2UserFxProcessContext* context) {
    auto& peer = *reinterpret_cast<PeerAction*>(context->user_data0);
    peer.action(peer.tick++);
    std::fill_n(context->channel_data[0], context->num_grain_samples, 0.0f);
    return SCE_NGS2_OK;
}

struct PeerSystem {
    std::uintptr_t system;
    std::uintptr_t master;
};

static PeerSystem CreatePeerSystem(PeerAction& peer, const Ngs2JobSchedulerOption* scheduler = nullptr) {
    Ngs2SystemOption option{};
    option.size = sizeof(option);
    option.max_grain_samples = 512;
    option.num_grain_samples = 64;
    option.sample_rate = 48000;
    option.max_voice_channels = 8;
    if (scheduler != nullptr) option.job_scheduler_options[0] = reinterpret_cast<std::uintptr_t>(scheduler);
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t system = 0;
    Require(sceNgs2SystemCreate(&option, &buffer, &system) == SCE_NGS2_OK);
    const auto master = Mastering(system, 1);
    const auto sender = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 1, 0});
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX, Ngs2SubmixerVoiceUserFxParam{{}, RunPeerAction, reinterpret_cast<std::uintptr_t>(&peer), 0, 0});
    Patch(sender, master);
    Event(sender, SCE_NGS2_VOICE_EVENT_PLAY);
    return {system, master};
}

static std::uintptr_t MonoSampler(std::uintptr_t system, std::uint32_t waveformType = SCE_NGS2_WAVEFORM_TYPE_PCM_I16L) {
    const auto voice = Sampler(system, false);
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {waveformType, 1, 48000, 0, 0, 0}});
    return voice;
}

static int AddBlocks(std::uintptr_t voice, const void* data, std::uint32_t flags, const Ngs2WaveformBlock& block) {
    Ngs2SamplerVoiceWaveformBlocksParam param{{sizeof(param), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS}, data, flags, 1, &block};
    return sceNgs2VoiceControl(voice, &param.header);
}

template <typename TError, typename TAction>
static void ExpectRejected(unsigned& accepted, const char* name, TAction action) {
    bool thrown = false;
    try { action(); } catch (const TError&) { thrown = true; }
    if (thrown) return;
    std::fprintf(stderr, "NGS2 capture validation: %s was accepted\n", name);
    ++accepted;
}

static void RenderGrain(std::uintptr_t system) {
    std::array<float, 64> samples{};
    const Ngs2RenderBufferInfo output{samples.data(), sizeof(samples), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
}

static void TestCaptureValidation() {
    const auto firstBuffer = usedBuffers;
    std::array<std::int16_t, 256> pcm{};
    pcm.fill(1000);
    PeerAction peer;
    const auto setup = CreatePeerSystem(peer);
    const auto configured = MonoSampler(setup.system);
    const auto stream = MonoSampler(setup.system);
    Require(AddBlocks(stream, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, {0, 64, 0, 0, 4096, 0, 0}) == SCE_NGS2_OK);
    const auto atrac9 = Sampler(setup.system, false);
    const auto fresh = Sampler(setup.system, false);
    unsigned checked = 0;
    unsigned accepted = 0;
    peer.action = [&](unsigned tick) {
        if (tick != 0) return;
        ExpectRejected<std::invalid_argument>(accepted, "a block whose data does not fit", [&] { AddBlocks(configured, pcm.data(), 0, {0, 1, 0, 0, 2, 0, 0}); });
        ExpectRejected<std::runtime_error>(accepted, "a partial-frame append", [&] {
            AddBlocks(stream, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, {64, 3, 0, 0, 0, 0, 0});
        });
        ExpectRejected<std::invalid_argument>(accepted, "an ATRAC9 setup with a mismatched config", [&] {
            Control(atrac9, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, 0, 0, 0}});
        });
        Control(fresh, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
        ExpectRejected<std::invalid_argument>(accepted, "a block that does not fit a buffered setup", [&] { AddBlocks(fresh, pcm.data(), 0, {0, 1, 0, 0, 2, 0, 0}); });
        Require(accepted == 0);
        Require(AddBlocks(fresh, pcm.data(), 0, {0, 4, 0, 0, 2, 0, 0}) == SCE_NGS2_OK);
        Require(AddBlocks(configured, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND, {0, 2, 0, 0, 0, 0, 0}) == SCE_NGS2_OK);
        Require(AddBlocks(configured, pcm.data(), 0, {0, 4, 0, 0, 2, 0, 0}) == static_cast<int>(0x804A8054u));
        Require(AddBlocks(stream, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, {64, 4, 0, 0, 0, 0, 0}) == SCE_NGS2_OK);
        ++checked;
    };
    for (unsigned grain = 0; grain < 3; ++grain) RenderGrain(setup.system);
    Require(checked == 1 && sceNgs2SystemDestroy(setup.system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static int AddNoBlocks(std::uintptr_t voice, std::uint32_t flags) {
    const Ngs2WaveformBlock unused{};
    Ngs2SamplerVoiceWaveformBlocksParam param{{sizeof(param), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS}, &unused, flags, 0, &unused};
    return sceNgs2VoiceControl(voice, &param.header);
}

static void TestBufferedWaveformCodes() {
    const auto firstBuffer = usedBuffers;
    constexpr int full = static_cast<int>(0x804A8054u);
    constexpr std::uint32_t playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING;
    std::array<std::int16_t, 128> pcm{};
    pcm.fill(1000);
    PeerAction peer;
    const auto setup = CreatePeerSystem(peer);
    const auto never = Sampler(setup.system, false);
    const auto ignored = MonoSampler(setup.system);
    const auto leading = MonoSampler(setup.system);
    const auto reopened = MonoSampler(setup.system);
    for (const auto voice : {ignored, leading, reopened}) Patch(voice, setup.master);
    unsigned checked = 0;
    peer.action = [&](unsigned tick) {
        if (tick != 0) return;
        Require(AddNoBlocks(never, 0) == SCE_NGS2_OK);
        Require(AddNoBlocks(never, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND) == full);
        Require(AddBlocks(never, pcm.data(), 0, {0, 128, 0, 0, 64, 0, 0}) == full);
        Require(AddBlocks(ignored, pcm.data(), 0x9, {0, 128, 0, 0, 64, 0, 0}) == SCE_NGS2_OK);
        Require(AddNoBlocks(ignored, 0x20) == SCE_NGS2_OK);
        Require(AddBlocks(ignored, pcm.data(), 1, {0, 128, 0, 0, 64, 0, 0}) == full);
        Require(AddBlocks(leading, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, {0, 128, 0, 0, 64, 0, 0}) == SCE_NGS2_OK);
        Require(AddBlocks(leading, pcm.data(), 1, {0, 128, 0, 0, 64, 0, 0}) == SCE_NGS2_OK);
        Require(AddBlocks(reopened, pcm.data(), 1, {0, 128, 0, 0, 64, 0, 0}) == SCE_NGS2_OK);
        Require(AddNoBlocks(reopened, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE) == SCE_NGS2_OK);
        Require(AddBlocks(reopened, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, {0, 128, 0, 0, 64, 0, 0}) == SCE_NGS2_OK);
        ++checked;
    };
    RenderGrain(setup.system);
    Require(checked == 1 && Flags(ignored) == 0 && Flags(leading) == 0 && Flags(reopened) == 0);
    Event(ignored, SCE_NGS2_VOICE_EVENT_PLAY);
    Event(leading, SCE_NGS2_VOICE_EVENT_PLAY);
    Event(reopened, SCE_NGS2_VOICE_EVENT_PLAY);
    RenderGrain(setup.system);
    Require(Flags(ignored) == playing);
    RenderGrain(setup.system);
    Require(Flags(ignored) == 0);
    for (unsigned grain = 0; grain < 3; ++grain) RenderGrain(setup.system);
    Require(Flags(leading) == (playing | SCE_NGS2_VOICE_STATE_FLAG_EMPTY) && Flags(reopened) == (playing | SCE_NGS2_VOICE_STATE_FLAG_EMPTY) && Flags(never) == 0);
    Require(sceNgs2SystemDestroy(setup.system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestBufferedAppendAfterDrain() {
    const auto firstBuffer = usedBuffers;
    std::array<std::int16_t, 64> pcm{};
    pcm.fill(1000);
    PeerAction peer;
    const auto setup = CreatePeerSystem(peer);
    const auto voice = MonoSampler(setup.system);
    Require(AddBlocks(voice, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, {0, 32, 0, 0, 16, 0, 0}) == SCE_NGS2_OK);
    Patch(voice, setup.master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    peer.action = [&](unsigned tick) {
        if (tick != 0) return;
        Require(AddBlocks(voice, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, {32, 32, 0, 0, 0, 0, 0}) == SCE_NGS2_OK);
        Control(voice, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.5f});
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    };
    RenderGrain(setup.system);
    RenderGrain(setup.system);
    constexpr std::uint32_t waiting = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING | SCE_NGS2_VOICE_STATE_FLAG_EMPTY;
    Require(Flags(voice) == waiting);
    RenderGrain(setup.system);
    Require(Flags(voice) == waiting);
    Ngs2VoicePortInfo info{};
    Require(sceNgs2VoiceGetPortInfo(voice, 0, &info, sizeof(info)) == SCE_NGS2_OK && info.volume == 0.5f && info.dest_handle == setup.master);
    Require(sceNgs2SystemDestroy(setup.system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void RequirePort(std::uintptr_t voice, std::uintptr_t dest, float volume) {
    Ngs2VoicePortInfo info{};
    Require(sceNgs2VoiceGetPortInfo(voice, 0, &info, sizeof(info)) == SCE_NGS2_OK);
    Require(info.dest_handle == dest && info.volume == volume && info.matrix_id == -1 && info.dest_input_id == 0);
}

static void TestEmptySetupPort() {
    const auto firstBuffer = usedBuffers;
    PeerAction peer;
    const auto setup = CreatePeerSystem(peer);
    const auto configured = MonoSampler(setup.system);
    const auto bare = Sampler(setup.system, false);
    for (const auto voice : {configured, bare}) {
        Patch(voice, setup.master);
        Control(voice, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.25f});
    }
    peer.action = [&](unsigned tick) {
        if (tick != 0) return;
        for (const auto voice : {configured, bare}) Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{});
        RequirePort(configured, setup.master, 0.25f);
        RequirePort(bare, 0, 1.0f);
    };
    for (unsigned grain = 0; grain < 2; ++grain) {
        RenderGrain(setup.system);
        RequirePort(configured, setup.master, 0.25f);
        RequirePort(bare, 0, 1.0f);
    }
    Require(sceNgs2SystemDestroy(setup.system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestPendingPatchDestroyedDest() {
    const auto firstBuffer = usedBuffers;
    PeerAction peer;
    const auto setup = CreatePeerSystem(peer);
    const auto source = Voice(CreateRack(setup.system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 1, 0});
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_SUBMIXER, nullptr, &query) == SCE_NGS2_OK);
    const auto destBuffer = Buffer(query);
    std::uintptr_t destRack = 0;
    Require(TestRackCreate(setup.system, SCE_NGS2_RACK_ID_SUBMIXER, nullptr, &destBuffer, &destRack) == SCE_NGS2_OK);
    const auto dest = Voice(destRack);
    Control(dest, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 1, 0});
    const auto rackSource = Voice(CreateRack(setup.system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(rackSource, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 1, 0});
    peer.action = [&](unsigned tick) {
        if (tick != 0) return;
        Patch(source, dest);
        Patch(rackSource, destRack);
        RequirePort(source, dest, 1.0f);
        RequirePort(rackSource, dest, 1.0f);
    };
    RenderGrain(setup.system);
    RequirePort(source, dest, 1.0f);
    Require(sceNgs2RackDestroy(destRack, nullptr) == SCE_NGS2_OK);
    RequirePort(source, 0, 1.0f);
    std::uintptr_t reused = 0;
    Require(TestRackCreate(setup.system, SCE_NGS2_RACK_ID_SUBMIXER, nullptr, &destBuffer, &reused) == SCE_NGS2_OK && reused == destRack);
    RequirePort(source, 0, 1.0f);
    RequirePort(rackSource, 0, 1.0f);
    RenderGrain(setup.system);
    RequirePort(source, 0, 1.0f);
    RequirePort(rackSource, 0, 1.0f);
    Require(sceNgs2SystemDestroy(setup.system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

struct RaceManager {
    Manager base;
    std::function<void()> control;
    bool started = false;
};

static int APS5_VABI RaceSubmit(std::uintptr_t handle, Ngs2JobHandler handler, void* context, std::uint32_t count) {
    reinterpret_cast<RaceManager*>(handle)->base.batches.push_back({handler, context, count});
    return SCE_NGS2_OK;
}

static int APS5_VABI RaceComplete(std::uintptr_t handle) {
    auto& manager = *reinterpret_cast<RaceManager*>(handle);
    std::thread control;
    if (!manager.started && !manager.base.batches.empty() && manager.control) {
        manager.started = true;
        control = std::thread(manager.control);
    }
    for (const auto& batch : manager.base.batches) {
        std::vector<std::thread> workers;
        for (std::uint32_t i = 0; i < batch.count; ++i) workers.emplace_back([&, i] { batch.handler(batch.context, i); });
        for (auto& worker : workers) worker.join();
    }
    manager.base.batches.clear();
    if (control.joinable()) control.join();
    return SCE_NGS2_OK;
}

static int APS5_VABI RaceDestroy(std::uintptr_t handle) {
    Require(reinterpret_cast<RaceManager*>(handle)->base.batches.empty());
    return SCE_NGS2_OK;
}

static void TestBlockQueueRace() {
    const auto firstBuffer = usedBuffers;
    RaceManager manager{{{}, 2}, {}, false};
    Ngs2JobSchedulerOption scheduler{};
    scheduler.size = sizeof(scheduler);
    scheduler.user_data = reinterpret_cast<std::uintptr_t>(&manager);
    scheduler.query_handler = Query;
    scheduler.init_handler = Initialize;
    scheduler.destroy_handler = RaceDestroy;
    scheduler.submit_handler = RaceSubmit;
    scheduler.flush_handler = RaceComplete;
    scheduler.join_handler = RaceComplete;
    scheduler.num_workers = 2;
    scheduler.max_jobs = 16;
    PeerAction peer;
    peer.action = [](unsigned) {};
    const auto setup = CreatePeerSystem(peer, &scheduler);
    const auto voice = MonoSampler(setup.system);
    std::array<std::int16_t, 4096> pcm{};
    pcm.fill(1000);
    Require(AddBlocks(voice, pcm.data(), 0, {0, 32, 0, 0, 16, 0, 0}) == SCE_NGS2_OK);
    Patch(voice, setup.master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    bool controlled = false;
    manager.control = [&] {
        Require(AddBlocks(voice, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET, {0, sizeof(pcm), 0, 0, pcm.size(), 0, 0}) == SCE_NGS2_OK);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        controlled = true;
    };
    std::array<float, 64> samples{};
    const Ngs2RenderBufferInfo output{samples.data(), sizeof(samples), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    for (unsigned grain = 0; grain < 3; ++grain) {
        samples.fill(-1234.0f);
        Require(sceNgs2SystemRender(setup.system, &output, 1) == SCE_NGS2_OK);
        if (grain == 2) for (const float sample : samples) Require(sample == 1000.0f / 32768.0f);
    }
    Require(manager.started && controlled && Flags(voice) == static_cast<std::uint32_t>(SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING));
    Require(sceNgs2SystemDestroy(setup.system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

int main(int argc, char** argv) {
    if (argc > 1) {
        const std::pair<const char*, void (*)()> scenarios[] = {{"validation", TestCaptureValidation}, {"drained", TestBufferedAppendAfterDrain},
            {"emptysetup", TestEmptySetupPort}, {"destroyed", TestPendingPatchDestroyedDest}, {"race", TestBlockQueueRace}};
        for (const auto& [name, run] : scenarios) {
            if (std::strcmp(argv[1], name) != 0) continue;
            run();
            std::printf("NGS2 worker buffering: %s scenario passed\n", name);
            return 0;
        }
        std::fprintf(stderr, "unknown scenario %s\n", argv[1]);
        return 2;
    }
    unsigned cases = 0;
    for (bool custom : {false, true}) for (unsigned order = 0; order < 3; ++order)
        for (auto update : {Update::Play, Update::Start, Update::Pause, Update::Kill, Update::Volume, Update::Matrix, Update::Combined, Update::Pitch, Update::Gain}) {
            TestBuffer(update, custom, false, order, 64);
            ++cases;
            if (update <= Update::Combined) { TestBuffer(update, custom, true, order, 64); ++cases; }
        }
    for (auto update : {Update::PlayStop, Update::StopPlay, Update::PauseResume, Update::KillPlay, Update::PlayKill, Update::PlayPause, Update::StopResume, Update::LongPause, Update::Mute, Update::SelectMatrix}) {
        for (unsigned order = 0; order < 3; ++order) {
            TestBuffer(update, false, true, order, 64);
            TestBuffer(update, true, false, order, 64);
            cases += 2;
        }
    }
    TestBuffer(Update::Combined, true, true, 2, 256);
    for (bool custom : {false, true}) for (bool commands : {false, true}) for (unsigned order = 0; order < 3; ++order) {
        TestBuffer(Update::Over, custom, commands, order, 64);
        cases++;
    }
    std::vector<float> worker, external, unchanged;
    TestBuffer(Update::Shift, true, false, 2, 256, &worker);
    TestBuffer(Update::ExternalShift, true, false, 2, 256, &external);
    TestBuffer(Update::UnchangedShift, true, false, 2, 256, &unchanged);
    Require(worker.size() == external.size() && worker.size() == unchanged.size());
    bool changed = false;
    for (std::size_t i = 0; i < worker.size(); ++i) {
        Require(std::fabs(worker[i] - external[i]) <= 1.0e-7f);
        if (i < 11 * 256) Require(worker[i] == unchanged[i]);
        else if (std::fabs(worker[i] - unchanged[i]) > 1.0e-5f) changed = true;
    }
    Require(changed);
    cases += 3;
    for (auto order : {SetupOrder::SetupOnly, SetupOrder::PlaySetup, SetupOrder::SetupPlay}) {
        TestBufferedSetup(order);
        ++cases;
    }
    TestBufferedSamplerSetup();
    ++cases;
    TestCaptureValidation();
    TestBufferedWaveformCodes();
    TestBufferedAppendAfterDrain();
    TestEmptySetupPort();
    TestPendingPatchDestroyedDest();
    TestBlockQueueRace();
    cases += 6;
    std::printf("NGS2 worker buffering: %u scenarios passed\n", cases + 1);
    return 0;
}

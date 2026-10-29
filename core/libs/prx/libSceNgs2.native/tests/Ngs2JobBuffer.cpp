#include <algorithm>
#include "Ngs2JobTest.hpp"

#include <array>
#include <cmath>
#include <cstring>

enum class Update { Play, Start, Pause, Kill, Volume, Matrix, Combined, Pitch, Gain, PlayStop, StopPlay, PauseResume, KillPlay, PlayKill, PlayPause, StopResume, LongPause, Mute, SelectMatrix, Shift, ExternalShift, UnchangedShift };
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

int main() {
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
    std::printf("NGS2 worker buffering: %u scenarios passed\n", cases + 1);
    return 0;
}

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

#ifdef APS5_NGS2_PS4_LIBRARY
static constexpr bool PS4_LIBRARY = true;
#else
static constexpr bool PS4_LIBRARY = false;
#endif


static void ReadPcm(const Ngs2Voice& voice, const std::uint8_t* frame, float* out) {
    Ngs2ReadPcmFrame(voice.waveformType, voice.channels, frame, out);
}

void Ngs2BindSamplerBlock(Ngs2Voice& voice, bool restartDecoder) {
    if (voice.blocks.empty()) return;
    auto& block = voice.blocks.front();
    if (block.started || block.info.num_samples == 0) return;
    voice.waveformUserData = block.info.user_data;
    if (voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9) {
        voice.atrac9.remainingSamples = block.info.num_samples;
        voice.atrac9.skipSamples = block.info.num_skip_samples;
        if (restartDecoder) voice.atrac9.restartOnDrain = true;
    } else {
        voice.pcmRemaining = block.info.num_samples + block.info.num_skip_samples;
    }
    block.started = true;
}

bool Ngs2FinishBlock(Ngs2Voice& voice) {
    auto& block = voice.blocks.front();
    if (block.data != nullptr) {
        voice.decodedBytes += block.info.data_size > block.dataCursor ? block.info.data_size - block.dataCursor : 0;
    }
    const bool repeat = block.info.num_repeats > 1;
    if (repeat) {
        if (block.info.num_repeats != UINT32_MAX) block.info.num_repeats--;
        block.numRepeated++;
        block.started = false;
        block.dataCursor = 0;
        if (voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9) {
            voice.atrac9.restartOnDrain = true;
        }
    }
    const Ngs2VoiceCallbackInfo info{
        0,
        reinterpret_cast<Ngs2Handle>(&voice),
        repeat ? SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT : SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END,
        0,
        block.info.user_data,
        block.data,
        static_cast<std::uint32_t>(block.info.data_size),
        0,
        0,
        0,
    };
    if (!repeat) {
        voice.blocks.pop_front();
        Ngs2PublishBlockCount(voice);
        Ngs2BindSamplerBlock(voice, true);
    }
    const auto revision = voice.waveformRevision;
    voice.pendingBlockCallbacks.push_back({info, voice.rack->system->callbackSequence.fetch_add(1, std::memory_order_relaxed)});
    return voice.waveformRevision == revision && (voice.state == Ngs2PlayState::Playing || voice.state == Ngs2PlayState::Paused || voice.outputTransition == Ngs2VoiceTransition::Stop);
}

std::uint32_t Ngs2ReadSamplerPcm(Ngs2Voice& voice, float* dest, std::uint32_t stride, std::uint32_t count) {
    std::uint32_t written = 0;
    const auto frameBytes = voice.channels * Ngs2PcmSampleBytes(voice.waveformType);
    while (written < count && !voice.blocks.empty()) {
        auto& block = voice.blocks.front();
        if (!block.started) {
            Ngs2BindSamplerBlock(voice, false);
            if (block.info.num_samples == 0) block.started = true;
        }
        if (voice.pcmRemaining == 0) {
            Ngs2FinishBlock(voice);
            continue;
        }
        if (block.data != nullptr && block.dataCursor >= block.info.data_size) {
            if (voice.blocks.size() == 1 && voice.acceptsBlocks) break;
            Ngs2FinishBlock(voice);
            continue;
        }
        if (block.data != nullptr && block.info.data_size - block.dataCursor < frameBytes) throw std::runtime_error("NGS2: a partial PCM frame is not implemented");
        float frame[NGS2_MAX_CHANNELS];
        ReadPcm(voice, block.data == nullptr ? nullptr : block.data + block.dataCursor, frame);
        for (std::uint32_t channel = 0; channel < voice.channels; ++channel) dest[static_cast<std::size_t>(channel) * stride + written] = frame[channel];
        ++written;
        const auto consumed = block.info.num_skip_samples + block.info.num_samples - voice.pcmRemaining;
        --voice.pcmRemaining;
        if (consumed >= block.info.num_skip_samples) ++voice.decodedSamples;
        voice.decodedBytes += frameBytes;
        block.dataCursor += frameBytes;
        if (voice.pcmRemaining != 0 && (written == count || block.dataCursor == block.info.data_size)) {
            voice.waveformData = block.data == nullptr ? nullptr : block.data + block.dataCursor;
        }
        if (voice.pcmRemaining == 0) Ngs2FinishBlock(voice);
    }
    return written;
}

static bool Bypasses(const Ngs2Filter& filter, std::uint32_t channel) {
    return channel < 64 && (filter.bypassMask & (std::uint64_t{1} << channel)) != 0;
}

static bool HasHistory(const Ngs2Filter& filter) {
    if (!filter.enabled) return false;
    for (std::uint32_t channel = 0; channel < filter.history.size(); channel++) {
        const auto& h = filter.history[channel];
        if (!Bypasses(filter, channel) && (h.x1 != 0.0 || h.x2 != 0.0 || h.y1 != 0.0 || h.y2 != 0.0)) return true;
    }
    return false;
}

static void ApplyFilter(Ngs2Voice& voice, Ngs2Filter& filter, std::uint32_t grain) {
    for (std::uint32_t channel = 0; channel < voice.channels; channel++) {
        if (Bypasses(filter, channel)) continue;
        auto& h = filter.history[channel];
        for (std::uint32_t i = 0; i < grain; i++) {
            auto& sample = voice.samples[channel * grain + i];
            const double x = sample;
            double y = filter.b0 * x + filter.b1 * h.x1 + filter.b2 * h.x2 - filter.a1 * h.y1 - filter.a2 * h.y2;
            if (std::abs(y) < 1e-20) y = 0.0;
            h = {x, h.x1, y, h.y1};
            sample = static_cast<float>(y);
        }
    }
}

static void RenderSampler(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t systemRate) {
    const auto channels = voice.channels;
    Ngs2ConsumeSampler(voice, grain, systemRate);
    if (voice.channels != channels) {
        voice.samples.assign(static_cast<std::size_t>(grain) * voice.channels, 0.0f);
        voice.hasSamples = false;
        return;
    }
    for (auto& filter : voice.filters) {
        if (filter.pending) {
            const auto parameter = filter.parameter;
            Ngs2SetFilter(voice, parameter);
        }
        if (!filter.enabled || (!voice.hasSamples && !HasHistory(filter))) continue;
        ApplyFilter(voice, filter, grain);
        voice.hasSamples = true;
    }
    const bool buffered = voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9 && voice.atrac9.windowCursor * voice.channels < voice.atrac9.window.size();
    if ((voice.state == Ngs2PlayState::Playing || voice.state == Ngs2PlayState::Paused) && voice.samplerEnded && !buffered && std::none_of(voice.filters.begin(), voice.filters.end(), HasHistory)) {
        voice.state = Ngs2PlayState::Empty;
    }
}

static void RenderVoice(Ngs2Voice& voice, const std::vector<Ngs2Voice*>& voices, std::uint32_t grain, std::uint32_t systemRate);

struct Ngs2DefaultRow {
    float mono;
    float stereo[2];
    float surround51[6];
    float surround71[8];
};

static constexpr float HALF_POWER = std::numbers::sqrt2_v<float> / 2.0f;
static constexpr float QUARTER_POWER = std::numbers::sqrt2_v<float> / 4.0f;

static constexpr Ngs2DefaultRow MONO_CENTER{1.0f, {HALF_POWER, HALF_POWER}, {0, 0, 1, 0, 0, 0}, {0, 0, 1, 0, 0, 0, 0, 0}};
static constexpr Ngs2DefaultRow FRONT_LEFT{HALF_POWER, {1, 0}, {1, 0, 0, 0, 0, 0}, {1, 0, 0, 0, 0, 0, 0, 0}};
static constexpr Ngs2DefaultRow FRONT_RIGHT{HALF_POWER, {0, 1}, {0, 1, 0, 0, 0, 0}, {0, 1, 0, 0, 0, 0, 0, 0}};
static constexpr Ngs2DefaultRow LFE{0.0f, {0, 0}, {0, 0, 0, 1, 0, 0}, {0, 0, 0, 1, 0, 0, 0, 0}};
static constexpr Ngs2DefaultRow QUAD_LEFT{0.5f, {HALF_POWER, 0}, {0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 1, 0, 0, 0}};
static constexpr Ngs2DefaultRow QUAD_RIGHT{0.5f, {0, HALF_POWER}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1, 0, 0}};
static constexpr Ngs2DefaultRow FIVE_SURROUND_LEFT{0.5f, {0, 0}, {0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 1, 0, 0, 0}};
static constexpr Ngs2DefaultRow FIVE_SURROUND_RIGHT{0.5f, {0, 0}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1, 0, 0}};
static constexpr Ngs2DefaultRow SURROUND_LEFT{0.5f, {0.5f, 0}, {0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 1, 0, 0, 0}};
static constexpr Ngs2DefaultRow SURROUND_RIGHT{0.5f, {0, 0.5f}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1, 0, 0}};
static constexpr Ngs2DefaultRow BACK_CENTER{QUARTER_POWER, {QUARTER_POWER, QUARTER_POWER}, {0, 0, 0, 0, HALF_POWER, HALF_POWER}, {0, 0, 0, 0, 0, 0, HALF_POWER, HALF_POWER}};
static constexpr Ngs2DefaultRow BACK_LEFT{0.5f, {0.5f, 0}, {0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 1, 0}};
static constexpr Ngs2DefaultRow BACK_RIGHT{0.5f, {0, 0.5f}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 0, 0, 1}};
static constexpr Ngs2DefaultRow SURROUND_LEFT_51{0.5f, {HALF_POWER, 0}, {0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 1, 0, 0, 0}};
static constexpr Ngs2DefaultRow SURROUND_RIGHT_51{0.5f, {0, HALF_POWER}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1, 0, 0}};
static constexpr Ngs2DefaultRow SURROUND_LEFT_71{HALF_POWER, {HALF_POWER, 0}, {0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 1, 0, 0, 0}};
static constexpr Ngs2DefaultRow SURROUND_RIGHT_71{HALF_POWER, {0, HALF_POWER}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 1, 0, 0}};
static constexpr Ngs2DefaultRow BACK_LEFT_71{HALF_POWER, {HALF_POWER, 0}, {0, 0, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 0, 1, 0}};
static constexpr Ngs2DefaultRow BACK_RIGHT_71{HALF_POWER, {0, HALF_POWER}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 0, 0, 0, 0, 1}};

static constexpr Ngs2DefaultRow DEFAULT_MAP[NGS2_MAX_CHANNELS][NGS2_MAX_CHANNELS] = {
    {MONO_CENTER},
    {FRONT_LEFT, FRONT_RIGHT},
    {FRONT_LEFT, FRONT_RIGHT, LFE},
    {FRONT_LEFT, FRONT_RIGHT, QUAD_LEFT, QUAD_RIGHT},
    {FRONT_LEFT, FRONT_RIGHT, MONO_CENTER, FIVE_SURROUND_LEFT, FIVE_SURROUND_RIGHT},
    {FRONT_LEFT, FRONT_RIGHT, MONO_CENTER, LFE, PS4_LIBRARY ? SURROUND_LEFT : SURROUND_LEFT_51, PS4_LIBRARY ? SURROUND_RIGHT : SURROUND_RIGHT_51},
    {FRONT_LEFT, FRONT_RIGHT, MONO_CENTER, LFE, SURROUND_LEFT, SURROUND_RIGHT, BACK_CENTER},
    {FRONT_LEFT, FRONT_RIGHT, MONO_CENTER, LFE, PS4_LIBRARY ? SURROUND_LEFT : SURROUND_LEFT_71, PS4_LIBRARY ? SURROUND_RIGHT : SURROUND_RIGHT_71, PS4_LIBRARY ? BACK_LEFT : BACK_LEFT_71, PS4_LIBRARY ? BACK_RIGHT : BACK_RIGHT_71},
};

static constexpr Ngs2DefaultRow FIXED_DOWNMIX_51[6] = {FRONT_LEFT, FRONT_RIGHT, MONO_CENTER, LFE, SURROUND_LEFT, SURROUND_RIGHT};
static constexpr Ngs2DefaultRow FIXED_DOWNMIX_71[8] = {FRONT_LEFT, FRONT_RIGHT, MONO_CENTER, LFE, SURROUND_LEFT, SURROUND_RIGHT, BACK_LEFT, BACK_RIGHT};

static float RowLevel(const Ngs2DefaultRow& row, std::uint32_t destChannels, std::uint32_t dest) {
    switch (destChannels) {
        case 1: return row.mono;
        case 2: return row.stereo[dest];
        case 6: return row.surround51[dest];
        case 8: return row.surround71[dest];
        default: throw std::runtime_error("NGS2: the default channel map into " + std::to_string(destChannels) + " channels is not implemented");
    }
}

static void RequireMappedChannels(std::uint32_t sourceChannels, std::uint32_t destChannels) {
    if (sourceChannels > NGS2_MAX_CHANNELS || destChannels > NGS2_MAX_CHANNELS) {
        throw std::runtime_error("NGS2: the default channel map from " + std::to_string(sourceChannels) +
                                 " into " + std::to_string(destChannels) + " channels is not implemented");
    }
}

float Ngs2DefaultLevel(std::uint32_t sourceChannels, std::uint32_t source, std::uint32_t destChannels, std::uint32_t dest) {
    RequireMappedChannels(sourceChannels, destChannels);
    return RowLevel(DEFAULT_MAP[sourceChannels - 1][source], destChannels, dest);
}

float Ngs2FixedDownmixLevel(std::uint32_t sourceChannels, std::uint32_t source, std::uint32_t destChannels, std::uint32_t dest) {
    RequireMappedChannels(sourceChannels, destChannels);
    const auto* rows = sourceChannels == 6 ? FIXED_DOWNMIX_51 : sourceChannels == 8 ? FIXED_DOWNMIX_71 : DEFAULT_MAP[sourceChannels - 1];
    return RowLevel(rows[source], destChannels, dest);
}

void Ngs2PrepareControlMix(Ngs2Voice& voice) {
    for (auto& port : voice.ports) port.rampFrom.clear();
    const bool changed = std::any_of(voice.pendingControls.begin(), voice.pendingControls.end(), [](const auto& control) {
        return std::holds_alternative<Ngs2VoicePortVolumeParam>(control) || std::holds_alternative<Ngs2VoicePortMatrixParam>(control) || std::holds_alternative<Ngs2MatrixControl>(control);
    });
    if (!changed) return;
    for (auto& port : voice.ports) {
        if (port.dest == nullptr || voice.channels == 0) continue;
        const auto outputs = port.dest->InputChannels();
        const auto* matrix = port.matrix < 0 || voice.matrices[port.matrix].empty() ? nullptr : &voice.matrices[port.matrix];
        port.rampFrom.assign(static_cast<std::size_t>(outputs) * voice.channels, 0.0f);
        for (std::uint32_t dst = 0; dst < outputs; ++dst) for (std::uint32_t src = 0; src < voice.channels; ++src) {
            const auto index = dst * voice.channels + src;
            if (matrix == nullptr || dst < matrix->size() / voice.channels) port.rampFrom[index] = port.volume *
                (matrix == nullptr ? Ngs2DefaultLevel(voice.channels, src, outputs, dst) : (*matrix)[index]);
        }
    }
}

static void MixPort(Ngs2Voice& voice, const Ngs2Voice& source, const Ngs2Port& port, std::uint32_t sourceBuffer, std::uint32_t grain) {
    const auto& sourceSamples = source.Samples(sourceBuffer);
    auto& destSamples = voice.Samples(port.destInput);
    if (sourceSamples.size() != static_cast<std::size_t>(grain) * source.channels ||
        destSamples.size() != static_cast<std::size_t>(grain) * voice.InputChannels()) {
        throw std::runtime_error("NGS2: channel layout changes during input rendering are not implemented");
    }
    const auto* matrix = port.matrix < 0 || source.matrices[port.matrix].empty() ? nullptr : &source.matrices[port.matrix];
    const auto channels = voice.InputChannels();
    const std::size_t outputs = port.rampFrom.empty() ? (matrix == nullptr ? channels : std::min<std::size_t>(channels, matrix->size() / source.channels)) : channels;
    for (std::size_t dst = 0; dst < outputs; dst++) {
        for (std::uint32_t src = 0; src < source.channels; src++) {
            const auto index = dst * source.channels + src;
            const float level = port.volume * (matrix == nullptr ? Ngs2DefaultLevel(source.channels, src, channels, static_cast<std::uint32_t>(dst)) : dst < matrix->size() / source.channels ? (*matrix)[index] : 0.0f);
            const float from = port.rampFrom.empty() ? level : port.rampFrom[index];
            if (level == 0.0f && from == 0.0f) continue;
            if (from == level) {
                for (std::uint32_t i = 0; i < grain; i++) destSamples[dst * grain + i] += sourceSamples[src * grain + i] * level;
            } else {
                const float step = (level - from) / static_cast<float>(grain);
                for (std::uint32_t i = 0; i < grain; i++) destSamples[dst * grain + i] += sourceSamples[src * grain + i] * (from + step * static_cast<float>(i));
            }
        }
    }
}

static void MixInputs(Ngs2Voice& voice, const std::vector<Ngs2Voice*>& voices, std::uint32_t grain, std::uint32_t systemRate) {
    for (auto* source : voices) {
        for (std::size_t p = 0; p < source->ports.size(); p++) {
            const auto& port = source->ports[p];
            if (port.dest != &voice || (port.volume == 0.0f && port.rampFrom.empty())) continue;
            const auto destInput = port.destInput;
            RenderVoice(*source, voices, grain, systemRate);
            if (port.dest != &voice || port.destInput != destInput) {
                throw std::runtime_error("NGS2: patch changes during source rendering are not implemented");
            }
            if (!source->hasSamples || source->KilledForRender()) continue;
            const auto sourceBuffer = source->rack->customPortBuffers.empty() ? 0u : source->rack->customPortBuffers[p];
            MixPort(voice, *source, port, sourceBuffer, grain);
            voice.hasSamples = true;
        }
    }
}

void Ngs2UpdatePeakMeter(Ngs2Voice& voice, std::uint32_t grain) {
    if (voice.peakMeterMode == 0 || voice.channels == 0) {
        voice.peakHeight = 0.0f;
        return;
    }
    const auto blocks = std::clamp(voice.rack->peakMeterBlocks, 1u, 256u);
    if (voice.peakMeterHistory.size() != voice.channels ||
        (!voice.peakMeterHistory.empty() && voice.peakMeterHistory.front().size() != blocks)) {
        voice.peakMeterHistory.assign(voice.channels, std::vector<float>(blocks, 0.0f));
        voice.peakMeterCount = 0;
        voice.peakMeterCursor = 0;
    }
    for (std::uint32_t channel = 0; channel < voice.channels; channel++) {
        float value = 0.0f;
        if (voice.peakMeterMode == 1) {
            for (std::uint32_t i = 0; i < grain; i++) value = std::max(value, std::abs(voice.samples[channel * grain + i]));
        } else {
            for (std::uint32_t i = 0; i < grain; i++) {
                const float sample = voice.samples[channel * grain + i];
                value += sample * sample;
            }
        }
        voice.peakMeterHistory[channel][voice.peakMeterCursor] = value;
    }
    voice.peakMeterCursor = (voice.peakMeterCursor + 1) % blocks;
    voice.peakMeterCount = std::min<std::uint32_t>(voice.peakMeterCount + 1, blocks);
    float height = 0.0f;
    for (std::uint32_t channel = 0; channel < voice.channels; channel++) {
        float channelPeak = 0.0f;
        if (voice.peakMeterMode == 1) {
            for (std::uint32_t block = 0; block < voice.peakMeterCount; block++) {
                channelPeak = std::max(channelPeak, voice.peakMeterHistory[channel][block]);
            }
        } else {
            for (std::uint32_t block = 0; block < voice.peakMeterCount; block++) channelPeak += voice.peakMeterHistory[channel][block];
            channelPeak = std::sqrt(channelPeak / static_cast<float>(voice.peakMeterCount * grain));
        }
        height = std::max(height, channelPeak);
    }
    voice.peakHeight = height;
}

static void RenderVoice(Ngs2Voice& voice, const std::vector<Ngs2Voice*>& voices, std::uint32_t grain, std::uint32_t systemRate) {
    if (voice.rendered) return;
    if (voice.rendering) throw std::invalid_argument("NGS2: the voice patches form a cycle");
    voice.rendering = true;
    struct RenderingScope {
        bool& active;
        ~RenderingScope() { active = false; }
    } renderingScope{voice.rendering};
    voice.samples.assign(static_cast<std::size_t>(grain) * voice.InputChannels(), 0.0f);
    for (auto& buffer : voice.customBuffers) buffer.assign(static_cast<std::size_t>(grain) * voice.InputChannels(), 0.0f);
    voice.hasSamples = false;
    const bool pitchShiftActive = (Ngs2HasPitchShift(voice) || voice.IsSampler() || voice.IsMixer()) &&
        (voice.state == Ngs2PlayState::Paused || voice.outputTransition == Ngs2VoiceTransition::Stop || voice.outputTransition == Ngs2VoiceTransition::Kill);
    const bool frozenSampler = voice.IsSampler() && voice.state == Ngs2PlayState::Paused &&
        (voice.stateFlags & (SCE_NGS2_VOICE_STATE_FLAG_PAUSED | SCE_NGS2_VOICE_STATE_FLAG_PLAYING)) ==
        (SCE_NGS2_VOICE_STATE_FLAG_PAUSED | SCE_NGS2_VOICE_STATE_FLAG_PLAYING);
    if ((voice.state == Ngs2PlayState::Playing || pitchShiftActive) && voice.channels != 0) {
        if (voice.rack->rackId == SCE_NGS2_RACK_ID_SAMPLER || voice.rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER) {
            if (!frozenSampler && voice.outputTransition != Ngs2VoiceTransition::Kill) RenderSampler(voice, grain, systemRate);
        } else {
            MixInputs(voice, voices, grain, systemRate);
        }
        if (voice.rack->rackId == SCE_NGS2_RACK_ID_REVERB && voice.reverb) voice.hasSamples = Ngs2ProcessReverb(voice, grain, systemRate);
        Ngs2ProcessLegacyUserFx(voice, grain, systemRate);
        Ngs2ProcessUserFx(voice, grain, systemRate);
        Ngs2ApplyVoiceGain(voice, grain);
        Ngs2UpdatePeakMeter(voice, grain);
    }
    if (voice.samples.size() != static_cast<std::size_t>(grain) * voice.channels) voice.samples.assign(static_cast<std::size_t>(grain) * voice.channels, 0.0f);
    voice.rendered = true;
}

struct Ngs2RenderJob {
    Ngs2Voice* voice;
    const std::vector<Ngs2Voice*>* voices;
    std::uint32_t grain;
    std::uint32_t sampleRate;
    std::exception_ptr error;
};

static void APS5_VABI RunRenderJob(void* context, std::uint32_t index) {
    auto& job = static_cast<Ngs2RenderJob*>(context)[index];
    const bool previous = Ngs2SetJobWorker(true);
    struct WorkerScope {
        bool previous;
        ~WorkerScope() { Ngs2SetJobWorker(previous); }
    } workerScope{previous};
    try { RenderVoice(*job.voice, *job.voices, job.grain, job.sampleRate); }
    catch (...) {
        job.error = std::current_exception();
        std::fill(job.voice->samples.begin(), job.voice->samples.end(), 0.0f);
        job.voice->hasSamples = false;
        job.voice->rendered = true;
    }
}

static bool ScheduledVoice(const Ngs2Voice& voice) {
    return voice.channels != 0 && (voice.state == Ngs2PlayState::Playing ||
        ((voice.IsSampler() || voice.IsMixer() || Ngs2HasPitchShift(voice)) && (voice.state == Ngs2PlayState::Paused || voice.outputTransition == Ngs2VoiceTransition::Stop || voice.outputTransition == Ngs2VoiceTransition::Kill)));
}

static void RenderScheduled(Ngs2System& system, const std::vector<Ngs2Voice*>& voices, std::uint32_t grain) {
    auto& manager = system.jobManagers[0];
    const auto& option = manager.option;
    if (option.submit_handler == nullptr || option.flush_handler == nullptr || option.join_handler == nullptr) {
        throw std::invalid_argument("NGS2: an active render job manager requires submit, flush and join handlers");
    }
    std::vector<std::uint32_t> depth(voices.size());
    std::vector<std::uint8_t> visiting(voices.size());
    const auto rank = [&](auto&& self, std::size_t i) -> std::uint32_t {
        if (depth[i] != 0) return depth[i];
        if (visiting[i] != 0) throw std::invalid_argument("NGS2: the voice patches form a cycle");
        visiting[i] = 1;
        std::uint32_t value = 1;
        if (!voices[i]->IsSampler()) {
            for (std::size_t j = 0; j < voices.size(); ++j) {
                if (!ScheduledVoice(*voices[j])) continue;
                for (const auto& port : voices[j]->ports) {
                    if (port.dest == voices[i] && (port.volume != 0.0f || !port.rampFrom.empty())) value = std::max(value, self(self, j) + 1);
                }
            }
        }
        visiting[i] = 0;
        return depth[i] = value;
    };
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < voices.size(); ++i) {
        if (ScheduledVoice(*voices[i])) { rank(rank, i); order.push_back(i); }
        else RenderVoice(*voices[i], voices, grain, system.option.sample_rate);
    }
    const auto rackIndex = [&](const Ngs2Voice* voice) { return std::find(system.racks.begin(), system.racks.end(), voice->rack) - system.racks.begin(); };
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (depth[a] != depth[b]) return depth[a] < depth[b];
        return rackIndex(voices[a]) > rackIndex(voices[b]);
    });
    std::vector<Ngs2RenderJob> jobs;
    jobs.reserve(order.size());
    for (auto i : order) jobs.push_back({voices[i], &voices, grain, system.option.sample_rate, {}});
    system.jobsExecuting = true;
    struct JobScope {
        Ngs2System& system;
        ~JobScope() { system.jobsExecuting = false; }
    } jobScope{system};
    bool submitted = false;
    struct Completion {
        const Ngs2JobManager& manager;
        bool& submitted;
        ~Completion() {
            if (submitted) {
                const Ngs2Unlocked unlocked;
                manager.option.join_handler(manager.handle);
            }
        }
    } completion{manager, submitted};
    std::size_t start = 0;
    while (start < jobs.size()) {
        auto end = start + 1;
        while (end < jobs.size() && depth[order[end]] == depth[order[start]]) ++end;
        for (auto batch = start; batch < end;) {
            const auto count = static_cast<std::uint32_t>(std::min<std::size_t>(end - batch, option.max_jobs));
            submitted = true;
            const Ngs2Unlocked unlocked;
            option.submit_handler(manager.handle, RunRenderJob, jobs.data() + batch, count);
            batch += count;
        }
        {
            const Ngs2Unlocked unlocked;
            if (end == jobs.size()) { option.join_handler(manager.handle); submitted = false; }
            else option.flush_handler(manager.handle);
        }
        start = end;
    }
    for (const auto& job : jobs) if (job.error) std::rethrow_exception(job.error);
    if (jobs.empty()) {
        const Ngs2Unlocked unlocked;
        option.join_handler(manager.handle);
    }
}

static std::size_t SampleBytes(std::uint32_t waveformType) {
    if (waveformType == SCE_NGS2_WAVEFORM_TYPE_PCM_I16L) return sizeof(std::int16_t);
    if (waveformType == SCE_NGS2_WAVEFORM_TYPE_PCM_F32L) return sizeof(float);
    throw std::runtime_error("NGS2: render waveform type " + Ngs2Hex(waveformType) + " is not implemented");
}

static bool HasMasteringMap(std::uint32_t channels, std::uint32_t outputChannels) {
    if (PS4_LIBRARY) return channels == 2 && (outputChannels == 6 || outputChannels == 8);
    const auto mapped = [](std::uint32_t count) { return count == 1 || count == 2 || count == 6 || count == 8; };
    return mapped(channels) && mapped(outputChannels);
}

static float MasteringOutputLevel(std::uint32_t channels, std::uint32_t channel, std::uint32_t outputChannels, std::uint32_t dest) {
    if (channels == outputChannels) return channel == dest ? 1.0f : 0.0f;
    if (channels == 1) return dest == 0 ? 1.0f : 0.0f;
    return Ngs2DefaultLevel(channels, channel, outputChannels, dest);
}

static std::vector<float>& OutputMix(std::vector<std::vector<float>>& mixes, const Ngs2RenderBufferInfo* bufferInfo, std::uint32_t numBufferInfo,
                                     const Ngs2Voice& voice, std::uint32_t grain) {
    if (voice.outputId >= numBufferInfo) throw std::invalid_argument("NGS2: mastering output " + std::to_string(voice.outputId) + " has no render buffer");
    const auto& output = bufferInfo[voice.outputId];
    const auto sampleBytes = SampleBytes(output.waveform_type);
    if (output.num_channels != voice.channels && !HasMasteringMap(voice.channels, output.num_channels)) throw std::runtime_error("NGS2: mixing " + std::to_string(voice.channels) + " mastering channels into " + std::to_string(output.num_channels) + " is not implemented");
    if (output.buffer == nullptr || output.buffer_size < static_cast<std::size_t>(grain) * output.num_channels * sampleBytes) {
        throw std::invalid_argument("NGS2: render buffer " + std::to_string(voice.outputId) + " is missing or too small");
    }
    auto& mix = mixes[voice.outputId];
    if (mix.empty()) mix.resize(static_cast<std::size_t>(grain) * output.num_channels);
    return mix;
}

static void WriteOutput(const Ngs2RenderBufferInfo& output, const std::vector<float>& mix) {
    if (output.waveform_type == SCE_NGS2_WAVEFORM_TYPE_PCM_F32L) {
        std::memcpy(output.buffer, mix.data(), mix.size() * sizeof(float));
        return;
    }
    auto* pcm = static_cast<std::int16_t*>(output.buffer);
    for (std::size_t i = 0; i < mix.size(); i++) {
        const float scaled = mix[i] * 32767.0f;
        pcm[i] = std::isnan(scaled) ? 0 : static_cast<std::int16_t>(std::clamp(scaled, -32767.0f, 32767.0f));
    }
}

static std::vector<std::size_t> OutputBytes(const Ngs2System& system, const Ngs2RenderBufferInfo* bufferInfo, std::uint32_t numBufferInfo) {
    const auto grain = system.option.num_grain_samples;
    std::vector<std::size_t> outputBytes(numBufferInfo);
    for (std::uint32_t i = 0; i < numBufferInfo; i++) {
        const auto& output = bufferInfo[i];
        if (output.buffer == nullptr) continue;
        if (output.num_channels == 0 || output.num_channels > NGS2_MAX_CHANNELS) {
            throw std::invalid_argument("NGS2: render channel count is invalid");
        }
        outputBytes[i] = static_cast<std::size_t>(grain) * output.num_channels * SampleBytes(output.waveform_type);
        if (output.buffer_size < outputBytes[i]) throw std::invalid_argument("NGS2: render buffer " + std::to_string(i) + " is too small");
    }
    return outputBytes;
}

void Ngs2RenderSilence(const Ngs2System& system, const Ngs2RenderBufferInfo* bufferInfo, std::uint32_t numBufferInfo) {
    const auto outputBytes = OutputBytes(system, bufferInfo, numBufferInfo);
    for (std::uint32_t i = 0; i < numBufferInfo; i++) {
        if (outputBytes[i] != 0) std::memset(bufferInfo[i].buffer, 0, outputBytes[i]);
    }
}

void Ngs2RenderSystem(Ngs2System& system, const Ngs2RenderBufferInfo* bufferInfo, std::uint32_t numBufferInfo) {
    const auto grain = system.option.num_grain_samples;
    const auto outputBytes = OutputBytes(system, bufferInfo, numBufferInfo);
    std::vector<Ngs2Voice*> voices;
    for (auto* rack : system.racks) {
        for (auto& voice : rack->voices) {
            voice.ApplyPendingControls();
            voice.decodePendingAtGrain = Ngs2DecodePending(voice);
            voice.rendered = false;
            Ngs2PublishSamplerState(voice);
            voices.push_back(&voice);
        }
    }
    {
        struct ProcessingScope {
            Ngs2System& system;
            bool previous;
            explicit ProcessingScope(Ngs2System& value) : system(value), previous(value.processingVoices) { system.processingVoices = true; }
            ~ProcessingScope() { system.processingVoices = previous; }
        } processing(system);
        if (system.jobManagers[0].handle != 0) RenderScheduled(system, voices, grain);
        std::vector<std::vector<float>> mixes(numBufferInfo);
        for (auto* voice : voices) {
            RenderVoice(*voice, voices, grain, system.option.sample_rate);
            if ((voice->rack->rackId != SCE_NGS2_RACK_ID_MASTERING && voice->rack->rackId != SCE_NGS2_RACK_ID_CUSTOM_MASTERING) || !voice->hasSamples || voice->KilledForRender()) continue;
            auto& mix = OutputMix(mixes, bufferInfo, numBufferInfo, *voice, grain);
            const std::uint32_t outputChannels = bufferInfo[voice->outputId].num_channels;
            for (std::uint32_t channel = 0; channel < voice->channels; channel++) {
                const bool lfe = channel == 3 && (voice->channels == 6 || voice->channels == 8);
                const float level = lfe ? voice->lfeLevel : voice->fbwLevel;
                for (std::uint32_t dest = 0; dest < outputChannels; dest++) {
                    const float map = MasteringOutputLevel(voice->channels, channel, outputChannels, dest);
                    if (map == 0.0f) continue;
                    for (std::uint32_t i = 0; i < grain; i++) mix[i * outputChannels + dest] += voice->samples[channel * grain + i] * level * map;
                }
            }
        }
        for (std::uint32_t i = 0; i < numBufferInfo; i++) {
            if (!mixes[i].empty()) WriteOutput(bufferInfo[i], mixes[i]);
            else if (outputBytes[i] != 0) std::memset(bufferInfo[i].buffer, 0, outputBytes[i]);
        }
    }
    for (auto* rack : system.racks) {
        for (auto& value : rack->voices) {
            auto* voice = &value;
            if (voice->state == Ngs2PlayState::Stopped) voice->state = Ngs2PlayState::Empty;
            const bool wasPaused = (voice->stateFlags & SCE_NGS2_VOICE_STATE_FLAG_PAUSED) != 0;
            voice->stateFlags = static_cast<std::uint32_t>(voice->state);
            if (wasPaused && voice->state == Ngs2PlayState::Paused && voice->IsSampler()) voice->stateFlags |= SCE_NGS2_VOICE_STATE_FLAG_PLAYING;
            if (voice->state == Ngs2PlayState::Playing && voice->IsSampler() && voice->samplerStarved && voice->acceptsBlocks) voice->stateFlags |= SCE_NGS2_VOICE_STATE_FLAG_EMPTY;
            Ngs2PublishSamplerState(*voice);
        }
        std::vector<std::pair<Ngs2Voice*, Ngs2QueuedCallback>> events;
        for (auto& voice : rack->voices) {
            voice.dispatchBlockCallbacks.swap(voice.pendingBlockCallbacks);
            for (const auto& event : voice.dispatchBlockCallbacks) events.emplace_back(&voice, event);
        }
        struct CallbackScope {
            Ngs2Rack& rack;
            ~CallbackScope() { for (auto& voice : rack.voices) voice.dispatchBlockCallbacks.clear(); }
        } callbackScope{*rack};
        if (system.jobManagers[0].handle != 0) {
            std::stable_sort(events.begin(), events.end(), [](const auto& a, const auto& b) { return a.second.sequence < b.second.sequence; });
        }
        for (const auto& event : events) {
            auto* voice = event.first;
            auto info = event.second.info;
            if (voice->callback == nullptr || (voice->callbackFlags & info.flag) == 0) continue;
            info.callback_data = voice->callbackData;
            const Ngs2Unlocked unlocked;
            voice->callback(&info);
        }
    }
    for (auto* rack : system.racks) ++rack->renderCount;
    system.renderCount++;
}

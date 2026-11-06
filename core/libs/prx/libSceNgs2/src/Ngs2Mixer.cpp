#include "Ngs2Mixer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace ngs2mix {
namespace {

constexpr std::uint32_t WaveformPcmS16 = 0x12;
constexpr std::uint32_t StateInUse = 0x1;
constexpr std::uint32_t StatePlaying = 0x2;
constexpr std::uint32_t InfiniteRepeats = 0xFFFFFFFF;
constexpr std::uint32_t MaxBlocks = 256;
constexpr std::uint32_t MaxRenderChannels = 8;

// Generic and rack-specific parameter ids observed in ANIMAL WELL's wrappers (eboot 0x17xxx-0x18xxx).
constexpr std::uint32_t ParamPatch = 0x00000005;
constexpr std::uint32_t ParamSamplerSetup = 0x10000000;
constexpr std::uint32_t ParamSamplerBlocks = 0x10000001;
// One float sent with every play (1.0 at the intro): the playback rate ratio.
constexpr std::uint32_t ParamSamplerPitch = 0x10000005;
constexpr std::uint32_t ParamSubmixerSetup = 0x20000000;
constexpr std::uint32_t ParamMasteringSetup = 0x30000000;
constexpr std::uint32_t ParamMasteringOutput = 0x30000005;
constexpr std::uint32_t ParamReverbSetup = 0x20010000;
constexpr std::uint32_t ParamReverbI3dl2 = 0x20010001;

// Freeverb tunings at 44.1 kHz, scaled to the system rate; the right channel is offset.
constexpr std::uint32_t CombTuning[4] = {1116, 1188, 1277, 1356};
constexpr std::uint32_t AllpassTuning[2] = {556, 441};
constexpr std::uint32_t StereoSpread = 23;
constexpr float AllpassFeedback = 0.5f;

// I3DL2 block as the title sends it (eboot 0x17xxx; 88-byte payload): two leading floats,
// then Room, RoomHF, RoomRolloff, DecayTime, DecayHFRatio, Reflections, ReflectionsDelay,
// Reverb, ReverbDelay, Diffusion, Density, HFReference.
struct I3dl2 {
    float leading[2];
    std::int32_t room;
    std::int32_t roomHF;
    float roomRolloff;
    float decayTime;
    float decayHFRatio;
    std::int32_t reflections;
    float reflectionsDelay;
    std::int32_t reverb;
    float reverbDelay;
    float diffusion;
    float density;
    float hfReference;
};

void configureReverb(VoiceMix& voice, const std::uint32_t systemRate) {
    const float rate = static_cast<float>(systemRate);
    const float decay = std::clamp(voice.reverbDecayTime, 0.1f, 20.0f);
    // Millibels to linear: wet level combines the room and late reverb attenuation.
    voice.reverbWet = std::pow(10.0f, static_cast<float>(std::clamp(voice.reverbLevelMb, -10000, 0)) / 2000.0f);
    // A decay HF ratio below 1 makes highs die faster: more damping in the comb loops.
    voice.reverbDamp = std::clamp(0.2f + (1.0f - voice.reverbDecayHFRatio) * 0.6f, 0.0f, 0.8f);
    for (int channel = 0; channel < 2; ++channel) {
        for (int comb = 0; comb < 4; ++comb) {
            const auto length = static_cast<std::size_t>((CombTuning[comb] + channel * StereoSpread) * rate / 44100.0f);
            voice.combs[channel][comb].assign(std::max<std::size_t>(length, 1), 0.0f);
            voice.combPos[channel][comb] = 0;
            voice.combFilter[channel][comb] = 0.0f;
            // RT60: the loop gain that loses 60 dB over the decay time.
            if (channel == 0) voice.reverbFeedback[comb] = std::pow(10.0f, -3.0f * static_cast<float>(length) / (decay * rate));
        }
        for (int stage = 0; stage < 2; ++stage) {
            const auto length = static_cast<std::size_t>((AllpassTuning[stage] + channel * StereoSpread) * rate / 44100.0f);
            voice.allpasses[channel][stage].assign(std::max<std::size_t>(length, 1), 0.0f);
            voice.allpassPos[channel][stage] = 0;
        }
    }
    voice.reverbConfigured = true;
}

void renderReverb(VoiceMix& voice) {
    if (!voice.playing || !voice.reverbConfigured) return;
    const std::size_t frames = voice.input.size() / 2;
    for (std::size_t frame = 0; frame < frames; ++frame) {
        const float in = (voice.input[frame * 2] + voice.input[frame * 2 + 1]) * 0.5f;
        for (int channel = 0; channel < 2; ++channel) {
            float sum = 0.0f;
            for (int comb = 0; comb < 4; ++comb) {
                std::vector<float>& line = voice.combs[channel][comb];
                std::size_t& pos = voice.combPos[channel][comb];
                const float delayed = line[pos];
                float& filter = voice.combFilter[channel][comb];
                filter = delayed * (1.0f - voice.reverbDamp) + filter * voice.reverbDamp;
                line[pos] = in + filter * voice.reverbFeedback[comb];
                pos = (pos + 1) % line.size();
                sum += delayed;
            }
            for (int stage = 0; stage < 2; ++stage) {
                std::vector<float>& line = voice.allpasses[channel][stage];
                std::size_t& pos = voice.allpassPos[channel][stage];
                const float delayed = line[pos];
                line[pos] = sum + delayed * AllpassFeedback;
                pos = (pos + 1) % line.size();
                sum = delayed - sum;
            }
            voice.output[frame * 2 + channel] = sum * 0.25f * voice.reverbWet * voice.volume;
        }
    }
}

// Command records are 32 bytes: word 0 low byte is the type. The title's play and stop
// helpers (eboot 0x177e0 / 0x178c0) send type 2 with event 1 and 2; volume is type 6.
constexpr std::uint32_t CommandEvent = 2;
constexpr std::uint32_t CommandVolume = 6;
constexpr std::uint32_t EventPlay = 1;
constexpr std::uint32_t EventStop = 2;

template <class T>
T load(const std::uint8_t* source) {
    T value{};
    std::memcpy(&value, source, sizeof(value));
    return value;
}

void rewind(VoiceMix& voice) {
    voice.blockIndex = 0;
    voice.repeatsDone = 0;
    voice.position = voice.blocks.empty() ? 0.0 : voice.blocks.front().numSkipSamples;
}

// Moves past the finished block (or repeats it); stops the voice after the last one.
void advanceBlock(VoiceMix& voice) {
    const WaveformBlock& block = voice.blocks[voice.blockIndex];
    const bool playable = block.numSamples > block.numSkipSamples;
    if (playable && (block.numRepeats == InfiniteRepeats || voice.repeatsDone < block.numRepeats)) {
        ++voice.repeatsDone;
        voice.position -= block.numSamples - block.numSkipSamples;
        return;
    }
    const double overshoot = std::max(0.0, voice.position - block.numSamples);
    ++voice.blockIndex;
    voice.repeatsDone = 0;
    if (voice.blockIndex >= voice.blocks.size()) {
        voice.playing = false;
        return;
    }
    // Keep the fractional phase so pitched playback does not jitter at block joins.
    voice.position = voice.blocks[voice.blockIndex].numSkipSamples + overshoot;
}

float sampleAt(const VoiceMix& voice, const WaveformBlock& block, const std::uint32_t frame, const std::uint32_t channel) {
    const std::uint64_t byte = (static_cast<std::uint64_t>(frame) * voice.channels + channel) * sizeof(std::int16_t);
    if (byte + sizeof(std::int16_t) > block.dataSize) return 0.0f;
    return static_cast<float>(load<std::int16_t>(voice.data + block.dataOffset + byte)) / 32768.0f;
}

void renderSampler(VoiceMix& voice, const std::uint32_t grain, const std::uint32_t systemRate) {
    if (!voice.playing || voice.data == nullptr || voice.waveformType != WaveformPcmS16) return;
    if (voice.channels == 0 || voice.channels > 2 || voice.sampleRate == 0 || systemRate == 0) return;
    if (voice.blocks.empty()) {
        voice.playing = false;
        return;
    }
    const double step = static_cast<double>(voice.sampleRate) * voice.pitch / systemRate;
    for (std::uint32_t index = 0; index < grain; ++index) {
        while (voice.playing && voice.position >= voice.blocks[voice.blockIndex].numSamples) advanceBlock(voice);
        if (!voice.playing) return;
        const WaveformBlock& block = voice.blocks[voice.blockIndex];
        // Linear interpolation between the two neighbouring frames: pitched and non-48 kHz
        // sources stepped by nearest frame aliased audibly. The last frame holds its value.
        const auto frame = static_cast<std::uint32_t>(voice.position);
        const auto next = frame + 1 < block.numSamples ? frame + 1 : frame;
        const auto fraction = static_cast<float>(voice.position - frame);
        const auto lerp = [&](const std::uint32_t channel) {
            const float a = sampleAt(voice, block, frame, channel);
            return a + (sampleAt(voice, block, next, channel) - a) * fraction;
        };
        const float left = lerp(0);
        const float right = voice.channels == 2 ? lerp(1) : left;
        voice.output[index * 2] = left * voice.volume;
        voice.output[index * 2 + 1] = right * voice.volume;
        voice.position += step;
    }
}

void writeMastering(const VoiceMix& voice, const Ngs2RenderBufferInfo* buffers, const std::uint32_t numBuffers, const std::uint32_t grain) {
    if (voice.renderBufferIndex < 0 || static_cast<std::uint32_t>(voice.renderBufferIndex) >= numBuffers) return;
    const Ngs2RenderBufferInfo& buffer = buffers[voice.renderBufferIndex];
    if (buffer.waveform_type != WaveformPcmS16) return;
    auto* samples = static_cast<std::int16_t*>(buffer.buffer);
    for (std::uint32_t index = 0; index < grain; ++index) {
        const float left = voice.output[index * 2];
        const float right = voice.output[index * 2 + 1];
        for (std::uint32_t channel = 0; channel < buffer.num_channels; ++channel) {
            const float value = buffer.num_channels == 1 ? (left + right) * 0.5f : channel == 0 ? left : channel == 1 ? right : 0.0f;
            std::int16_t& target = samples[index * buffer.num_channels + channel];
            const float mixed = static_cast<float>(target) + value * 32767.0f;
            target = static_cast<std::int16_t>(std::lround(std::clamp(mixed, -32768.0f, 32767.0f)));
        }
    }
}

} // namespace

bool ApplyParam(VoiceMix& voice, const std::uint32_t id, const std::uint8_t* payload, const std::size_t size) {
    if (id == ParamPatch) {
        if (size < sizeof(Patch)) return false;
        const Patch patch = load<Patch>(payload);
        std::erase_if(voice.patches, [&patch](const Patch& existing) { return existing.port == patch.port; });
        if (patch.destHandle != 0) voice.patches.push_back(patch);
        return true;
    }
    if ((id >> 16) != voice.rackId) return false;
    switch (id) {
    case ParamSamplerSetup:
        if (size < 3 * sizeof(std::uint32_t)) return false;
        voice.waveformType = load<std::uint32_t>(payload);
        voice.channels = load<std::uint32_t>(payload + 4);
        voice.sampleRate = load<std::uint32_t>(payload + 8);
        return true;
    case ParamSamplerBlocks: {
        if (size < 0x18) return false;
        const auto data = load<std::uint64_t>(payload);
        const auto numBlocks = load<std::uint32_t>(payload + 0xC);
        const auto blocks = load<std::uint64_t>(payload + 0x10);
        if (numBlocks > MaxBlocks || (numBlocks != 0 && (data == 0 || blocks == 0))) return false;
        voice.data = reinterpret_cast<const std::uint8_t*>(data);
        voice.blocks.resize(numBlocks);
        if (numBlocks != 0) std::memcpy(voice.blocks.data(), reinterpret_cast<const void*>(blocks), numBlocks * sizeof(WaveformBlock));
        rewind(voice);
        return true;
    }
    case ParamReverbI3dl2: {
        if (size < sizeof(I3dl2)) return false;
        const I3dl2 params = load<I3dl2>(payload);
        if (!(params.decayTime > 0.0f) || !(params.decayHFRatio > 0.0f)) return false;
        voice.reverbLevelMb = params.room + params.reverb;
        voice.reverbDecayTime = params.decayTime;
        voice.reverbDecayHFRatio = params.decayHFRatio;
        voice.reverbParamsSet = true;
        voice.reverbConfigured = false;
        return true;
    }
    case ParamReverbSetup:
        // {input channels, output channels}: the network is stereo in, stereo out.
        return size >= 2 * sizeof(std::uint32_t) && load<std::uint32_t>(payload) == 2 && load<std::uint32_t>(payload + 4) == 2;
    case ParamSamplerPitch: {
        if (size < sizeof(float)) return false;
        const float pitch = load<float>(payload);
        if (!(pitch > 0.0f && pitch <= 16.0f)) return false;
        voice.pitch = pitch;
        return true;
    }
    case ParamSubmixerSetup:
    case ParamMasteringSetup:
        // Channel count of the bus; the mix runs in stereo and both titles' buses are stereo.
        return size >= sizeof(std::uint32_t) && load<std::uint32_t>(payload) == 2;
    case ParamMasteringOutput:
        if (size < sizeof(std::int32_t)) return false;
        voice.renderBufferIndex = load<std::int32_t>(payload);
        return true;
    default:
        return false;
    }
}

bool RunCommand(VoiceMix& voice, const std::uint32_t* record) {
    switch (record[0] & 0xFF) {
    case CommandEvent:
        if (record[2] == EventPlay) {
            voice.playing = voice.rackId != RackSampler || !voice.blocks.empty();
            rewind(voice);
            return true;
        }
        if (record[2] == EventStop) {
            voice.playing = false;
            return true;
        }
        return false;
    case CommandVolume: {
        float volume = 0.0f;
        std::memcpy(&volume, &record[2], sizeof(volume));
        if (!(volume >= 0.0f && volume <= 16.0f)) return false;
        voice.volume = volume;
        return true;
    }
    default:
        return false;
    }
}

std::uint32_t StateFlags(const VoiceMix& voice) {
    return voice.playing ? StateInUse | StatePlaying : 0;
}

void Render(const std::vector<VoiceMix*>& allVoices, const std::function<VoiceMix*(std::uint64_t)>& resolve,
            const Ngs2RenderBufferInfo* buffers, const std::uint32_t numBuffers, const std::uint32_t systemRate) {
    std::uint32_t grain = std::numeric_limits<std::uint32_t>::max();
    for (std::uint32_t index = 0; index < numBuffers; ++index) {
        const Ngs2RenderBufferInfo& buffer = buffers[index];
        if (buffer.waveform_type != WaveformPcmS16 || buffer.num_channels == 0 || buffer.num_channels > MaxRenderChannels) continue;
        grain = std::min<std::uint32_t>(grain, static_cast<std::uint32_t>(buffer.buffer_size / (buffer.num_channels * sizeof(std::int16_t))));
    }
    if (grain == std::numeric_limits<std::uint32_t>::max() || grain == 0) return;
    // An idle voice produces nothing and drops its input, so only playing voices are mixed.
    // The title owns 256 sampler voices with a handful sounding at once.
    std::vector<VoiceMix*> voices;
    for (VoiceMix* voice : allVoices)
        if (voice->playing) voices.push_back(voice);
    for (VoiceMix* voice : voices) {
        voice->input.assign(static_cast<std::size_t>(grain) * 2, 0.0f);
        voice->output.assign(static_cast<std::size_t>(grain) * 2, 0.0f);
    }
    // Kahn order over the patch graph so every voice runs after all of its sources, whatever
    // the rack creation order; voices left in a cycle keep their given order at the end.
    std::vector<std::size_t> pending(voices.size(), 0);
    std::vector<std::vector<std::size_t>> edges(voices.size());
    for (std::size_t source = 0; source < voices.size(); ++source)
        for (const Patch& patch : voices[source]->patches) {
            const auto found = std::find(voices.begin(), voices.end(), resolve(patch.destHandle));
            if (found == voices.end() || *found == voices[source]) continue;
            const auto target = static_cast<std::size_t>(found - voices.begin());
            edges[source].push_back(target);
            ++pending[target];
        }
    std::vector<VoiceMix*> order;
    std::vector<bool> placed(voices.size(), false);
    for (std::size_t scan = 0; scan < voices.size(); ++scan)
        if (pending[scan] == 0) order.push_back(voices[scan]), placed[scan] = true;
    for (std::size_t next = 0; next < order.size(); ++next) {
        const auto index = static_cast<std::size_t>(std::find(voices.begin(), voices.end(), order[next]) - voices.begin());
        for (const std::size_t target : edges[index])
            if (--pending[target] == 0 && !placed[target]) order.push_back(voices[target]), placed[target] = true;
    }
    for (std::size_t index = 0; index < voices.size(); ++index)
        if (!placed[index]) order.push_back(voices[index]);
    for (VoiceMix* voice : order) {
        switch (voice->rackId) {
        case RackSampler:
            renderSampler(*voice, grain, systemRate);
            break;
        case RackSubmixer:
        case RackMastering:
            if (voice->playing)
                for (std::size_t index = 0; index < voice->output.size(); ++index) voice->output[index] = voice->input[index] * voice->volume;
            break;
        case RackReverb:
            if (voice->reverbParamsSet && !voice->reverbConfigured) configureReverb(*voice, systemRate);
            renderReverb(*voice);
            break;
        default:
            // Unknown racks contribute nothing rather than a guessed effect.
            break;
        }
        if (voice->rackId == RackMastering) writeMastering(*voice, buffers, numBuffers, grain);
        for (const Patch& patch : voice->patches) {
            VoiceMix* destination = resolve(patch.destHandle);
            if (destination == nullptr || destination == voice || !destination->playing) continue;
            for (std::size_t index = 0; index < voice->output.size(); ++index) destination->input[index] += voice->output[index];
        }
    }
}

} // namespace ngs2mix

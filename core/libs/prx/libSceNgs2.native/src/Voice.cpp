#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <numbers>
#include <stdexcept>
#include <string>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

static constexpr std::uint32_t COMMAND_EVENT = 2;
static constexpr std::uint32_t COMMAND_MATRIX_LEVELS = 5;
static constexpr std::uint32_t COMMAND_PORT_VOLUME = 6;
static constexpr std::uint32_t COMMAND_PORT_MATRIX = 7;
static constexpr std::uint8_t COMMAND_TYPE_FLOAT = 1;
static constexpr std::uint8_t COMMAND_TYPE_INT = 3;
static constexpr std::uint8_t COMMAND_TYPE_UINT = 4;
static constexpr std::uint8_t COMMAND_TYPE_FLOAT_ARRAY = 0x11;

void Ngs2Voice::SetEvent(std::uint32_t eventId) {
    switch (eventId) {
        case SCE_NGS2_VOICE_EVENT_PLAY:
            if (state == Ngs2PlayState::Empty || state == Ngs2PlayState::Stopped) {
                state = Ngs2PlayState::Playing;
                stateFlags |= SCE_NGS2_VOICE_STATE_FLAG_INUSE;
            }
            break;
        case SCE_NGS2_VOICE_EVENT_STOP:
            if (state == Ngs2PlayState::Playing || state == Ngs2PlayState::Paused) state = Ngs2PlayState::Stopped;
            break;
        case SCE_NGS2_VOICE_EVENT_STOP_IMM:
        case SCE_NGS2_VOICE_EVENT_KILL:
            state = Ngs2PlayState::Empty;
            if (reverb) reverb->Clear();
            break;
        case SCE_NGS2_VOICE_EVENT_PAUSE:
            if (state == Ngs2PlayState::Playing) state = Ngs2PlayState::Paused;
            break;
        case SCE_NGS2_VOICE_EVENT_RESUME:
            if (state == Ngs2PlayState::Paused) state = Ngs2PlayState::Playing;
            break;
        default: throw std::invalid_argument("NGS2: unknown voice event " + Ngs2Hex(eventId));
    }
}

void Ngs2Voice::ResetSetup() {
    SetEvent(SCE_NGS2_VOICE_EVENT_STOP_IMM);
    std::fill(ports.begin(), ports.end(), Ngs2Port{});
    for (auto& matrix : matrices) matrix.clear();
    filters.clear();
    fbwLevel = 1.0f;
    lfeLevel = 1.0f;
    channels = 0;
    inputChannels = 0;
    sampleRate = 0;
    waveformType = 0;
    atrac9 = {};
    reverb.reset();
    pitch = 1.0f;
    phase = 0;
    blocks.clear();
    acceptsBlocks = true;
    decodedSamples = 0;
    decodedBytes = 0;
    waveformEnd = nullptr;
}

const std::uint8_t* Ngs2StreamEnd(const Ngs2Voice& voice, const Ngs2Block& block) {
    const auto& last = block.pieces.back();
    return last.data + last.frames * voice.channels * sizeof(std::int16_t);
}

const std::uint8_t* Ngs2Voice::WaveformData() const {
    if (blocks.empty()) return waveformEnd;
    const auto& block = blocks.front();
    if (waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9) return block.data + block.dataCursor;
    if (block.streaming) {
        for (const auto& piece : block.pieces) {
            if (block.cursor >= piece.firstFrame && block.cursor - piece.firstFrame < piece.frames) return piece.data + (block.cursor - piece.firstFrame) * channels * sizeof(std::int16_t);
        }
        return Ngs2StreamEnd(*this, block);
    }
    return block.data + (static_cast<std::size_t>(block.info.num_skip_samples) + block.cursor) * channels * sizeof(std::int16_t);
}

static Ngs2Port& PortAt(Ngs2Voice& voice, std::uint32_t port) {
    if (port >= voice.ports.size()) throw std::invalid_argument("NGS2: voice port " + std::to_string(port) + " is out of range");
    return voice.ports[port];
}

static void SetMatrixLevels(Ngs2Voice& voice, std::uint32_t matrixId, const float* levels, std::uint32_t numLevels) {
    if (matrixId >= voice.matrices.size() || numLevels > NGS2_MAX_CHANNELS * NGS2_MAX_CHANNELS || (levels == nullptr && numLevels != 0)) {
        APS5_INVALID_ARG_EX;
    }
    voice.matrices[matrixId].assign(levels, levels + numLevels);
}

static void SetPortMatrix(Ngs2Voice& voice, std::uint32_t port, std::int32_t matrixId) {
    if (matrixId < -1 || matrixId >= static_cast<std::int32_t>(voice.matrices.size())) APS5_INVALID_ARG_EX;
    PortAt(voice, port).matrix = matrixId;
}

static void Patch(Ngs2Voice& voice, const Ngs2VoicePatchParam& patch) {
    auto& port = PortAt(voice, patch.port);
    if (patch.dest_input_id != 0) throw std::runtime_error("NGS2: voice input " + std::to_string(patch.dest_input_id) + " is not implemented");
    if (patch.dest_handle == 0) {
        port.dest = nullptr;
        return;
    }
    auto* dest = Ngs2FindVoice(patch.dest_handle);
    if (dest == nullptr || dest->rack->system != voice.rack->system || dest->rack->rackId == SCE_NGS2_RACK_ID_SAMPLER) APS5_INVALID_ARG_EX;
    port.dest = dest;
}

static void ApplyCommonParam(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param) {
    switch (param.id) {
        case SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS: {
            const auto& levels = ParamAs<Ngs2VoiceMatrixLevelsParam>(param);
            SetMatrixLevels(voice, levels.matrix_id, levels.levels, levels.num_levels);
            break;
        }
        case SCE_NGS2_VOICE_PARAM_PORT_VOLUME: {
            const auto& volume = ParamAs<Ngs2VoicePortVolumeParam>(param);
            PortAt(voice, volume.port).volume = volume.level;
            break;
        }
        case SCE_NGS2_VOICE_PARAM_PORT_MATRIX: {
            const auto& matrix = ParamAs<Ngs2VoicePortMatrixParam>(param);
            SetPortMatrix(voice, matrix.port, matrix.matrix_id);
            break;
        }
        case SCE_NGS2_VOICE_PARAM_PORT_DELAY: {
            const auto& delay = ParamAs<Ngs2VoicePortDelayParam>(param);
            PortAt(voice, delay.port);
            if (delay.num_samples != 0) throw std::runtime_error("NGS2: port delay is not implemented");
            break;
        }
        case SCE_NGS2_VOICE_PARAM_PATCH: Patch(voice, ParamAs<Ngs2VoicePatchParam>(param)); break;
        case SCE_NGS2_VOICE_PARAM_EVENT: voice.SetEvent(ParamAs<Ngs2VoiceEventParam>(param).event_id); break;
        case SCE_NGS2_VOICE_PARAM_CALLBACK: {
            const auto& callback = ParamAs<Ngs2VoiceCallbackParam>(param);
            voice.callback = callback.callback;
            voice.callbackData = callback.callback_data;
            voice.callbackFlags = callback.flags;
            break;
        }
        default: throw std::runtime_error("NGS2: voice param " + Ngs2Hex(param.id) + " is not implemented");
    }
}

static void SetupSampler(Ngs2Voice& voice, const Ngs2WaveformFormat& format) {
    voice.ResetSetup();
    if (format.waveform_type == 0 && format.num_channels == 0 && format.sample_rate == 0) return;
    if (format.waveform_type != SCE_NGS2_WAVEFORM_TYPE_PCM_I16L && format.waveform_type != SCE_NGS2_WAVEFORM_TYPE_ATRAC9) {
        throw std::runtime_error("NGS2: waveform type " + Ngs2Hex(format.waveform_type) + " is not implemented");
    }
    if (format.frame_offset != 0 || format.frame_margin != 0) throw std::runtime_error("NGS2: waveform frame offset and margin are not implemented");
    if (format.num_channels == 0 || format.num_channels > NGS2_MAX_CHANNELS || format.sample_rate == 0) APS5_INVALID_ARG_EX;
    voice.channels = format.num_channels;
    voice.sampleRate = format.sample_rate;
    voice.waveformType = format.waveform_type;
    if (format.waveform_type == SCE_NGS2_WAVEFORM_TYPE_ATRAC9) Ngs2SetupAtrac9(voice, format);
}

static void AppendStreamData(Ngs2Voice& voice, const Ngs2SamplerVoiceWaveformBlocksParam& param, std::size_t frameBytes) {
    if (voice.blocks.empty() || !voice.blocks.back().streaming) throw std::runtime_error("NGS2: appending waveform data to a voice without a streaming waveform is not implemented");
    auto& stream = voice.blocks.back();
    for (std::uint32_t i = 0; i < param.num_blocks; i++) {
        const auto& block = param.blocks[i];
        if (block.data_size == 0) continue;
        if (block.data_size % frameBytes != 0 || block.num_skip_samples != 0 || block.num_repeats != 0) {
            throw std::runtime_error("NGS2: appending a waveform block with skipped samples, repeats or a partial frame is not implemented");
        }
        stream.pieces.push_back({static_cast<const std::uint8_t*>(param.data) + block.data_offset, stream.availableFrames, block.data_size / frameBytes});
        stream.availableFrames += block.data_size / frameBytes;
    }
}

static void AddWaveformBlocks(Ngs2Voice& voice, const Ngs2SamplerVoiceWaveformBlocksParam& param) {
    constexpr std::uint32_t knownFlags = SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET;
    if ((param.flags & ~knownFlags) != 0) throw std::runtime_error("NGS2: waveform block flags " + Ngs2Hex(param.flags) + " are not implemented");
    if (voice.channels == 0 || (param.num_blocks != 0 && (param.blocks == nullptr || param.data == nullptr))) APS5_INVALID_ARG_EX;
    const bool reset = (param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET) != 0;
    if (!voice.acceptsBlocks && !reset) throw std::invalid_argument("NGS2: the voice waveform was already closed");
    if (reset) {
        voice.blocks.clear();
        voice.phase = 0;
        voice.waveformEnd = nullptr;
        if (voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9) Ngs2RestartAtrac9(voice);
    }
    const std::size_t frameBytes = voice.channels * sizeof(std::int16_t);
    if ((param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND) != 0) {
        if (voice.waveformType != SCE_NGS2_WAVEFORM_TYPE_PCM_I16L) throw std::runtime_error("NGS2: appending waveform data to a voice that is not 16-bit PCM is not implemented");
        AppendStreamData(voice, param, frameBytes);
        voice.acceptsBlocks = (param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE) != 0;
        return;
    }
    voice.acceptsBlocks = (param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE) != 0;
    for (std::uint32_t i = 0; i < param.num_blocks; i++) {
        const auto& block = param.blocks[i];
        if (block.num_samples == 0 && block.data_size == 0) continue;
        const std::uint64_t bytes = voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9 ? Ngs2Atrac9BlockBytes(voice, block)
                                  : (static_cast<std::uint64_t>(block.num_skip_samples) + block.num_samples) * frameBytes;
        const bool streaming = block.num_samples != 0 && bytes > block.data_size && voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_PCM_I16L &&
                               block.num_skip_samples == 0 && block.num_repeats == 0 && block.data_size >= frameBytes && block.data_size % frameBytes == 0;
        if (!streaming && (block.num_samples == 0 || bytes > block.data_size)) {
            throw std::invalid_argument("NGS2: waveform block " + std::to_string(i) + " does not fit its data");
        }
        voice.blocks.push_back({static_cast<const std::uint8_t*>(param.data) + block.data_offset, block});
        if (streaming) {
            auto& stream = voice.blocks.back();
            stream.streaming = true;
            stream.availableFrames = block.data_size / frameBytes;
            stream.pieces.push_back({stream.data, 0, stream.availableFrames});
        }
    }
}

static void SetFilter(Ngs2Voice& voice, const Ngs2SamplerVoiceFilterParam& param) {
    if (param.index >= voice.rack->maxFilters) APS5_INVALID_ARG_EX;
    if (voice.filters.size() <= param.index) voice.filters.resize(param.index + 1);
    auto& filter = voice.filters[param.index];
    if (param.type == 0) {
        filter = {};
        return;
    }
    if (param.location != 1 || param.type != 1) {
        throw std::runtime_error("NGS2: sampler filter location " + std::to_string(param.location) + " type " + std::to_string(param.type) + " is not implemented");
    }
    if (!std::isfinite(param.frequency) || param.frequency < 0.0f || !std::isfinite(param.q) || param.q <= 0.0f || !std::isfinite(param.level) || param.level < 0.0f) {
        APS5_INVALID_ARG_EX;
    }
    const double rate = voice.rack->system->option.sample_rate;
    filter.enabled = true;
    filter.bypassMask = param.channel_mask;
    filter.history.resize(voice.channels);
    if (param.frequency == 0.0f || param.frequency >= rate * 0.5) {
        filter.b0 = param.frequency == 0.0f ? 0.0 : param.level;
        filter.b1 = filter.b2 = filter.a1 = filter.a2 = 0.0;
        return;
    }
    const double omega = 2.0 * std::numbers::pi * param.frequency / rate;
    const double cosine = std::cos(omega);
    const double alpha = std::sin(omega) / (2.0 * param.q);
    const double inverseA0 = 1.0 / (1.0 + alpha);
    filter.b0 = (1.0 - cosine) * 0.5 * inverseA0 * param.level;
    filter.b1 = 2.0 * filter.b0;
    filter.b2 = filter.b0;
    filter.a1 = -2.0 * cosine * inverseA0;
    filter.a2 = (1.0 - alpha) * inverseA0;
}

static void ApplySamplerParam(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param) {
    switch (param.id) {
        case SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP: SetupSampler(voice, ParamAs<Ngs2SamplerVoiceSetupParam>(param).format); break;
        case SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS: AddWaveformBlocks(voice, ParamAs<Ngs2SamplerVoiceWaveformBlocksParam>(param)); break;
        case SCE_NGS2_SAMPLER_VOICE_PARAM_EXIT_LOOP: {
            const auto looping = std::find_if(voice.blocks.begin(), voice.blocks.end(), [](const Ngs2Block& block) { return block.info.num_repeats != 0; });
            if (looping != voice.blocks.end()) looping->info.num_repeats = 0;
            break;
        }
        case SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH: {
            const float ratio = ParamAs<Ngs2SamplerVoicePitchParam>(param).ratio;
            if (!std::isfinite(ratio) || ratio < 0.0f) APS5_INVALID_ARG_EX;
            voice.pitch = ratio;
            break;
        }
        case SCE_NGS2_SAMPLER_VOICE_PARAM_FILTER: SetFilter(voice, ParamAs<Ngs2SamplerVoiceFilterParam>(param)); break;
        default: throw std::runtime_error("NGS2: sampler voice param " + Ngs2Hex(param.id) + " is not implemented");
    }
}

static void SetupMixer(Ngs2Voice& voice, std::uint32_t numChannels) {
    if (numChannels == 0 || numChannels > voice.rack->maxChannels) APS5_INVALID_ARG_EX;
    voice.ResetSetup();
    voice.channels = numChannels;
}

static void ApplyParam(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param) {
    const std::uint32_t rackId = param.id >> 16;
    if (rackId == 0) {
        ApplyCommonParam(voice, param);
        return;
    }
    if (rackId == (SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 >> 16) || rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER) {
        Ngs2ApplyCustomParam(voice, param);
        return;
    }
    if (rackId != voice.rack->rackId) throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " belongs to another rack");
    switch (rackId) {
        case SCE_NGS2_RACK_ID_SAMPLER: ApplySamplerParam(voice, param); return;
        case SCE_NGS2_RACK_ID_SUBMIXER:
            if (param.id != SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP) break;
            if (ParamAs<Ngs2SubmixerVoiceSetupParam>(param).flags != 0) throw std::runtime_error("NGS2: submixer setup flags are not implemented");
            SetupMixer(voice, ParamAs<Ngs2SubmixerVoiceSetupParam>(param).num_io_channels);
            return;
        case SCE_NGS2_RACK_ID_REVERB:
            if (param.id == SCE_NGS2_REVERB_VOICE_PARAM_SETUP) {
                const auto& setup = ParamAs<Ngs2ReverbVoiceSetupParam>(param);
                if (setup.flags != 0) throw std::runtime_error("NGS2: reverb setup flags are not implemented");
                if (setup.num_input_channels == 0 || setup.num_input_channels > voice.rack->maxChannels) APS5_INVALID_ARG_EX;
                SetupMixer(voice, setup.num_output_channels);
                voice.inputChannels = setup.num_input_channels;
                return;
            }
            if (param.id == SCE_NGS2_REVERB_VOICE_PARAM_I3DL2) {
                Ngs2SetReverb(voice, ParamAs<Ngs2ReverbVoiceI3dl2Param>(param).i3dl2);
                return;
            }
            break;
        case SCE_NGS2_RACK_ID_MASTERING:
            if (param.id == SCE_NGS2_MASTERING_VOICE_PARAM_SETUP) {
                SetupMixer(voice, ParamAs<Ngs2MasteringVoiceSetupParam>(param).num_io_channels);
                return;
            }
            if (param.id == SCE_NGS2_MASTERING_VOICE_PARAM_GAIN) {
                const auto& gain = ParamAs<Ngs2MasteringVoiceGainParam>(param);
                if (!std::isfinite(gain.fbw_level) || !std::isfinite(gain.lfe_level)) APS5_INVALID_ARG_EX;
                voice.fbwLevel = gain.fbw_level;
                voice.lfeLevel = gain.lfe_level;
                return;
            }
            if (param.id == SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT) {
                voice.outputId = ParamAs<Ngs2MasteringVoiceOutputParam>(param).output_id;
                return;
            }
            break;
        default: break;
    }
    throw std::runtime_error("NGS2: voice param " + Ngs2Hex(param.id) + " is not implemented");
}

static Ngs2Voice& CheckedVoice(Ngs2Handle handle) {
    auto* voice = Ngs2FindVoice(handle);
    if (voice == nullptr) throw std::invalid_argument("NGS2: invalid voice handle");
    return *voice;
}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceNgs2VoiceControl(uintptr_t voice_handle, const Ngs2VoiceParamHeader* param_list) {
    std::lock_guard lock(Ngs2Mutex());
    auto& voice = CheckedVoice(voice_handle);
    if (param_list == nullptr) APS5_INVALID_ARG_EX;
    for (const auto* param = param_list;; param = reinterpret_cast<const Ngs2VoiceParamHeader*>(reinterpret_cast<const std::uint8_t*>(param) + param->next)) {
        ApplyParam(voice, *param);
        if (param->next == 0) break;
    }
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2VoiceRunCommands(uintptr_t voice_handle, const Ngs2VoiceCommand* commands, size_t num_commands) {
    std::lock_guard lock(Ngs2Mutex());
    auto& voice = CheckedVoice(voice_handle);
    if (commands == nullptr && num_commands != 0) APS5_INVALID_ARG_EX;
    for (std::size_t i = 0; i < num_commands; i++) {
        const auto& command = commands[i];
        const std::uint32_t index = command.id >> 24;
        const std::uint32_t id = command.id & 0xffffff;
        const std::uint8_t expectedType = id == COMMAND_EVENT ? COMMAND_TYPE_UINT
                                        : id == COMMAND_MATRIX_LEVELS ? COMMAND_TYPE_FLOAT_ARRAY
                                        : id == COMMAND_PORT_VOLUME ? COMMAND_TYPE_FLOAT
                                        : id == COMMAND_PORT_MATRIX ? COMMAND_TYPE_INT : 0;
        if (expectedType == 0) throw std::runtime_error("NGS2: voice command " + Ngs2Hex(command.id) + " is not implemented");
        if (command.type != expectedType) throw std::invalid_argument("NGS2: voice command " + Ngs2Hex(command.id) + " has value type " + Ngs2Hex(command.type));
        if (id == COMMAND_EVENT) voice.SetEvent(command.value.u);
        else if (id == COMMAND_MATRIX_LEVELS) SetMatrixLevels(voice, index, command.value.levels, command.count);
        else if (id == COMMAND_PORT_VOLUME) PortAt(voice, index).volume = command.value.f;
        else SetPortMatrix(voice, index, command.value.i);
    }
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2VoiceGetState(uintptr_t voice_handle, Ngs2VoiceState* state, size_t state_size) {
    if (state == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::lock_guard lock(Ngs2Mutex());
    const auto& voice = CheckedVoice(voice_handle);
    switch (voice.rack->rackId) {
        case SCE_NGS2_RACK_ID_SAMPLER: {
            if (state_size != sizeof(Ngs2SamplerVoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
            auto& sampler = *reinterpret_cast<Ngs2SamplerVoiceState*>(state);
            sampler = {};
            sampler.voice_state.state_flags = voice.stateFlags;
            sampler.envelope_height = 1.0f;
            sampler.num_decoded_samples = voice.decodedSamples;
            sampler.decoded_data_size = voice.decodedBytes;
            sampler.user_data = voice.blocks.empty() ? 0 : voice.blocks.front().info.user_data;
            sampler.waveform_data = voice.WaveformData();
            return SCE_NGS2_OK;
        }
        case SCE_NGS2_RACK_ID_SUBMIXER: {
            if (state_size != sizeof(Ngs2SubmixerVoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
            auto& submixer = *reinterpret_cast<Ngs2SubmixerVoiceState*>(state);
            submixer = {};
            submixer.voice_state.state_flags = voice.stateFlags;
            return SCE_NGS2_OK;
        }
        case SCE_NGS2_RACK_ID_REVERB:
            if (state_size != sizeof(Ngs2VoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
            *state = {voice.stateFlags, 0};
            return SCE_NGS2_OK;
        default: throw std::runtime_error("NGS2: voice state of rack " + Ngs2Hex(voice.rack->rackId) + " is not implemented");
    }
}

int APS5_VABI sceNgs2VoiceGetPortInfo(uintptr_t voice_handle, uint32_t port, Ngs2VoicePortInfo* info, size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size != sizeof(Ngs2VoicePortInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    std::lock_guard lock(Ngs2Mutex());
    auto& voice = CheckedVoice(voice_handle);
    const auto& source = PortAt(voice, port);
    *info = {};
    info->matrix_id = source.matrix;
    info->volume = source.volume;
    info->dest_handle = reinterpret_cast<Ngs2Handle>(source.dest);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2VoiceQueryInfo(uintptr_t voice_handle, uint32_t info_id, void* info, size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::lock_guard lock(Ngs2Mutex());
    const auto& voice = CheckedVoice(voice_handle);
    switch (info_id) {
        case SCE_NGS2_VOICE_INFO_CHANNELS: {
            if (info_size != sizeof(Ngs2VoiceChannelsInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
            *static_cast<Ngs2VoiceChannelsInfo*>(info) = {voice.channels, 0};
            return SCE_NGS2_OK;
        }
        default: throw std::runtime_error("NGS2: voice info " + Ngs2Hex(info_id) + " is not implemented");
    }
}

int APS5_VABI sceNgs2VoiceGetStateFlags(uintptr_t voice_handle, uint32_t* state_flags) {
    if (state_flags == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::lock_guard lock(Ngs2Mutex());
    *state_flags = CheckedVoice(voice_handle).stateFlags;
    return SCE_NGS2_OK;
}

}

#pragma GCC visibility pop

#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <numbers>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

static constexpr std::uint32_t COMMAND_EVENT = 2;
static constexpr std::uint32_t COMMAND_MATRIX_LEVELS = 5;
static constexpr std::uint32_t COMMAND_PORT_VOLUME = 6;
static constexpr std::uint32_t COMMAND_PORT_MATRIX = 7;
static constexpr std::uint8_t COMMAND_TYPE_FLOAT = 1;
static constexpr std::uint8_t COMMAND_TYPE_INT = 3;
static constexpr std::uint8_t COMMAND_TYPE_FLOAT_ARRAY = 0x11;
static constexpr float MAX_MATRIX_LEVEL = 4.0f;
static constexpr float MAX_MASTERING_GAIN = 32.0f;

struct Ngs2ReturnCode {
    int code;
};

static void SendEvent(Ngs2Voice& voice, std::uint32_t eventId) {
    if (eventId == SCE_NGS2_VOICE_EVENT_PLAY && voice.channels == 0) throw Ngs2ReturnCode{SCE_NGS2_ERROR_INVALID_VOICE_ACCESS};
    voice.SetEvent(eventId);
}

struct Ngs2ControlFrame;
static thread_local Ngs2ControlFrame* currentControl = nullptr;

struct Ngs2ControlFrame {
    Ngs2Voice& voice;
    Ngs2ControlFrame* previous;
    explicit Ngs2ControlFrame(Ngs2Voice& value) : voice(value), previous(currentControl) {
        currentControl = this;
        voice.controlOwner = std::this_thread::get_id();
        if (voice.rack->system != nullptr) ++voice.rack->system->controlCallbacks;
    }
    ~Ngs2ControlFrame() {
        currentControl = previous;
        voice.controlOwner = {};
        if (voice.rack->system != nullptr) --voice.rack->system->controlCallbacks;
        Ngs2NotifyWaiters();
    }
    Ngs2ControlFrame(const Ngs2ControlFrame&) = delete;
    Ngs2ControlFrame& operator=(const Ngs2ControlFrame&) = delete;
};

bool Ngs2ControlActive(const Ngs2System& system) {
    for (auto* frame = currentControl; frame != nullptr; frame = frame->previous)
        if (frame->voice.rack->system == &system) return true;
    return false;
}

bool Ngs2ControlActive(const Ngs2Rack& rack) {
    for (auto* frame = currentControl; frame != nullptr; frame = frame->previous)
        if (frame->voice.rack == &rack) return true;
    return false;
}

static void CheckControlReentry(const Ngs2Voice& voice) {
    for (auto* frame = currentControl; frame != nullptr; frame = frame->previous)
        if (&frame->voice == &voice) throw std::runtime_error("NGS2: same-voice reentry from a UserFx2 control handler is not implemented");
}

class Ngs2VoiceAccess {
public:
    Ngs2Voice* const voice;
    explicit Ngs2VoiceAccess(Ngs2Handle handle) : voice(Ngs2FindVoice(handle)) {
        if (voice == nullptr) return;
        CheckControlReentry(*voice);
        ++voice->rack->voiceAccesses;
        if (voice->rack->system != nullptr) ++voice->rack->system->voiceAccesses;
        try {
            while (voice->controlOwner != std::thread::id{}) Ngs2WaitOnce();
        } catch (...) {
            release();
            throw;
        }
    }
    ~Ngs2VoiceAccess() { if (voice != nullptr) release(); }
    Ngs2VoiceAccess(const Ngs2VoiceAccess&) = delete;
    Ngs2VoiceAccess& operator=(const Ngs2VoiceAccess&) = delete;
private:
    void release() {
        --voice->rack->voiceAccesses;
        if (voice->rack->system != nullptr) --voice->rack->system->voiceAccesses;
        Ngs2NotifyWaiters();
    }
};

bool Ngs2Voice::IsSampler() const {
    return rack != nullptr && (rack->rackId == SCE_NGS2_RACK_ID_SAMPLER || rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER);
}

bool Ngs2Voice::IsMixer() const {
    return rack != nullptr && (rack->rackId == SCE_NGS2_RACK_ID_SUBMIXER || rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER ||
        rack->rackId == SCE_NGS2_RACK_ID_MASTERING || rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING);
}

bool Ngs2Voice::IsMastering() const {
    return rack != nullptr && (rack->rackId == SCE_NGS2_RACK_ID_MASTERING || rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING);
}

std::uint32_t Ngs2Voice::UserFxFlags(std::uint32_t flags) const {
    if (outputTransition == Ngs2VoiceTransition::Stop) flags |= 8;
    else if (outputTransition == Ngs2VoiceTransition::Kill) flags |= 32;
    else if (state == Ngs2PlayState::Paused && outputTransition != Ngs2VoiceTransition::Pause) flags |= 4;
    return flags;
}

void Ngs2Voice::SetEvent(std::uint32_t eventId) {
    if (state != Ngs2PlayState::Empty &&
        (eventId == SCE_NGS2_VOICE_EVENT_STOP || eventId == SCE_NGS2_VOICE_EVENT_STOP_IMM || eventId == SCE_NGS2_VOICE_EVENT_KILL)) {
        for (auto& gain : customGain) if (gain.configured) gain.lifecycleUnknown = true;
    }
    switch (eventId) {
        case SCE_NGS2_VOICE_EVENT_PLAY:
            if (state == Ngs2PlayState::Empty || state == Ngs2PlayState::Stopped) {
                if (IsSampler()) Ngs2ReplaySampler(*this);
                if (Ngs2HasPitchShift(*this) || IsSampler() || IsMixer()) {
                    Ngs2RestartPitchShift(*this);
                    outputTransition = Ngs2VoiceTransition::None;
                }
                state = Ngs2PlayState::Playing;
                stateFlags |= SCE_NGS2_VOICE_STATE_FLAG_INUSE;
                if (userFxHandler) userFxFlags |= 3;
                for (auto& fx : customUserFx) if (fx.handler) fx.flags |= 3;
                for (auto& fx : userFx) {
                    if (fx.configured) {
                        std::copy(fx.controlParam.begin(), fx.controlParam.end(), fx.param.begin());
                        fx.active = true;
                    }
                    fx.flags |= 3;
                }
            }
            break;
        case SCE_NGS2_VOICE_EVENT_STOP:
            if (state == Ngs2PlayState::Playing || state == Ngs2PlayState::Paused) {
                state = Ngs2PlayState::Stopped;
                if (Ngs2HasPitchShift(*this) || IsSampler() || IsMixer()) outputTransition = Ngs2VoiceTransition::Stop;
            }
            break;
        case SCE_NGS2_VOICE_EVENT_STOP_IMM:
        case SCE_NGS2_VOICE_EVENT_KILL:
            state = Ngs2PlayState::Empty;
            Ngs2ClearReverb(*this);
            Ngs2RestartPitchShift(*this);
            outputTransition = Ngs2VoiceTransition::None;
            break;
        case SCE_NGS2_VOICE_EVENT_PAUSE:
            if (state == Ngs2PlayState::Playing) {
                state = Ngs2PlayState::Paused;
                if (Ngs2HasPitchShift(*this) || IsSampler() || IsMixer()) outputTransition = Ngs2VoiceTransition::Pause;
            }
            break;
        case SCE_NGS2_VOICE_EVENT_RESUME:
            if (state == Ngs2PlayState::Paused) {
                state = Ngs2PlayState::Playing;
                if (Ngs2HasPitchShift(*this) || IsSampler() || IsMixer()) outputTransition = Ngs2VoiceTransition::Resume;
            }
            break;
        default: throw std::invalid_argument("NGS2: unknown voice event " + Ngs2Hex(eventId));
    }
}

static void ValidateDeferredEvent(const Ngs2Voice& voice, std::uint32_t event) {
    if (event == SCE_NGS2_VOICE_EVENT_PLAY && voice.channels == 0) throw Ngs2ReturnCode{SCE_NGS2_ERROR_INVALID_VOICE_ACCESS};
    if (!voice.IsSampler() && !voice.IsMixer()) throw std::runtime_error("NGS2: deferred events for this voice configuration are not implemented");
    switch (event) {
        case SCE_NGS2_VOICE_EVENT_PLAY:
        case SCE_NGS2_VOICE_EVENT_STOP:
        case SCE_NGS2_VOICE_EVENT_STOP_IMM:
        case SCE_NGS2_VOICE_EVENT_KILL:
        case SCE_NGS2_VOICE_EVENT_PAUSE:
        case SCE_NGS2_VOICE_EVENT_RESUME: return;
        default: throw std::runtime_error("NGS2: this deferred voice event is not implemented");
    }
}

static void PublishPendingEvent(Ngs2Voice& voice, const Ngs2PendingControl& control) {
    const auto* event = std::get_if<std::uint32_t>(&control);
    if (event == nullptr) return;
    if (*event == SCE_NGS2_VOICE_EVENT_PLAY) {
        voice.pendingPlay = true;
        if (Ngs2HasPitchShift(voice)) voice.KillForRender(false);
    } else if (*event == SCE_NGS2_VOICE_EVENT_KILL) {
        voice.pendingPlay = false;
        if (!voice.IsMastering()) voice.KillForRender(true);
    }
}

static bool DeferringControls(const Ngs2Voice& voice) {
    return (voice.rack->system != nullptr && (voice.rack->system->processingVoices || voice.rack->system->jobsExecuting)) || !voice.pendingControls.empty();
}

static void EnqueueControl(Ngs2Voice& voice, Ngs2PendingControl control) {
    voice.pendingControls.push_back(std::move(control));
    PublishPendingEvent(voice, voice.pendingControls.back());
}

void Ngs2Voice::ResetSetup() {
    Ngs2ResetUserFx2(*this);
    for (auto& gain : customGain) if (gain.configured) gain.lifecycleUnknown = true;
    ++waveformRevision;
    SetEvent(SCE_NGS2_VOICE_EVENT_STOP_IMM);
    Ngs2ResetPitchShift(*this);
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
    pitch = 1.0f;
    phase = 0;
    blocks.clear();
    samplerReplayBlocks.clear();
    samplerWritePosition = 0;
    acceptsBlocks = true;
    decodedSamples = 0;
    decodedBytes = 0;
    waveformData = nullptr;
    waveformUserData = 0;
    userFxHandler = nullptr;
    userFxData = {};
    userFxFlags = 0;
    callback = nullptr;
    callbackData = 0;
    callbackFlags = 0;
    peakMeterHistory.clear();
    peakMeterMode = 0;
    peakMeterCount = 0;
    peakMeterCursor = 0;
    peakHeight = 0.0f;
}

const std::uint8_t* Ngs2Voice::WaveformData() const {
    return waveformData;
}

static Ngs2Port& PortAt(Ngs2Voice& voice, std::uint32_t port) {
    if (port >= voice.ports.size()) throw std::invalid_argument("NGS2: voice port " + std::to_string(port) + " is out of range");
    return voice.ports[port];
}

static void SetMatrixLevels(Ngs2Voice& voice, std::uint32_t matrixId, const float* levels, std::uint32_t numLevels) {
    if (numLevels == 0 && voice.channels == 0 && matrixId < voice.matrices.size()) {
        voice.matrices[matrixId].clear();
        return;
    }
    if (matrixId >= voice.matrices.size() || numLevels == 0 || numLevels > NGS2_MAX_CHANNELS * NGS2_MAX_CHANNELS || levels == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto& matrix = voice.matrices[matrixId];
    matrix.resize(numLevels);
    for (std::uint32_t i = 0; i < numLevels; i++) matrix[i] = std::clamp(levels[i], -MAX_MATRIX_LEVEL, MAX_MATRIX_LEVEL);
}

static void SetPortMatrix(Ngs2Voice& voice, std::uint32_t port, std::int32_t matrixId) {
    if (matrixId < -1 || matrixId >= static_cast<std::int32_t>(voice.matrices.size())) APS5_INVALID_ARG_EX;
    PortAt(voice, port).matrix = matrixId;
}

static Ngs2MatrixControl CaptureMatrixControl(const Ngs2Voice& voice, std::uint32_t index, const float* levels, std::uint32_t count) {
    if (index >= voice.matrices.size() || count == 0 || count > NGS2_MAX_CHANNELS * NGS2_MAX_CHANNELS || levels == nullptr) APS5_INVALID_ARG_EX;
    Ngs2MatrixControl result{index, {levels, levels + count}};
    for (auto& level : result.levels) level = std::clamp(level, -MAX_MATRIX_LEVEL, MAX_MATRIX_LEVEL);
    return result;
}

static int ControlUserFx2(Ngs2Voice& voice, std::uint32_t index, const Ngs2CustomVoiceUserFx2Param& value) {
    auto& fx = voice.userFx[index];
    const auto& module = voice.rack->userFx[index];
    if (value.data == nullptr && value.data_size != 0) APS5_INVALID_ARG_EX;
    if (!fx.configured) fx.controlParam = fx.param;
    fx.configured = true;
    Ngs2UserFx2ControlContext context{value.data, value.data_size, voice.rack->userFxCommon[index].data(), fx.controlParam.data(), module.user_data, {}};
    const Ngs2ControlFrame frame(voice);
    int result;
    {
        const Ngs2Unlocked unlocked;
        result = module.control_handler(&context);
    }
    if (result < 0) return result;
    if (DeferringControls(voice)) EnqueueControl(voice, Ngs2UserFxControl{index, fx.controlParam});
    else {
        std::copy(fx.controlParam.begin(), fx.controlParam.end(), fx.param.begin());
        fx.active = true;
        fx.flags |= 2;
    }
    return SCE_NGS2_OK;
}

static Ngs2PendingControl CaptureParamControl(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param) {
    if (param.id == SCE_NGS2_VOICE_PARAM_EVENT) {
        const auto event = ParamAs<Ngs2VoiceEventParam>(param).event_id;
        ValidateDeferredEvent(voice, event);
        return event;
    }
    if (param.id == SCE_NGS2_VOICE_PARAM_PORT_VOLUME) {
        const auto value = ParamAs<Ngs2VoicePortVolumeParam>(param);
        PortAt(voice, value.port);
        return value;
    }
    if (param.id == SCE_NGS2_VOICE_PARAM_PORT_MATRIX) {
        const auto value = ParamAs<Ngs2VoicePortMatrixParam>(param);
        PortAt(voice, value.port);
        if (value.matrix_id < -1 || value.matrix_id >= static_cast<std::int32_t>(voice.matrices.size())) APS5_INVALID_ARG_EX;
        return value;
    }
    if (param.id == SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS) {
        const auto& value = ParamAs<Ngs2VoiceMatrixLevelsParam>(param);
        return CaptureMatrixControl(voice, value.matrix_id, value.levels, value.num_levels);
    }
    if (voice.IsSampler() && (param.id == SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH || param.id == SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_PITCH) && (param.id >> 16) == voice.rack->rackId) {
        auto value = ParamAs<Ngs2SamplerVoicePitchParam>(param);
        if (std::isnan(value.ratio)) APS5_INVALID_ARG_EX;
        value.ratio = std::clamp(value.ratio, 0.0f, 4.0f);
        return value;
    }
    if ((voice.rack->rackId == SCE_NGS2_RACK_ID_MASTERING || voice.rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_MASTERING) &&
        (param.id == SCE_NGS2_MASTERING_VOICE_PARAM_GAIN || param.id == SCE_NGS2_CUSTOM_MASTERING_VOICE_PARAM_GAIN) && (param.id >> 16) == voice.rack->rackId) {
        auto value = ParamAs<Ngs2MasteringVoiceGainParam>(param);
        value.fbw_level = std::clamp(value.fbw_level, -MAX_MASTERING_GAIN, MAX_MASTERING_GAIN);
        value.lfe_level = std::clamp(value.lfe_level, -MAX_MASTERING_GAIN, MAX_MASTERING_GAIN);
        return value;
    }
    if ((param.id & ~0xffu) == SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT) {
        const auto index = param.id & 0x1fu;
        if (index >= voice.customPitchShift.size() || voice.rack->customModuleIds[index] != SCE_NGS2_CUSTOM_MODULE_ID_PITCH_SHIFT) APS5_INVALID_ARG_EX;
        return ParamAs<Ngs2CustomVoicePitchShiftParam>(param);
    }
    if (voice.rack->rackId == SCE_NGS2_RACK_ID_SUBMIXER && param.id == SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX)
        return ParamAs<Ngs2SubmixerVoiceUserFxParam>(param);
    if (voice.rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER && (param.id & ~0x1fu) == SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX) {
        const auto index = param.id & 0x1fu;
        if (index >= voice.customUserFx.size() || voice.rack->customModuleIds[index] != SCE_NGS2_CUSTOM_MODULE_ID_USER_FX) APS5_INVALID_ARG_EX;
        return ParamAs<Ngs2CustomVoiceUserFxParam>(param);
    }
    if ((param.id & ~0x1fu) == SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2) {
        const auto index = param.id & 0x1fu;
        if (index >= voice.userFx.size() || voice.rack->customModuleIds[index] != SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2) APS5_INVALID_ARG_EX;
        if (voice.rack->userFx[index].control_handler != nullptr) throw std::runtime_error("NGS2: UserFx2 control handlers require immediate dispatch");
        const auto& value = ParamAs<Ngs2CustomVoiceUserFx2Param>(param);
        if (value.data_size != voice.userFx[index].param.size() || (value.data == nullptr && value.data_size != 0)) APS5_INVALID_ARG_EX;
        Ngs2UserFxControl result{index, {}};
        if (value.data_size != 0) {
            const auto* data = static_cast<const std::uint8_t*>(value.data);
            result.param.assign(data, data + value.data_size);
        }
        voice.userFx[index].controlParam = result.param;
        voice.userFx[index].configured = true;
        return result;
    }
    throw std::runtime_error("NGS2: this buffered voice parameter is not implemented");
}

static Ngs2PendingControl CaptureCommandControl(Ngs2Voice& voice, const Ngs2VoiceCommand& command) {
    const auto id = command.id & 0xffffffu;
    const auto index = command.id >> 24;
    if (id == COMMAND_EVENT) {
        ValidateDeferredEvent(voice, command.value.u);
        return command.value.u;
    }
    if (id == COMMAND_PORT_VOLUME && command.type == COMMAND_TYPE_FLOAT) {
        PortAt(voice, index);
        return Ngs2VoicePortVolumeParam{{}, index, command.value.f};
    }
    if (id == COMMAND_PORT_MATRIX && command.type == COMMAND_TYPE_INT) {
        PortAt(voice, index);
        if (command.value.i < -1 || command.value.i >= static_cast<std::int32_t>(voice.matrices.size())) APS5_INVALID_ARG_EX;
        return Ngs2VoicePortMatrixParam{{}, index, command.value.i};
    }
    if (id == COMMAND_MATRIX_LEVELS && command.type == COMMAND_TYPE_FLOAT_ARRAY)
        return CaptureMatrixControl(voice, index, command.value.levels, command.count);
    throw std::runtime_error("NGS2: this buffered voice command is not implemented");
}

void Ngs2Voice::ApplyPendingControls() {
    Ngs2PrepareControlMix(*this);
    bool restart = false, stop = false, kill = false;
    std::uint32_t pause = 0;
    for (auto& control : pendingControls) std::visit([&](auto& value) {
        using TValue = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<TValue, std::uint32_t>) {
            if (value == SCE_NGS2_VOICE_EVENT_PLAY) {
                restart = true;
                stop = kill = false;
                pause = 0;
            } else if (value == SCE_NGS2_VOICE_EVENT_KILL) {
                restart = stop = false;
                kill = true;
                pause = 0;
            } else if (value == SCE_NGS2_VOICE_EVENT_STOP || value == SCE_NGS2_VOICE_EVENT_STOP_IMM) stop = true;
            else pause = value;
        } else if constexpr (std::is_same_v<TValue, Ngs2VoicePortVolumeParam>) ports[value.port].volume = value.level;
        else if constexpr (std::is_same_v<TValue, Ngs2VoicePortMatrixParam>) ports[value.port].matrix = value.matrix_id;
        else if constexpr (std::is_same_v<TValue, Ngs2MatrixControl>) matrices[value.index] = std::move(value.levels);
        else if constexpr (std::is_same_v<TValue, Ngs2SamplerVoicePitchParam>) pitch = value.ratio;
        else if constexpr (std::is_same_v<TValue, Ngs2MasteringVoiceGainParam>) { fbwLevel = value.fbw_level; lfeLevel = value.lfe_level; }
        else if constexpr (std::is_same_v<TValue, Ngs2SubmixerVoiceUserFxParam>) {
            userFxHandler = value.handler;
            userFxData = {value.user_data0, value.user_data1, value.user_data2};
            userFxFlags = value.handler ? 2 : 0;
        } else if constexpr (std::is_same_v<TValue, Ngs2UserFxControl>) {
            std::copy(value.param.begin(), value.param.end(), userFx[value.index].param.begin());
            userFx[value.index].active = true;
            userFx[value.index].flags |= 2;
        } else Ngs2ApplyCustomParam(*this, value.header);
    }, control);
    if (kill) {
        SetEvent(SCE_NGS2_VOICE_EVENT_KILL);
        outputTransition = Ngs2VoiceTransition::Kill;
    }
    else {
        if (restart) {
            if (state != Ngs2PlayState::Empty) state = Ngs2PlayState::Stopped;
            SetEvent(SCE_NGS2_VOICE_EVENT_PLAY);
        }
        if (stop) SetEvent(SCE_NGS2_VOICE_EVENT_STOP);
        else if (pause != 0) SetEvent(pause);
        if (restart && pause == SCE_NGS2_VOICE_EVENT_PAUSE && !stop) {
            outputTransition = Ngs2VoiceTransition::None;
            stateFlags = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PAUSED;
        }
    }
    pendingControls.clear();
    pendingPlay = false;
    KillForRender(false);
}

static void Patch(Ngs2Voice& voice, const Ngs2VoicePatchParam& patch) {
    auto& port = PortAt(voice, patch.port);
    if (patch.dest_handle == 0) {
        if (patch.dest_input_id != 0) throw std::runtime_error("NGS2: a disconnected port with a nonzero input id is not implemented");
        port.dest = nullptr;
        port.detachedDest = 0;
        port.destInput = 0;
        return;
    }
    auto* dest = Ngs2FindVoice(patch.dest_handle);
    if (dest == nullptr) {
        if (patch.dest_input_id != 0) throw std::runtime_error("NGS2: a rack destination with a nonzero input id is not implemented");
        auto* rack = Ngs2FindRack(patch.dest_handle);
        if (rack != nullptr && !rack->voices.empty()) dest = &rack->voices.front();
    }
    if (dest == nullptr || dest->rack->system != voice.rack->system || dest->rack->rackId == SCE_NGS2_RACK_ID_SAMPLER ||
        dest->rack->rackId == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER) {
        APS5_INVALID_ARG_EX;
    }
    if (patch.dest_input_id != 0 &&
        (dest->rack->rackId != SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER || patch.dest_input_id >= dest->rack->maxInputs)) {
        throw std::runtime_error("NGS2: voice input " + std::to_string(patch.dest_input_id) + " is not implemented for this rack");
    }
    port.dest = dest;
    port.detachedDest = 0;
    port.destInput = patch.dest_input_id;
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
        case SCE_NGS2_VOICE_PARAM_EVENT: SendEvent(voice, ParamAs<Ngs2VoiceEventParam>(param).event_id); break;
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
    if (format.waveform_type == 0 && format.num_channels == 0 && format.sample_rate == 0) {
        if (voice.channels == 0) voice.ResetSetup();
        return;
    }
    voice.ResetSetup();
    if (format.waveform_type != SCE_NGS2_WAVEFORM_TYPE_ATRAC9) Ngs2PcmSampleBytes(format.waveform_type);
    if (format.waveform_type != SCE_NGS2_WAVEFORM_TYPE_PCM_I16L && format.waveform_type != SCE_NGS2_WAVEFORM_TYPE_ATRAC9 && format.config_data != 0) {
        throw std::runtime_error("NGS2: configured PCM waveforms are not implemented");
    }
    if (format.frame_offset != 0 || format.frame_margin != 0) throw std::runtime_error("NGS2: waveform frame offset and margin are not implemented");
    if (format.num_channels == 0 || format.num_channels > NGS2_MAX_CHANNELS || format.sample_rate == 0) APS5_INVALID_ARG_EX;
    voice.channels = format.num_channels;
    voice.sampleRate = format.sample_rate;
    voice.waveformType = format.waveform_type;
    if (format.waveform_type == SCE_NGS2_WAVEFORM_TYPE_ATRAC9) Ngs2SetupAtrac9(voice, format);
    Ngs2PrepareSampler(voice);
}

static void AppendStreamData(Ngs2Voice& voice, const Ngs2SamplerVoiceWaveformBlocksParam& param, std::size_t frameBytes) {
    const bool compressed = voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9;
    const bool pending = !voice.blocks.empty() || (compressed ? voice.atrac9.remainingSamples != 0 : voice.pcmRemaining != 0);
    if (!pending) throw std::invalid_argument("NGS2: appending data requires a waveform");
    for (std::uint32_t i = 0; i < param.num_blocks; ++i) {
        auto info = param.blocks[i];
        if (info.data_size == 0) continue;
        if (!compressed && info.data_size % frameBytes != 0) throw std::runtime_error("NGS2: appending a partial PCM frame is not implemented");
        const auto* data = static_cast<const std::uint8_t*>(param.data) + info.data_offset;
        if (!voice.blocks.empty() && voice.blocks.back().data + voice.blocks.back().info.data_size == data && voice.blocks.back().info.user_data == info.user_data) {
            voice.blocks.back().info.data_size += info.data_size;
            if (!voice.samplerReplayBlocks.empty()) {
                const auto last = voice.samplerWritePosition == 0 ? voice.samplerReplayBlocks.size() - 1 : voice.samplerWritePosition - 1;
                voice.samplerReplayBlocks[last].info.data_size += info.data_size;
            }
            continue;
        }
        info.num_repeats = 0;
        info.num_skip_samples = 0;
        info.num_samples = 0;
        voice.blocks.push_back({data, info});
        Ngs2SaveSamplerBlock(voice, voice.blocks.back());
    }
}

static void AddWaveformBlocks(Ngs2Voice& voice, const Ngs2SamplerVoiceWaveformBlocksParam& param) {
    constexpr std::uint32_t knownFlags = SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET | SCE_NGS2_WAVEFORM_BLOCKS_FLAG_SILENCE;
    if ((param.flags & ~knownFlags) != 0) throw std::runtime_error("NGS2: waveform block flags " + Ngs2Hex(param.flags) + " are not implemented");
    if ((param.num_blocks != 0 && (voice.channels == 0 || param.blocks == nullptr || param.data == nullptr)) || (voice.channels == 0 && (param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND) != 0)) APS5_INVALID_ARG_EX;
    const bool reset = (param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET) != 0;
    const auto pcmCursor = reset && !voice.blocks.empty() ? voice.blocks.front().dataCursor : 0u;
    const bool preservePcm = reset && voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_PCM_I16L && !voice.blocks.empty() && voice.blocks.front().started;
    const bool preserveCodec = reset && voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9 && ((!voice.blocks.empty() && voice.blocks.front().started) || voice.atrac9.remainingSamples != 0);
    std::uint64_t codecCursor = 0;
    if (preserveCodec && !voice.blocks.empty()) codecCursor = voice.blocks.front().dataCursor;
    if (!voice.acceptsBlocks && !reset) throw std::invalid_argument("NGS2: the voice waveform was already closed");
    if (reset) {
        ++voice.waveformRevision;
        voice.blocks.clear();
        voice.samplerReplayBlocks.clear();
        voice.samplerWritePosition = 0;
    }
    const std::size_t frameBytes = voice.channels == 0 || voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9 ? 0 : voice.channels * Ngs2PcmSampleBytes(voice.waveformType);
    if ((param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND) != 0) {
        AppendStreamData(voice, param, frameBytes);
        voice.acceptsBlocks = (param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE) != 0;
        return;
    }
    voice.acceptsBlocks = (param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE) != 0;
    const bool allowsSilence = (param.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_SILENCE) != 0;
    for (std::uint32_t i = 0; i < param.num_blocks; i++) {
        const auto& block = param.blocks[i];
        if (block.num_samples == 0 && block.data_size == 0) continue;
        if (allowsSilence && block.data_size == 0) {
            if (voice.waveformType != SCE_NGS2_WAVEFORM_TYPE_PCM_I16L) throw std::runtime_error("NGS2: a dataless waveform block under the silence flag is not implemented for this format");
            voice.blocks.push_back({nullptr, block});
            Ngs2SaveSamplerBlock(voice, voice.blocks.back());
            continue;
        }
        const std::uint64_t bytes = voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9 ? Ngs2Atrac9BlockBytes(voice, block)
                                  : (static_cast<std::uint64_t>(block.num_skip_samples) + block.num_samples) * frameBytes;
        const bool streaming = block.num_samples != 0 && bytes > block.data_size && voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_PCM_I16L &&
                               block.num_skip_samples == 0 && block.num_repeats == 0 && block.data_size >= frameBytes && block.data_size % frameBytes == 0;
        const bool compressedStream = voice.waveformType == SCE_NGS2_WAVEFORM_TYPE_ATRAC9 && voice.acceptsBlocks && block.num_repeats == 0;
        if (block.num_samples == 0 || (bytes > block.data_size && !streaming && !compressedStream)) {
            throw std::invalid_argument("NGS2: waveform block " + std::to_string(i) + " does not fit its data");
        }
        voice.blocks.push_back({static_cast<const std::uint8_t*>(param.data) + block.data_offset, block});
        Ngs2SaveSamplerBlock(voice, voice.blocks.back());
    }
    if (preservePcm && !voice.blocks.empty()) {
        auto& first = voice.blocks.front();
        first.started = true;
        first.dataCursor = pcmCursor;
    }
    if (preserveCodec && !voice.blocks.empty()) {
        voice.blocks.front().dataCursor = codecCursor;
        voice.blocks.front().started = true;
    }
}

void Ngs2SetFilter(Ngs2Voice& voice, const Ngs2SamplerVoiceFilterParam& param) {
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
    filter.parameter = param;
    filter.pending = voice.rack->system == nullptr;
    filter.enabled = true;
    filter.bypassMask = param.channel_mask;
    filter.history.resize(voice.channels);
    if (filter.pending) return;
    const double rate = voice.rack->system->option.sample_rate;
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

void Ngs2PublishSamplerState(Ngs2Voice& voice) {
    auto& state = voice.publishedSampler;
    state = {};
    state.voice_state.state_flags = voice.stateFlags;
    if (voice.state == Ngs2PlayState::Empty) return;
    state.envelope_height = 1.0f;
    state.peak_height = voice.peakHeight;
    state.num_decoded_samples = voice.decodedSamples;
    state.decoded_data_size = voice.decodedBytes;
    state.user_data = voice.waveformUserData;
    state.waveform_data = voice.WaveformData();
}

static void SetPeakMeter(Ngs2Voice& voice, std::uint32_t detectType) {
    if (detectType > 2) throw Ngs2ReturnCode{SCE_NGS2_ERROR_INVALID_PEAK_METER_MODE};
    if (voice.peakMeterMode != detectType) {
        voice.peakMeterHistory.clear();
        voice.peakMeterCount = 0;
        voice.peakMeterCursor = 0;
        voice.peakHeight = 0.0f;
    }
    voice.peakMeterMode = detectType;
}

static void ApplySamplerParam(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param) {
    switch (param.id) {
        case SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP:
        case SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_SETUP: {
            const auto& setup = ParamAs<Ngs2SamplerVoiceSetupParam>(param);
            SetupSampler(voice, setup.format);
            break;
        }
        case SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS:
        case SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS:
            AddWaveformBlocks(voice, ParamAs<Ngs2SamplerVoiceWaveformBlocksParam>(param));
            break;
        case SCE_NGS2_SAMPLER_VOICE_PARAM_EXIT_LOOP:
        case SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_EXIT_LOOP: {
            const auto looping = std::find_if(voice.blocks.begin(), voice.blocks.end(), [](const Ngs2Block& block) { return block.info.num_repeats != 0; });
            if (looping != voice.blocks.end()) looping->info.num_repeats = 0;
            break;
        }
        case SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH:
        case SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_PITCH: {
            const float ratio = ParamAs<Ngs2SamplerVoicePitchParam>(param).ratio;
            if (std::isnan(ratio)) APS5_INVALID_ARG_EX;
            voice.pitch = std::clamp(ratio, 0.0f, 4.0f);
            break;
        }
        case SCE_NGS2_SAMPLER_VOICE_PARAM_FILTER: Ngs2SetFilter(voice, ParamAs<Ngs2SamplerVoiceFilterParam>(param)); break;
        case SCE_NGS2_SAMPLER_VOICE_PARAM_PEAK_METER: SetPeakMeter(voice, ParamAs<Ngs2VoicePeakMeterParam>(param).detect_type); break;
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
        case SCE_NGS2_RACK_ID_SAMPLER:
        case SCE_NGS2_RACK_ID_CUSTOM_SAMPLER: ApplySamplerParam(voice, param); return;
        case SCE_NGS2_RACK_ID_SUBMIXER:
            if (param.id == SCE_NGS2_SUBMIXER_VOICE_PARAM_PEAK_METER) {
                SetPeakMeter(voice, ParamAs<Ngs2VoicePeakMeterParam>(param).detect_type);
                return;
            }
            if (param.id == SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX) {
                const auto& fx = ParamAs<Ngs2SubmixerVoiceUserFxParam>(param);
                voice.userFxHandler = fx.handler;
                voice.userFxData = {fx.user_data0, fx.user_data1, fx.user_data2};
                voice.userFxFlags = fx.handler ? voice.userFxFlags | 2u : 0;
                return;
            }
            if (param.id != SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP) break;
            if (ParamAs<Ngs2SubmixerVoiceSetupParam>(param).flags != 0) throw std::runtime_error("NGS2: submixer setup flags are not implemented");
            SetupMixer(voice, ParamAs<Ngs2SubmixerVoiceSetupParam>(param).num_io_channels);
            return;
        case SCE_NGS2_RACK_ID_REVERB:
            if (param.id == SCE_NGS2_REVERB_VOICE_PARAM_SETUP) {
                const auto& setup = ParamAs<Ngs2ReverbVoiceSetupParam>(param);
                if (setup.flags != 0) throw std::runtime_error("NGS2: reverb setup flags are not implemented");
                for (const auto count : {setup.num_input_channels, setup.num_output_channels}) {
                    if ((count != 1 && count != 2 && count != 6 && count != 8) || count > voice.rack->maxChannels) APS5_INVALID_ARG_EX;
                }
                SetupMixer(voice, setup.num_output_channels);
                voice.inputChannels = setup.num_input_channels;
                Ngs2SetupReverb(voice);
                return;
            }
            if (param.id == SCE_NGS2_REVERB_VOICE_PARAM_I3DL2) {
                Ngs2SetReverbParams(voice, ParamAs<Ngs2ReverbVoiceI3DL2Param>(param).i3dl2);
                return;
            }
            break;
        case SCE_NGS2_RACK_ID_MASTERING:
        case SCE_NGS2_RACK_ID_CUSTOM_MASTERING:
            if (param.id == SCE_NGS2_MASTERING_VOICE_PARAM_SETUP || param.id == SCE_NGS2_CUSTOM_MASTERING_VOICE_PARAM_SETUP) {
                SetupMixer(voice, ParamAs<Ngs2MasteringVoiceSetupParam>(param).num_io_channels);
                return;
            }
            if (param.id == SCE_NGS2_MASTERING_VOICE_PARAM_GAIN || param.id == SCE_NGS2_CUSTOM_MASTERING_VOICE_PARAM_GAIN) {
                const auto& gain = ParamAs<Ngs2MasteringVoiceGainParam>(param);
                voice.fbwLevel = std::clamp(gain.fbw_level, -MAX_MASTERING_GAIN, MAX_MASTERING_GAIN);
                voice.lfeLevel = std::clamp(gain.lfe_level, -MAX_MASTERING_GAIN, MAX_MASTERING_GAIN);
                return;
            }
            if (param.id == SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT || param.id == SCE_NGS2_CUSTOM_MASTERING_VOICE_PARAM_OUTPUT) {
                voice.outputId = ParamAs<Ngs2MasteringVoiceOutputParam>(param).output_id;
                return;
            }
            break;
        default: break;
    }
    throw std::runtime_error("NGS2: voice param " + Ngs2Hex(param.id) + " is not implemented");
}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceNgs2VoiceControl(uintptr_t voice_handle, const Ngs2VoiceParamHeader* param_list) {
    if (param_list == nullptr) return SCE_NGS2_ERROR_NULL_PARAM_LIST;
    const Ngs2Lock lock;
    const Ngs2VoiceAccess access(voice_handle);
    auto* found = access.voice;
    if (found == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    auto& voice = *found;
    if (voice.rack->system == nullptr && !voice.pendingControls.empty()) voice.ApplyPendingControls();
    try {
        for (const auto* param = param_list;; param = reinterpret_cast<const Ngs2VoiceParamHeader*>(reinterpret_cast<const std::uint8_t*>(param) + param->next)) {
            if ((param->id & ~0xffu) == SCE_NGS2_CUSTOM_VOICE_PARAM_PITCH_SHIFT) {
                const auto index = param->id & 0x1fu;
                if (index >= voice.customPitchShift.size() || voice.rack->customModuleIds[index] != SCE_NGS2_CUSTOM_MODULE_ID_PITCH_SHIFT) {
                    return SCE_NGS2_ERROR_INVALID_PARAM_ID;
                }
            }
            if ((param->id & ~0x1fu) == SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2) {
                const auto index = param->id & 0x1fu;
                if (index < voice.userFx.size() && voice.rack->customModuleIds[index] == SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2 && voice.rack->userFx[index].control_handler != nullptr) {
                    const int result = ControlUserFx2(voice, index, ParamAs<Ngs2CustomVoiceUserFx2Param>(*param));
                    if (result < 0) return result;
                    if (param->next == 0) break;
                    continue;
                }
            }
            if (DeferringControls(voice) && param->id != SCE_NGS2_VOICE_PARAM_CALLBACK) {
                EnqueueControl(voice, CaptureParamControl(voice, *param));
                if (param->next == 0) break;
                continue;
            }
            if (voice.channels != 0 && (param->id >> 16) == voice.rack->rackId && (param->id == SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS || param->id == SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS)) {
                const auto& blocks = ParamAs<Ngs2SamplerVoiceWaveformBlocksParam>(*param);
                const bool reset = (blocks.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET) != 0;
                if (!voice.acceptsBlocks && !reset) return static_cast<int>(0x804A8054u);
                std::size_t size = reset ? 0 : voice.blocks.size();
                const auto* end = size != 0 && voice.blocks.back().data != nullptr ? voice.blocks.back().data + voice.blocks.back().info.data_size : nullptr;
                auto lastUserData = size != 0 ? voice.blocks.back().info.user_data : 0;
                for (std::uint32_t i = 0; blocks.blocks != nullptr && i < blocks.num_blocks; ++i) {
                    const auto& block = blocks.blocks[i];
                    const auto* data = blocks.data == nullptr ? nullptr : static_cast<const std::uint8_t*>(blocks.data) + block.data_offset;
                    if (block.data_size == 0 && block.num_samples == 0) continue;
                    if ((blocks.flags & SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND) == 0 || data != end || block.user_data != lastUserData) {
                        ++size;
                    }
                    end = data == nullptr ? nullptr : data + block.data_size;
                    lastUserData = block.user_data;
                }
                if (size > std::max(1u, voice.rack->maxWaveformBlocks)) return static_cast<int>(0x804A8054u);
            }
            if ((param->id & ~0xffu) == SCE_NGS2_CUSTOM_VOICE_PARAM_GAIN) {
                const auto result = Ngs2ApplyCustomGainParam(voice, *param);
                if (result != SCE_NGS2_OK) return result;
            } else ApplyParam(voice, *param);
            if (param->next == 0) break;
        }
    } catch (const Ngs2ReturnCode& result) {
        return result.code;
    }
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2VoiceRunCommands(uintptr_t voice_handle, const Ngs2VoiceCommand* commands, size_t num_commands) {
    const Ngs2Lock lock;
    const Ngs2VoiceAccess access(voice_handle);
    auto* found = access.voice;
    if (found == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    auto& voice = *found;
    if (voice.rack->system == nullptr && !voice.pendingControls.empty()) voice.ApplyPendingControls();
    try {
        if (commands == nullptr && num_commands != 0) APS5_INVALID_ARG_EX;
        if (DeferringControls(voice)) {
            if (num_commands > voice.pendingControls.max_size() - voice.pendingControls.size()) throw std::length_error("NGS2: too many buffered voice commands");
            std::vector<Ngs2PendingControl> captured;
            captured.reserve(num_commands);
            for (std::size_t i = 0; i < num_commands; ++i) captured.push_back(CaptureCommandControl(voice, commands[i]));
            voice.pendingControls.reserve(voice.pendingControls.size() + captured.size());
            static_assert(std::is_nothrow_move_constructible_v<Ngs2PendingControl>);
            for (auto& control : captured) EnqueueControl(voice, std::move(control));
            return SCE_NGS2_OK;
        }
        for (std::size_t i = 0; i < num_commands; i++) {
            const auto& command = commands[i];
            const std::uint32_t index = command.id >> 24;
            const std::uint32_t id = command.id & 0xffffff;
            if (id == COMMAND_EVENT) {
                SendEvent(voice, command.value.u);
                continue;
            }
            const std::uint8_t expectedType = id == COMMAND_MATRIX_LEVELS ? COMMAND_TYPE_FLOAT_ARRAY
                                            : id == COMMAND_PORT_VOLUME ? COMMAND_TYPE_FLOAT
                                            : id == COMMAND_PORT_MATRIX ? COMMAND_TYPE_INT : 0;
            if (expectedType == 0) throw std::runtime_error("NGS2: voice command " + Ngs2Hex(command.id) + " is not implemented");
            if (command.type != expectedType) throw std::invalid_argument("NGS2: voice command " + Ngs2Hex(command.id) + " has value type " + Ngs2Hex(command.type));
            if (id == COMMAND_MATRIX_LEVELS) SetMatrixLevels(voice, index, command.value.levels, command.count);
            else if (id == COMMAND_PORT_VOLUME) PortAt(voice, index).volume = command.value.f;
            else SetPortMatrix(voice, index, command.value.i);
        }
    } catch (const Ngs2ReturnCode& result) {
        return result.code;
    }
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2VoiceGetState(uintptr_t voice_handle, Ngs2VoiceState* state, size_t state_size) {
    const Ngs2Lock lock;
    const Ngs2VoiceAccess access(voice_handle);
    auto* found = access.voice;
    if (found == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    if (state_size < sizeof(Ngs2VoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    if (state == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    const auto& voice = *found;
    switch (voice.rack->rackId) {
        case SCE_NGS2_RACK_ID_SAMPLER: {
            if (state_size != sizeof(Ngs2SamplerVoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
            auto& sampler = *reinterpret_cast<Ngs2SamplerVoiceState*>(state);
            if (voice.pendingPlay) { sampler = {}; sampler.voice_state.state_flags = SCE_NGS2_VOICE_STATE_FLAG_INUSE; return SCE_NGS2_OK; }
            if (voice.rack->system != nullptr && voice.rack->system->renderDepth != 0 && voice.rack->system->jobManagers[0].handle != 0) {
                sampler = voice.publishedSampler;
                return SCE_NGS2_OK;
            }
            sampler = {};
            sampler.voice_state.state_flags = voice.stateFlags;
            if (voice.state == Ngs2PlayState::Empty) return SCE_NGS2_OK;
            sampler.envelope_height = 1.0f;
            sampler.peak_height = voice.peakHeight;
            sampler.num_decoded_samples = voice.decodedSamples;
            sampler.decoded_data_size = voice.decodedBytes;
            sampler.user_data = voice.waveformUserData;
            sampler.waveform_data = voice.WaveformData();
            return SCE_NGS2_OK;
        }
        case SCE_NGS2_RACK_ID_REVERB: {
            if (state_size != sizeof(Ngs2VoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
            *state = {};
            state->state_flags = voice.stateFlags;
            return SCE_NGS2_OK;
        }
        case SCE_NGS2_RACK_ID_SUBMIXER: {
            if (state_size != sizeof(Ngs2SubmixerVoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
            auto& submixer = *reinterpret_cast<Ngs2SubmixerVoiceState*>(state);
            submixer = {};
            submixer.voice_state.state_flags = voice.stateFlags;
            submixer.peak_height = voice.peakHeight;
            return SCE_NGS2_OK;
        }
        case SCE_NGS2_RACK_ID_CUSTOM_SAMPLER: {
            if (state_size != sizeof(Ngs2CustomSamplerVoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
            auto& sampler = *reinterpret_cast<Ngs2CustomSamplerVoiceState*>(state);
            if (voice.pendingPlay) { sampler = {}; sampler.voice_state.state_flags = SCE_NGS2_VOICE_STATE_FLAG_INUSE; return SCE_NGS2_OK; }
            sampler = {};
            sampler.voice_state.state_flags = voice.stateFlags;
            if ((voice.rack->system == nullptr || voice.rack->system->renderDepth == 0) && voice.state == Ngs2PlayState::Empty) return SCE_NGS2_OK;
            sampler.num_decoded_samples = voice.rack->system != nullptr && voice.rack->system->renderDepth != 0 && voice.rack->system->jobManagers[0].handle != 0
                ? voice.publishedSampler.num_decoded_samples : voice.decodedSamples;
            return SCE_NGS2_OK;
        }
        case SCE_NGS2_RACK_ID_CUSTOM_MASTERING: {
            if (state_size != sizeof(Ngs2VoiceState)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
            *state = {};
            state->state_flags = voice.stateFlags;
            return SCE_NGS2_OK;
        }
        default: throw std::runtime_error("NGS2: voice state of rack " + Ngs2Hex(voice.rack->rackId) + " is not implemented");
    }
}

int APS5_VABI sceNgs2VoiceGetPortInfo(uintptr_t voice_handle, uint32_t port, Ngs2VoicePortInfo* info, size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size < sizeof(Ngs2VoicePortInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    const Ngs2Lock lock;
    const Ngs2VoiceAccess access(voice_handle);
    auto* found = access.voice;
    if (found == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    auto& voice = *found;
    const auto& source = PortAt(voice, port);
    auto volume = source.volume;
    auto matrix = source.matrix;
    for (const auto& control : voice.pendingControls) {
        if (const auto* value = std::get_if<Ngs2VoicePortVolumeParam>(&control); value && value->port == port) volume = value->level;
        if (const auto* value = std::get_if<Ngs2VoicePortMatrixParam>(&control); value && value->port == port) matrix = value->matrix_id;
    }
    *info = {};
    info->matrix_id = matrix;
    info->volume = volume;
    info->dest_handle = source.dest == nullptr ? source.detachedDest : reinterpret_cast<Ngs2Handle>(source.dest);
    info->dest_input_id = source.destInput;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2VoiceQueryInfo(uintptr_t voice_handle, uint32_t info_id, void* info, size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    const Ngs2Lock lock;
    const Ngs2VoiceAccess access(voice_handle);
    auto* found = access.voice;
    if (found == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    const auto& voice = *found;
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
    const Ngs2Lock lock;
    const Ngs2VoiceAccess access(voice_handle);
    const auto* found = access.voice;
    if (found == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    *state_flags = found->pendingPlay ? SCE_NGS2_VOICE_STATE_FLAG_INUSE : found->stateFlags;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2VoiceGetOwner(uintptr_t voice_handle, uintptr_t* owner_rack, uint32_t* voice_index) {
    const Ngs2Lock lock;
    const Ngs2VoiceAccess access(voice_handle);
    const auto* found = access.voice;
    if (found == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    if (owner_rack == nullptr || voice_index == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *owner_rack = reinterpret_cast<Ngs2Handle>(found->rack);
    *voice_index = static_cast<uint32_t>(found - found->rack->voices.data());
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2VoiceGetMatrixInfo(uintptr_t voice_handle, uint32_t matrix_id, Ngs2VoiceMatrixInfo* info, size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size < sizeof(Ngs2VoiceMatrixInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    const Ngs2Lock lock;
    const Ngs2VoiceAccess access(voice_handle);
    const auto* found = access.voice;
    if (found == nullptr) return SCE_NGS2_ERROR_INVALID_VOICE_HANDLE;
    if (matrix_id >= found->matrices.size()) APS5_INVALID_ARG_EX;
    const auto* levels = &found->matrices[matrix_id];
    for (const auto& control : found->pendingControls)
        if (const auto* value = std::get_if<Ngs2MatrixControl>(&control); value && value->index == matrix_id) levels = &value->levels;
    if (levels->empty()) *info = {};
    info->num_levels = static_cast<std::uint32_t>(levels->size());
    std::copy(levels->begin(), levels->end(), info->levels);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2PanInit(Ngs2PanWork* work, const float* speaker_angles, float unit_angle, uint32_t num_speakers) {
    return Ngs2PanInit(work, speaker_angles, unit_angle, num_speakers);
}

int APS5_VABI sceNgs2PanGetVolumeMatrix(Ngs2PanWork* work, const Ngs2PanParam* params, uint32_t num_params, uint32_t matrix_format, float* out_volume_matrix) {
    return Ngs2PanGetVolumeMatrix(work, params, num_params, matrix_format, out_volume_matrix);
}

int APS5_VABI sceNgs2GeomApply(const Ngs2GeomListenerWork* listener, const Ngs2GeomSourceParam* source, Ngs2GeomAttribute* out_attrib, std::uint32_t flags) {
    return Ngs2GeomApply(listener, source, out_attrib, flags);
}

int APS5_VABI sceNgs2GeomResetListenerParam(Ngs2GeomListenerParam* out_listener_param) {
    return Ngs2GeomResetListenerParam(out_listener_param);
}

int APS5_VABI sceNgs2GeomCalcListener(const Ngs2GeomListenerParam* param, Ngs2GeomListenerWork* outWork, std::uint32_t flags) {
    return Ngs2GeomCalcListener(param, outWork, flags);
}

int APS5_VABI sceNgs2GeomResetSourceParam(Ngs2GeomSourceParam* out_source_param) {
    return Ngs2GeomResetSourceParam(out_source_param);
}

}

#pragma GCC visibility pop

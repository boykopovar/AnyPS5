#ifndef CORE_LIBS_PRX_LIBSCENGS2_SRC_NGS2INTERNAL_HPP
#define CORE_LIBS_PRX_LIBSCENGS2_SRC_NGS2INTERNAL_HPP

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"
#include "PitchShift.hpp"

static constexpr std::uint32_t NGS2_MAX_CHANNELS = 8;
static constexpr std::uint32_t MIN_GRAIN_SAMPLES = 64;

struct Ngs2System;
struct Ngs2Rack;

enum class Ngs2PlayState : std::uint32_t {
    Empty = 0,
    Playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING,
    Paused = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PAUSED,
    Stopped = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING | SCE_NGS2_VOICE_STATE_FLAG_STOPPED,
};

struct Ngs2Block {
    const std::uint8_t* data;
    Ngs2WaveformBlock info;
    std::uint32_t numRepeated = 0;
    std::size_t dataCursor = 0;
    bool started = false;
    std::uint32_t submittedRepeats = 0;
};

struct Ngs2Atrac9DecoderDeleter {
    void operator()(void* handle) const;
};

struct Ngs2Atrac9 {
    std::unique_ptr<void, Ngs2Atrac9DecoderDeleter> decoder;
    std::uint8_t config[4] = {};
    std::uint32_t frameSamples = 0;
    std::uint32_t framesInSuperframe = 0;
    std::uint32_t superframeBytes = 0;
    std::vector<float> window;
    std::uint32_t windowCursor = 0;
    std::vector<std::uint8_t> input;
    std::uint32_t remainingSamples = 0;
    std::uint32_t skipSamples = 0;
    std::uint32_t inputCursor = 0;
    std::uint32_t frameIndex = 0;
    bool restartOnDrain = false;
};

struct Ngs2ReverbState;

struct Ngs2ReverbDeleter {
    void operator()(Ngs2ReverbState* reverb) const;
};

struct Ngs2Voice;

struct Ngs2FilterHistory {
    double x1 = 0.0;
    double x2 = 0.0;
    double y1 = 0.0;
    double y2 = 0.0;
};

struct Ngs2Filter {
    bool enabled = false;
    bool pending = false;
    Ngs2SamplerVoiceFilterParam parameter{};
    std::uint64_t bypassMask = 0;
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    std::vector<Ngs2FilterHistory> history;
};

struct Ngs2Port {
    Ngs2Voice* dest = nullptr;
    Ngs2Handle detachedDest = 0;
    float volume = 1.0f;
    std::int32_t matrix = -1;
    std::vector<float> rampFrom;
    std::uint32_t destInput = 0;
};

struct Ngs2UserFx2 {
    std::vector<std::uint8_t> param;
    std::vector<std::uint8_t> controlParam;
    std::vector<std::uint8_t> initialParam;
    bool configured = false;
    bool active = false;
    std::vector<std::uint8_t> work;
    std::vector<std::uint8_t> state;
    std::uint32_t flags = 0;
};

struct Ngs2VoiceCustomUserFx {
    Ngs2UserFxProcessHandler handler = nullptr;
    std::array<std::uintptr_t, 3> data{};
    std::uint32_t flags = 0;
};

struct Ngs2VoiceCustomGain {
    std::array<float, SCE_NGS2_CUSTOM_SUBMIXER_MAX_CHANNELS> current{};
    std::array<float, SCE_NGS2_CUSTOM_SUBMIXER_MAX_CHANNELS> target{};
    bool configured = false;
    bool lifecycleUnknown = false;
};

struct Ngs2QueuedCallback {
    Ngs2VoiceCallbackInfo info;
    std::uint64_t sequence;
};

struct Ngs2MatrixControl {
    std::uint32_t index;
    std::vector<float> levels;
};

struct Ngs2UserFxControl {
    std::uint32_t index;
    std::vector<std::uint8_t> param;
};

using Ngs2PendingControl = std::variant<std::uint32_t, Ngs2VoicePortVolumeParam, Ngs2VoicePortMatrixParam,
    Ngs2MatrixControl, Ngs2SamplerVoicePitchParam, Ngs2MasteringVoiceGainParam, Ngs2CustomVoicePitchShiftParam, Ngs2SubmixerVoiceUserFxParam, Ngs2CustomVoiceUserFxParam, Ngs2UserFxControl>;

struct Ngs2Voice {
    Ngs2Rack* rack = nullptr;
    Ngs2PlayState state = Ngs2PlayState::Empty;
    std::uint32_t stateFlags = 0;
    Ngs2SamplerVoiceState publishedSampler{};
    std::uint32_t channels = 0;
    std::uint32_t inputChannels = 0;
    std::uint32_t sampleRate = 0;
    std::uint32_t waveformType = 0;
    Ngs2Atrac9 atrac9;
    std::unique_ptr<Ngs2ReverbState, Ngs2ReverbDeleter> reverb;
    float pitch = 1.0f;
    std::uint64_t phase = 0;
    std::array<std::array<float, 4>, NGS2_MAX_CHANNELS> samplerHistory{};
    std::uint32_t samplerCachedFrames = 4;
    bool samplerStarved = false;
    bool samplerEnded = false;
    std::uint32_t pcmRemaining = 0;
    std::vector<Ngs2QueuedCallback> pendingBlockCallbacks;
    std::vector<Ngs2QueuedCallback> dispatchBlockCallbacks;
    std::vector<float> samplerScratch;
    std::vector<Ngs2Block> samplerReplayBlocks;
    std::uint32_t samplerWritePosition = 0;
    std::deque<Ngs2Block> blocks;
    bool acceptsBlocks = true;
    std::vector<Ngs2PendingControl> pendingControls;
    bool pendingPlay = false;
    std::thread::id controlOwner{};
    alignas(std::atomic_ref<bool>::required_alignment) mutable bool killCurrent = false;
    std::uint64_t decodedSamples = 0;
    std::uint64_t decodedBytes = 0;
    std::uint64_t waveformRevision = 0;
    const std::uint8_t* waveformData = nullptr;
    std::uintptr_t waveformUserData = 0;
    Ngs2VoiceCallbackHandler callback = nullptr;
    std::uintptr_t callbackData = 0;
    std::uint32_t callbackFlags = 0;
    std::vector<Ngs2Port> ports;
    std::vector<std::vector<float>> matrices;
    std::vector<Ngs2Filter> filters;
    std::uint32_t outputId = 0;
    float fbwLevel = 1.0f;
    float lfeLevel = 1.0f;
    std::vector<Ngs2UserFx2> userFx;
    std::vector<Ngs2VoiceCustomUserFx> customUserFx;
    std::vector<Ngs2VoiceCustomGain> customGain;
    std::vector<Ngs2PitchShift> customPitchShift;
    Ngs2VoiceTransition outputTransition = Ngs2VoiceTransition::None;
    Ngs2UserFxProcessHandler userFxHandler = nullptr;
    std::array<std::uintptr_t, 3> userFxData{};
    std::uint32_t userFxFlags = 0;
    std::vector<float> samples;
    std::vector<std::vector<float>> customBuffers;
    std::vector<std::vector<float>> peakMeterHistory;
    std::uint32_t peakMeterMode = 0;
    std::uint32_t peakMeterCount = 0;
    std::uint32_t peakMeterCursor = 0;
    float peakHeight = 0.0f;
    bool rendering = false;
    bool rendered = false;
    bool hasSamples = false;

    void SetEvent(std::uint32_t eventId);
    void ApplyPendingControls();
    bool KilledForRender() const { return std::atomic_ref<bool>(killCurrent).load(std::memory_order_relaxed); }
    void KillForRender(bool killed) { std::atomic_ref<bool>(killCurrent).store(killed, std::memory_order_relaxed); }
    void ResetSetup();
    bool IsSampler() const;
    bool IsMixer() const;
    bool IsMastering() const;
    std::uint32_t UserFxFlags(std::uint32_t flags) const;
    const std::uint8_t* WaveformData() const;
    std::uint32_t InputChannels() const { return inputChannels != 0 ? inputChannels : channels; }
    std::vector<float>& Samples(std::uint32_t buffer) { return buffer == 0 ? samples : customBuffers.at(buffer - 1); }
    const std::vector<float>& Samples(std::uint32_t buffer) const { return buffer == 0 ? samples : customBuffers.at(buffer - 1); }
};

struct Ngs2Rack {
    Ngs2System* system = nullptr;
    std::uint32_t rackId = 0;
    std::uint32_t uid = 0;
    char name[64] = {};
    std::uint32_t maxChannels = 0;
    std::uint32_t maxFilters = 0;
    std::uint32_t maxGrainSamples = 0;
    std::uint32_t maxChannelWorks = 0;
    std::uint32_t maxWaveformBlocks = 0;
    std::uint32_t maxInputs = 0;
    std::uint32_t peakMeterBlocks = 8;
    std::uintptr_t userData = 0;
    Ngs2ContextBufferInfo bufferInfo{};
    Ngs2BufferAllocator allocator{};
    std::uint32_t maxVoiceChannels = 8;
    std::uint32_t maxOutputChannels = 8;
    std::uint32_t numModules = 0;
    std::uint32_t numBuffers = 1;
    std::uint64_t renderCount = 0;
    std::vector<std::uint8_t> infoExtension;
    std::vector<std::vector<std::uint8_t>> customModuleOptions;
    std::vector<Ngs2Voice> voices;
    std::vector<Ngs2CustomUserFx2ModuleOption> userFx;
    std::vector<std::vector<std::uint8_t>> userFxCommon;
    std::vector<std::vector<std::uint8_t>> userFxParam;
    std::vector<std::uint32_t> customModuleIds;
    std::vector<Ngs2CustomModuleInfo> customModules;
    std::vector<std::uint32_t> customPortBuffers;
    std::thread::id lockOwner{};
    std::uint32_t lockDepth = 0;
    std::uint32_t voiceAccesses = 0;
    bool destroying = false;
};

struct Ngs2JobManager {
    Ngs2JobSchedulerOption option{};
    std::uintptr_t handle = 0;
};

struct Ngs2System {
    Ngs2SystemOption option{};
    std::array<Ngs2JobManager, 4> jobManagers{};
    Ngs2ContextBufferInfo bufferInfo{};
    Ngs2BufferAllocator allocator{};
    std::uint32_t uid = 0;
    std::int64_t renderCount = 0;
    std::uint32_t nextRackUid = 1;
    std::uint32_t renderDepth = 0;
    bool jobsExecuting = false;
    bool processingVoices = false;
    bool destroying = false;
    std::uint32_t voiceAccesses = 0;
    std::uint32_t controlCallbacks = 0;
    std::uintptr_t userData = 0;
    std::vector<Ngs2Rack*> racks;
    std::atomic<std::uint64_t> callbackSequence{0};
};

std::string Ngs2Hex(std::uint32_t value);
std::size_t Ngs2PcmSampleBytes(std::uint32_t waveformType);
void Ngs2ReadPcmFrame(std::uint32_t waveformType, std::uint32_t channels, const std::uint8_t* frame, float* output);
void Ngs2UpdatePeakMeter(Ngs2Voice& voice, std::uint32_t grain);
std::recursive_mutex& Ngs2Mutex();

class Ngs2Lock {
public:
    Ngs2Lock();
    ~Ngs2Lock();
    Ngs2Lock(const Ngs2Lock&) = delete;
    Ngs2Lock& operator=(const Ngs2Lock&) = delete;
};

class Ngs2Unlocked {
public:
    Ngs2Unlocked();
    ~Ngs2Unlocked();
    Ngs2Unlocked(const Ngs2Unlocked&) = delete;
    Ngs2Unlocked& operator=(const Ngs2Unlocked&) = delete;

private:
    std::uint32_t depth;
};

bool Ngs2ControlActive(const Ngs2System& system);
bool Ngs2ControlActive(const Ngs2Rack& rack);
bool Ngs2ExclusiveElsewhere();
bool Ngs2SetJobWorker(bool active);
bool Ngs2RenderingHere(const Ngs2System& system);
void Ngs2EnterExclusive();
void Ngs2LeaveExclusive(std::uint32_t depth = 1);
void Ngs2WaitOnce();
void Ngs2NotifyWaiters();
Ngs2System* Ngs2FindSystem(Ngs2Handle handle);

template <typename TParam>
const TParam& ParamAs(const Ngs2VoiceParamHeader& param) {
    if (param.size < sizeof(TParam)) throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " is too small");
    return reinterpret_cast<const TParam&>(param);
}

std::vector<Ngs2Rack*>& Ngs2DetachedRacks();
Ngs2Rack* Ngs2FindRack(Ngs2Handle handle);
Ngs2Voice* Ngs2FindVoice(Ngs2Handle handle);
void* Ngs2Place(const Ngs2ContextBufferInfo* bufferInfo, std::size_t size, std::size_t alignment);
int Ngs2DestroyRack(Ngs2Rack& rack, Ngs2ContextBufferInfo* outBufferInfo);
int Ngs2ReleaseBuffer(const Ngs2BufferAllocator& allocator, Ngs2ContextBufferInfo bufferInfo, Ngs2ContextBufferInfo* outBufferInfo);
void Ngs2SetupAtrac9(Ngs2Voice& voice, const Ngs2WaveformFormat& format);
void Ngs2SetFilter(Ngs2Voice& voice, const Ngs2SamplerVoiceFilterParam& param);
std::size_t Ngs2Atrac9BlockBytes(const Ngs2Voice& voice, const Ngs2WaveformBlock& block);
void Ngs2RestartAtrac9(Ngs2Voice& voice);
bool Ngs2FinishBlock(Ngs2Voice& voice);
void Ngs2BindSamplerBlock(Ngs2Voice& voice, bool restartDecoder);
int Ngs2CheckCustomRack(std::uint32_t rackId, const Ngs2CustomRackOption& option, std::uint32_t maxInputs);
int Ngs2SetupUserFx(Ngs2Rack& rack, const Ngs2CustomRackOption& option);
void Ngs2PrepareSampler(Ngs2Voice& voice);
void Ngs2RestartSampler(Ngs2Voice& voice);
void Ngs2ReplaySampler(Ngs2Voice& voice);
void Ngs2SaveSamplerBlock(Ngs2Voice& voice, Ngs2Block& block);
void Ngs2ConsumeSampler(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate);
std::uint32_t Ngs2ReadSamplerPcm(Ngs2Voice& voice, float* dest, std::uint32_t stride, std::uint32_t count);
std::uint32_t Ngs2ReadSamplerAtrac9(Ngs2Voice& voice, float* dest, std::uint32_t stride, std::uint32_t count);
void Ngs2CleanupUserFx(Ngs2Rack& rack);
int Ngs2ApplyCustomGainParam(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param);
void Ngs2ApplyCustomParam(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param);
void Ngs2ProcessUserFx(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate);
void Ngs2SetReverbParams(Ngs2Voice& voice, const Ngs2ReverbI3DL2Param& params);
void Ngs2SetupReverb(Ngs2Voice& voice);
void Ngs2ClearReverb(Ngs2Voice& voice);
bool Ngs2ProcessReverb(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate);
void Ngs2ProcessLegacyUserFx(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate);
void Ngs2PublishSamplerState(Ngs2Voice& voice);
void Ngs2ResetJobSchedulerOption(Ngs2JobSchedulerOption& option);
void Ngs2ResetUserFx2(Ngs2Voice& voice);
void Ngs2PrepareControlMix(Ngs2Voice& voice);
void Ngs2RenderSystem(Ngs2System& system, const Ngs2RenderBufferInfo* bufferInfo, std::uint32_t numBufferInfo);
void Ngs2RenderSilence(const Ngs2System& system, const Ngs2RenderBufferInfo* bufferInfo, std::uint32_t numBufferInfo);
float Ngs2DefaultLevel(std::uint32_t sourceChannels, std::uint32_t source, std::uint32_t destChannels, std::uint32_t dest);
int Ngs2PanInit(Ngs2PanWork* work, const float* speaker_angles, float unit_angle, uint32_t num_speakers);
int Ngs2PanGetVolumeMatrix(Ngs2PanWork* work, const Ngs2PanParam* params, uint32_t num_params, uint32_t matrix_format, float* out_volume_matrix);
int Ngs2GeomResetListenerParam(Ngs2GeomListenerParam* out_listener_param);
int Ngs2GeomCalcListener(const Ngs2GeomListenerParam* param, Ngs2GeomListenerWork* outWork, std::uint32_t flags);
int Ngs2GeomApply(const Ngs2GeomListenerWork* listener, const Ngs2GeomSourceParam* source, Ngs2GeomAttribute* outAttrib, std::uint32_t flags);
int Ngs2GeomResetSourceParam(Ngs2GeomSourceParam* out_source_param);

#endif

#ifndef CORE_LIBS_PRX_LIBSCENGS2_SRC_NGS2INTERNAL_HPP
#define CORE_LIBS_PRX_LIBSCENGS2_SRC_NGS2INTERNAL_HPP

#include <cstddef>
#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include "prx/libSceNgs2.native/include/Ngs2Types.hpp"

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

struct Ngs2Piece {
    const std::uint8_t* data;
    std::uint64_t firstFrame;
    std::uint64_t frames;
};

struct Ngs2Block {
    const std::uint8_t* data;
    Ngs2WaveformBlock info;
    std::uint32_t cursor = 0;
    std::uint32_t numRepeated = 0;
    std::size_t dataCursor = 0;
    bool streaming = false;
    std::uint64_t availableFrames = 0;
    std::vector<Ngs2Piece> pieces;
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
    std::uint32_t windowStart = 0;
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
    float volume = 1.0f;
    std::int32_t matrix = -1;
};

struct Ngs2UserFx2 {
    std::vector<std::uint8_t> param;
    std::vector<std::uint8_t> work;
    std::vector<std::uint8_t> state;
    std::uint32_t flags = 0;
};

struct Ngs2Voice {
    Ngs2Rack* rack = nullptr;
    Ngs2PlayState state = Ngs2PlayState::Empty;
    std::uint32_t stateFlags = 0;
    std::uint32_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint32_t waveformType = 0;
    Ngs2Atrac9 atrac9;
    std::unique_ptr<Ngs2ReverbState, Ngs2ReverbDeleter> reverb;
    float pitch = 1.0f;
    std::uint64_t phase = 0;
    std::deque<Ngs2Block> blocks;
    bool acceptsBlocks = true;
    std::uint64_t decodedSamples = 0;
    std::uint64_t decodedBytes = 0;
    const std::uint8_t* waveformEnd = nullptr;
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
    Ngs2UserFxProcessHandler userFxHandler = nullptr;
    std::array<std::uintptr_t, 3> userFxData{};
    std::uint32_t userFxFlags = 0;
    std::vector<float> samples;
    bool rendering = false;
    bool rendered = false;
    bool hasSamples = false;

    void SetEvent(std::uint32_t eventId);
    void ResetSetup();
    const std::uint8_t* WaveformData() const;
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
    std::uint32_t maxInputs = 0;
    Ngs2ContextBufferInfo bufferInfo{};
    Ngs2BufferAllocator allocator{};
    std::vector<Ngs2Voice> voices;
    std::vector<Ngs2CustomUserFx2ModuleOption> userFx;
    std::vector<std::vector<std::uint8_t>> userFxCommon;
};

struct Ngs2System {
    Ngs2SystemOption option{};
    Ngs2ContextBufferInfo bufferInfo{};
    Ngs2BufferAllocator allocator{};
    std::uint32_t uid = 0;
    std::int64_t renderCount = 0;
    std::uintptr_t userData = 0;
    std::vector<Ngs2Rack*> racks;
};

std::string Ngs2Hex(std::uint32_t value);
const std::uint8_t* Ngs2StreamEnd(const Ngs2Voice& voice, const Ngs2Block& block);
std::recursive_mutex& Ngs2Mutex();
Ngs2System* Ngs2FindSystem(Ngs2Handle handle);

template <typename TParam>
const TParam& ParamAs(const Ngs2VoiceParamHeader& param) {
    if (param.size < sizeof(TParam)) throw std::invalid_argument("NGS2: voice param " + Ngs2Hex(param.id) + " is too small");
    return reinterpret_cast<const TParam&>(param);
}

Ngs2Rack* Ngs2FindRack(Ngs2Handle handle);
Ngs2Voice* Ngs2FindVoice(Ngs2Handle handle);
void* Ngs2Place(const Ngs2ContextBufferInfo* bufferInfo, std::size_t size, std::size_t alignment);
int Ngs2DestroyRack(Ngs2Rack& rack, Ngs2ContextBufferInfo* outBufferInfo);
int Ngs2ReleaseBuffer(const Ngs2BufferAllocator& allocator, Ngs2ContextBufferInfo bufferInfo, Ngs2ContextBufferInfo* outBufferInfo);
void Ngs2SetupAtrac9(Ngs2Voice& voice, const Ngs2WaveformFormat& format);
std::size_t Ngs2Atrac9BlockBytes(const Ngs2Voice& voice, const Ngs2WaveformBlock& block);
void Ngs2RestartAtrac9(Ngs2Voice& voice);
const float* Ngs2Atrac9Frame(Ngs2Voice& voice, Ngs2Block& block, std::uint32_t frame);
void Ngs2CheckCustomRack(const Ngs2CustomRackOption& option);
void Ngs2SetupUserFx(Ngs2Rack& rack, const Ngs2CustomRackOption& option);
void Ngs2CleanupUserFx(Ngs2Rack& rack);
void Ngs2ApplyCustomParam(Ngs2Voice& voice, const Ngs2VoiceParamHeader& param);
void Ngs2ProcessUserFx(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate);
void Ngs2SetReverbParams(Ngs2Voice& voice, const Ngs2ReverbI3DL2Param& params);
void Ngs2SetupReverb(Ngs2Voice& voice);
void Ngs2ClearReverb(Ngs2Voice& voice);
bool Ngs2ProcessReverb(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate);
void Ngs2ProcessLegacyUserFx(Ngs2Voice& voice, std::uint32_t grain, std::uint32_t sampleRate);
void Ngs2RenderSystem(Ngs2System& system, const Ngs2RenderBufferInfo* bufferInfo, std::uint32_t numBufferInfo);

#endif

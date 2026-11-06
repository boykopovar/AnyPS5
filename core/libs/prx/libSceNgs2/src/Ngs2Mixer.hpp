#ifndef CORE_LIBS_PRX_LIBSCENGS2_NGS2MIXER_HPP
#define CORE_LIBS_PRX_LIBSCENGS2_NGS2MIXER_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include "SceTypes.hpp"

// Software mix of an NGS2 voice graph. Rack ids follow the parameter ids the SDK emits
// (param id = rack id << 16 | index), recovered from ANIMAL WELL's eboot wrappers.
namespace ngs2mix {

constexpr std::uint32_t RackSampler = 0x1000;
constexpr std::uint32_t RackSubmixer = 0x2000;
constexpr std::uint32_t RackReverb = 0x2001;
constexpr std::uint32_t RackMastering = 0x3000;

// Waveform block of param 0x10000001 (0x28 bytes, see eboot 0x177e0).
struct WaveformBlock {
    std::uint64_t dataOffset;
    std::uint64_t dataSize;
    std::uint32_t numRepeats;
    std::uint32_t numSkipSamples;
    std::uint32_t numSamples;
    std::uint32_t reserved;
    std::uint64_t userData;
};
static_assert(sizeof(WaveformBlock) == 0x28);

struct Patch {
    std::uint32_t port;
    std::uint32_t destInput;
    std::uint64_t destHandle;
};

struct VoiceMix {
    std::uint32_t rackId = 0;
    bool playing = false;
    float volume = 1.0f;
    std::vector<Patch> patches;
    // Sampler source.
    std::uint32_t waveformType = 0;
    std::uint32_t channels = 1;
    std::uint32_t sampleRate = 48000;
    float pitch = 1.0f;
    const std::uint8_t* data = nullptr;
    std::vector<WaveformBlock> blocks;
    std::size_t blockIndex = 0;
    double position = 0.0;
    std::uint32_t repeatsDone = 0;
    // Reverb: I3DL2 parameters (param 0x20010001) driving a Schroeder network.
    bool reverbParamsSet = false;
    std::int32_t reverbLevelMb = 0;
    float reverbDecayTime = 1.0f;
    float reverbDecayHFRatio = 1.0f;
    bool reverbConfigured = false;
    float reverbWet = 0.0f;
    float reverbFeedback[4] = {};
    float reverbDamp = 0.0f;
    std::vector<float> combs[2][4];
    std::size_t combPos[2][4] = {};
    float combFilter[2][4] = {};
    std::vector<float> allpasses[2][2];
    std::size_t allpassPos[2][2] = {};
    // Mastering destination.
    std::int32_t renderBufferIndex = -1;
    // Per-grain interleaved stereo scratch.
    std::vector<float> input;
    std::vector<float> output;
};

// Applies one parameter block payload; false when the id is not understood for the voice.
bool ApplyParam(VoiceMix& voice, std::uint32_t id, const std::uint8_t* payload, std::size_t size);
// Runs one 32-byte command record; false when the command type is not understood.
bool RunCommand(VoiceMix& voice, const std::uint32_t* record);
std::uint32_t StateFlags(const VoiceMix& voice);
// Mixes one grain through voices (processing order) into the render buffers.
void Render(const std::vector<VoiceMix*>& voices, const std::function<VoiceMix*(std::uint64_t)>& resolve,
            const Ngs2RenderBufferInfo* buffers, std::uint32_t numBuffers, std::uint32_t systemRate);

} // namespace ngs2mix

#endif

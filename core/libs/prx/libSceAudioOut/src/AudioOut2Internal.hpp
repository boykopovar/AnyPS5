#ifndef CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUT2INTERNAL_HPP
#define CORE_LIBS_PRX_LIBSCEAUDIOOUT_SRC_AUDIOOUT2INTERNAL_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <vector>

#include "SDL.h"
#include "SceTypes.hpp"

// Shared by the AudioOut2 context and port files.
//
// APS5_TRACE_AUDIOOUT2=1 prints the title's use of the API to stderr: context and port parameters, every
// attribute the title sets (first few per port, then sampled), the first pushes and queue polls, and a
// once-per-second summary with the push rate and the SDL queue level.
bool AudioOut2TraceEnabled();
// Seconds since the first AudioOut2 call, for the trace.
double AudioOut2TraceSeconds();

#define AUDIOOUT2_TRACE(...) \
    do { \
        if (AudioOut2TraceEnabled()) std::fprintf(stderr, "[audioout2] " __VA_ARGS__); \
    } while (0)

static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80260502);
static constexpr int SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE = static_cast<int>(0x80260503);
// Not recovered from a title or SDK header: follows the 0x802605xx pattern of the codes above. Returned
// by a non-blocking push when the modelled hardware queue is full.
static constexpr int SCE_AUDIO_OUT2_ERROR_QUEUE_FULL = static_cast<int>(0x80260507);

static constexpr std::uint32_t AUDIO_OUT2_SAMPLE_RATE = 48000;
// The grain a push carries, in samples per channel: a context's num_grains (Demon's Souls: 256, 5.33 ms).
static constexpr std::uint32_t AUDIO_OUT2_DEFAULT_GRAIN = 256;
static constexpr std::uint32_t AUDIO_OUT2_OUTPUT_CHANNELS = 2;
static constexpr std::uint32_t AUDIO_OUT2_PORT_CHANNELS_MAX = 8;

struct AudioOut2Context;

struct AudioOut2Port {
    bool used = false;
    AudioOut2Context* context = nullptr;
    std::uint16_t type = 0;
    std::uint32_t dataFormat = 0;
    std::uint32_t samplingFreq = 0;
    std::uint32_t flags = 0;
    // Channel count decoded from dataFormat; 0 when the format is not understood (the port is then
    // not rendered). Samples are float.
    std::uint32_t channels = 0;
    // The PCM buffer (one grain, float, interleaved) the title last handed over through the data
    // attribute. It is guest memory the title rewrites every tick, so it is read when the context
    // mixes, not when it is set.
    const float* data = nullptr;
    float volume[AUDIO_OUT2_PORT_CHANNELS_MAX] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    std::uint64_t dataSets = 0;
    std::uint64_t attributeTraces = 0;
};

struct AudioOut2Context {
    std::mutex lock;
    std::uint32_t grain = AUDIO_OUT2_DEFAULT_GRAIN;
    std::uint32_t queueDepth = 1;
    // Fallback hardware queue model for a context without an SDL device: pushes that have not
    // finished playing, and the time the last of them finishes; playback is real time, so the queue
    // drains as the wall clock advances. With a device the SDL queue is the hardware queue.
    std::uint32_t queued = 0;
    std::chrono::steady_clock::time_point playHead;
    SDL_AudioDeviceID device = 0;
    // Stereo float mix of the ports for one push.
    std::vector<float> mix;
    // Trace counters.
    std::uint64_t pushes = 0;
    std::uint64_t blockingPushes = 0;
    std::uint64_t fullRejects = 0;
    std::uint64_t advances = 0;
    std::uint64_t queueLevelPolls = 0;
    std::uint64_t primes = 0;
    std::uint64_t dropped = 0;
    std::uint64_t summaryPushes = 0;
    std::uint64_t summaryAdvances = 0;
    std::uint64_t summaryPolls = 0;
    // Largest sample magnitude of the grains mixed since the last summary: the mixed signal's level.
    float summaryPeak = 0.0f;
    std::chrono::steady_clock::time_point summaryStart;
};

// Mixes every port of the context that carries PCM data into out (stereo float, frames frames), summing
// onto the zeroed buffer. Returns the number of ports mixed.
std::uint32_t AudioOut2MixPorts(const AudioOut2Context& context, float* out, std::uint32_t frames);
// Forgets the ports of a context being destroyed.
void AudioOut2ReleasePorts(const AudioOut2Context& context);

#endif

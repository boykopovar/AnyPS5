#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "AudioOut2Internal.hpp"

// An AudioOut2 context is the hardware output queue: every push appends a grain (num_grains samples)
// to a queue of queue_depth grains that plays in real time. Titles pace their mixer on that queue, so
// the level must follow the output clock. With an SDL device open, a grain's port buffers are read at
// the next push, the device callback plays the mixed grains behind a cushion and the level counts the
// grains beyond that cushion; otherwise each push is mixed at once and a wall-clock model of the
// queue stands in.

using Clock = std::chrono::steady_clock;

static constexpr std::size_t CONTEXT_MEMORY = 0x10000;
static constexpr std::uint32_t DEFAULT_MAX_PORTS = 16;
// Pushes, polls and advances traced individually before the trace falls back to the per-second summary.
static constexpr std::uint64_t CALL_TRACE_FULL = 16;
// Grains the device waits for before it starts playing (and again whenever it ran dry), so the
// pushing thread's scheduling jitter does not starve it; the pad device queues as much silence ahead
// of its first grain.
static constexpr std::uint32_t CUSHION_MS = 40;
// Grains the device has not played are not allowed to run further ahead than this; newer ones are dropped.
static constexpr std::uint32_t MAX_QUEUED_MS = 250;
// A blocking push on a full queue gives up after this long.
static constexpr std::chrono::milliseconds FULL_WAIT_TIMEOUT{200};
static constexpr std::chrono::milliseconds FULL_WAIT_STEP{1};
static constexpr std::chrono::microseconds READ_DELAY{100};
static constexpr std::uint16_t DEVICE_SAMPLES = 512;
// The summed ports (a 7.1 bed folded to stereo plus the object ports) peak above full scale in the
// intro cutscene (2.1 measured); the device takes float and clips hard, so the mix is attenuated
// and clamped.
static constexpr float MASTER_GAIN = 0.5f;
static constexpr std::size_t OUTPUT_FRAME_BYTES = AUDIO_OUT2_OUTPUT_CHANNELS * sizeof(float);
static constexpr std::uint32_t OUTPUT_BYTES_PER_MS = AUDIO_OUT2_SAMPLE_RATE * OUTPUT_FRAME_BYTES / 1000;
static constexpr std::chrono::seconds PAD_PROBE_INTERVAL{2};
static constexpr std::uint32_t PAD_SLACK_GRAINS = 2;

bool AudioOut2TraceEnabled() {
    static const bool enabled = std::getenv("APS5_TRACE_AUDIOOUT2") != nullptr;
    return enabled;
}

double AudioOut2TraceSeconds() {
    static const auto start = Clock::now();
    return std::chrono::duration<double>(Clock::now() - start).count();
}

static AudioOut2Context* FromHandle(AudioOut2ContextHandle ctx) {
    return reinterpret_cast<AudioOut2Context*>(ctx);
}

static Clock::duration GrainDuration(const AudioOut2Context& context) {
    return std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(static_cast<double>(context.grain) / AUDIO_OUT2_SAMPLE_RATE));
}

static std::uint32_t GrainBytes(const AudioOut2Context& context) {
    return static_cast<std::uint32_t>(context.grain * OUTPUT_FRAME_BYTES);
}

static std::uint32_t CushionGrains(const AudioOut2Context& context) {
    return (CUSHION_MS * AUDIO_OUT2_SAMPLE_RATE / 1000 + context.grain - 1) / context.grain;
}

static std::uint32_t UnplayedGrains(const AudioOut2Context& context) {
    const auto frames = context.output.size() / AUDIO_OUT2_OUTPUT_CHANNELS;
    return static_cast<std::uint32_t>(context.pendingGrains.size() + (frames + context.grain - 1) / context.grain);
}

static std::uint32_t PendingMs(const AudioOut2Context& context) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(UnplayedGrains(context)) * context.grain * 1000 / AUDIO_OUT2_SAMPLE_RATE);
}

// Retires the modelled grains whose playback finished by now. The caller holds the context lock.
static void Drain(AudioOut2Context& context, Clock::time_point now) {
    const auto duration = GrainDuration(context);
    while (context.queued > 0) {
        const auto oldestEnd = context.playHead - duration * (context.queued - 1);
        if (oldestEnd > now) break;
        context.queued--;
    }
    if (context.queued == 0 && context.playHead < now) context.playHead = now;
}

// Grains queued and not yet played, as the title sees them. The caller holds the context lock.
static std::uint32_t QueueLevel(AudioOut2Context& context, Clock::time_point now) {
    Drain(context, now);
    if (context.device == 0) return context.queued;
    const auto unplayed = UnplayedGrains(context);
    const auto cushion = CushionGrains(context);
    return unplayed > cushion ? std::min(context.queueDepth, unplayed - cushion) : 0;
}

static void DeviceCallback(void* userdata, Uint8* stream, int length);

static void OpenDevice(AudioOut2Context& context) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
        AUDIOOUT2_TRACE("SDL audio init failed: %s\n", SDL_GetError());
        return;
    }
    SDL_AudioSpec desired{};
    desired.freq = static_cast<int>(AUDIO_OUT2_SAMPLE_RATE);
    desired.format = AUDIO_F32SYS;
    desired.channels = AUDIO_OUT2_OUTPUT_CHANNELS;
    desired.samples = DEVICE_SAMPLES;
    desired.callback = DeviceCallback;
    desired.userdata = &context;
    SDL_AudioSpec obtained{};
    // No format change is allowed: SDL converts to the device's native format itself, so the callback
    // always writes stereo float at 48 kHz.
    context.device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (context.device == 0) {
        AUDIOOUT2_TRACE("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return;
    }
    SDL_PauseAudioDevice(context.device, 0);
    AUDIOOUT2_TRACE("SDL device %u opened: %d Hz, format 0x%x, %u channels, %u samples\n", context.device, obtained.freq, obtained.format, obtained.channels, obtained.samples);
}

static void CloseDevice(SDL_AudioDeviceID device) {
    if (device != 0 && SDL_WasInit(SDL_INIT_AUDIO) != 0) SDL_CloseAudioDevice(device);
}

static void OpenPadDevice(AudioOut2Context& context) {
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) return;
    const int count = SDL_GetNumAudioDevices(0);
    for (int index = 0; index < count; index++) {
        const char* listed = SDL_GetAudioDeviceName(index, 0);
        if (!AudioOut2IsPadAudioDevice(listed)) continue;
        const std::string name(listed);
        SDL_AudioSpec native{};
        if (SDL_GetAudioDeviceSpec(index, 0, &native) == 0 && native.channels != 0 && native.channels != AUDIO_OUT2_PAD_CHANNELS) continue;
        const auto layout = AudioOut2PadLayoutForDriver(SDL_GetCurrentAudioDriver());
        SDL_AudioSpec desired{};
        desired.freq = static_cast<int>(AUDIO_OUT2_SAMPLE_RATE);
        desired.format = AUDIO_F32SYS;
        desired.channels = static_cast<Uint8>(layout.channels);
        desired.samples = DEVICE_SAMPLES;
        desired.callback = nullptr;
        SDL_AudioSpec obtained{};
        context.padDevice = SDL_OpenAudioDevice(name.c_str(), 0, &desired, &obtained, 0);
        if (context.padDevice == 0) {
            APS5_LOG_ERR("AudioOut2: could not open the controller sound card '%s': %s", name.c_str(), SDL_GetError());
            return;
        }
        context.padLayout = layout;
        context.padMix.assign(static_cast<std::size_t>(context.grain) * AUDIO_OUT2_PAD_CHANNELS, 0.0f);
        context.padFrames.assign(static_cast<std::size_t>(context.grain) * layout.channels, 0.0f);
        SDL_PauseAudioDevice(context.padDevice, 0);
        APS5_LOG_OUT("AudioOut2: controller speaker and vibration ports play on '%s'", name.c_str());
        return;
    }
}

static void ClosePadDevice(AudioOut2Context& context) {
    if (context.padDevice != 0 && SDL_WasInit(SDL_INIT_AUDIO) != 0) {
        SDL_ClearQueuedAudio(context.padDevice);
        SDL_CloseAudioDevice(context.padDevice);
    }
    context.padDevice = 0;
}

static void UpdatePadDevice(AudioOut2Context& context, Clock::time_point now) {
    if (context.padDevice != 0 && SDL_GetAudioDeviceStatus(context.padDevice) == SDL_AUDIO_STOPPED) {
        APS5_LOG_CHARS_OUT("AudioOut2: the controller sound card went away; its ports play in the main mix");
        ClosePadDevice(context);
        context.nextPadProbe = now + PAD_PROBE_INTERVAL;
    }
    if (context.padDevice != 0 || now < context.nextPadProbe) return;
    context.nextPadProbe = now + PAD_PROBE_INTERVAL;
    if (AudioOut2HasPadPorts(context)) OpenPadDevice(context);
}

static void QueuePadGrain(AudioOut2Context& context) {
    AudioOut2WritePadFrames(context.padMix.data(), context.padLayout, context.padFrames.data(), context.grain);
    const auto frameBytes = static_cast<std::uint32_t>(context.padLayout.channels * sizeof(float));
    const auto grainBytes = context.grain * frameBytes;
    const auto cushionBytes = CUSHION_MS * (AUDIO_OUT2_SAMPLE_RATE / 1000) * frameBytes;
    const auto mainQueuedBytes = context.device != 0 ? UnplayedGrains(context) * grainBytes : cushionBytes + context.queueDepth * grainBytes;
    const auto queuedBytes = SDL_GetQueuedAudioSize(context.padDevice);
    if (queuedBytes > mainQueuedBytes + PAD_SLACK_GRAINS * grainBytes) return;
    if (queuedBytes == 0) {
        static const std::vector<float> silence(static_cast<std::size_t>(CUSHION_MS) * AUDIO_OUT2_SAMPLE_RATE / 1000 * AUDIO_OUT2_PAD_DEVICE_CHANNELS_MAX, 0.0f);
        SDL_QueueAudio(context.padDevice, silence.data(), cushionBytes);
    }
    SDL_QueueAudio(context.padDevice, context.padFrames.data(), grainBytes);
}

static void Render(AudioOut2Context& context, const AudioOut2Grain& grain) {
    float* pad = nullptr;
    if (context.padDevice != 0) {
        std::fill(context.padMix.begin(), context.padMix.end(), 0.0f);
        pad = context.padMix.data();
    }
    std::fill(context.mix.begin(), context.mix.end(), 0.0f);
    AudioOut2MixPorts(context, grain, context.mix.data(), pad, context.grain);
    for (float& sample : context.mix) {
        sample = std::clamp(sample * MASTER_GAIN, -1.0f, 1.0f);
        if (AudioOut2TraceEnabled()) context.summaryPeak = std::max(context.summaryPeak, std::abs(sample));
    }
    if (pad != nullptr) {
        AudioOut2FinishPadMix(pad, context.grain);
        QueuePadGrain(context);
    }
}

static void DeviceCallback(void* userdata, Uint8* stream, int length) {
    auto& context = *static_cast<AudioOut2Context*>(userdata);
    std::lock_guard lock(context.lock);
    auto* out = reinterpret_cast<float*>(stream);
    const auto samples = static_cast<std::size_t>(length) / sizeof(float);
    const auto cushionSamples = static_cast<std::size_t>(CushionGrains(context)) * context.grain * AUDIO_OUT2_OUTPUT_CHANNELS;
    if (context.priming && context.output.size() >= cushionSamples) context.priming = false;
    const auto count = context.priming ? 0 : std::min(samples, context.output.size());
    std::copy_n(context.output.begin(), count, out);
    context.output.erase(context.output.begin(), context.output.begin() + static_cast<std::ptrdiff_t>(count));
    std::fill(out + count, out + samples, 0.0f);
    if (count < samples && !context.priming) {
        context.priming = true;
        context.primes++;
    }
}

static void TraceSummary(AudioOut2Context& context, Clock::time_point now) {
    if (!AudioOut2TraceEnabled()) return;
    if (context.summaryStart == Clock::time_point{}) {
        context.summaryStart = now;
        return;
    }
    const auto elapsed = std::chrono::duration<double>(now - context.summaryStart).count();
    if (elapsed < 1.0) return;
    std::fprintf(stderr, "[audioout2] t=%.3f ctx %p: %.1f pushes/s, %.1f advances/s, %.1f queue polls/s, hw queue %u/%u, unread %u ms, mix peak %.3f; totals: pushes %llu (%llu blocking, %llu queue-full rejects), primes %llu, dropped %llu\n",
        AudioOut2TraceSeconds(), static_cast<void*>(&context), static_cast<double>(context.summaryPushes) / elapsed, static_cast<double>(context.summaryAdvances) / elapsed,
        static_cast<double>(context.summaryPolls) / elapsed, QueueLevel(context, now), context.queueDepth, PendingMs(context), static_cast<double>(context.summaryPeak),
        static_cast<unsigned long long>(context.pushes), static_cast<unsigned long long>(context.blockingPushes), static_cast<unsigned long long>(context.fullRejects),
        static_cast<unsigned long long>(context.primes), static_cast<unsigned long long>(context.dropped));
    context.summaryStart = now;
    context.summaryPushes = 0;
    context.summaryAdvances = 0;
    context.summaryPolls = 0;
    context.summaryPeak = 0.0f;
}

extern "C" {

// The title's per-tick step between setting the ports' data and pushing. A push records the buffers the
// ports point at and the next push reads them, and the push paces the clock, so nothing is due here.
int APS5_VABI sceAudioOut2ContextAdvance(AudioOut2ContextHandle ctx) {
    auto* context = FromHandle(ctx);
    if (!context) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    std::lock_guard lock(context->lock);
    context->advances++;
    context->summaryAdvances++;
    if (context->advances <= CALL_TRACE_FULL) AUDIOOUT2_TRACE("t=%.3f Advance ctx %p (pushes so far %llu)\n", AudioOut2TraceSeconds(), static_cast<void*>(context), static_cast<unsigned long long>(context->pushes));
    return 0;
}

int APS5_VABI sceAudioOut2ContextCreate(const AudioOut2ContextParam* params, void* buffer, size_t buffer_size, AudioOut2ContextHandle* ctx) {
    if (!params || !ctx) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    auto* context = new AudioOut2Context();
    context->grain = params->num_grains ? params->num_grains : AUDIO_OUT2_DEFAULT_GRAIN;
    context->queueDepth = params->queue_depth ? params->queue_depth : 1;
    context->playHead = Clock::now();
    context->mix.assign(static_cast<std::size_t>(context->grain) * AUDIO_OUT2_OUTPUT_CHANNELS, 0.0f);
    AUDIOOUT2_TRACE("t=%.3f ContextCreate: max_ports=%u max_object_ports=%u guarantee_object_ports=%u queue_depth=%u num_grains=%u flags=0x%x buffer=%p size=%zu -> ctx %p: %u-sample grains (%.2f ms), %u queued, %u Hz stereo float output\n",
        AudioOut2TraceSeconds(), params->max_ports, params->max_object_ports, params->guarantee_object_ports, params->queue_depth, params->num_grains, params->flags, buffer, buffer_size,
        static_cast<void*>(context), context->grain, 1000.0 * context->grain / AUDIO_OUT2_SAMPLE_RATE, context->queueDepth, AUDIO_OUT2_SAMPLE_RATE);
    OpenDevice(*context);
    *ctx = reinterpret_cast<AudioOut2ContextHandle>(context);
    return 0;
}

int APS5_VABI sceAudioOut2ContextDestroy(AudioOut2ContextHandle ctx) {
    auto* context = FromHandle(ctx);
    if (!context) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    AUDIOOUT2_TRACE("t=%.3f ContextDestroy ctx %p after %llu pushes\n", AudioOut2TraceSeconds(), static_cast<void*>(context), static_cast<unsigned long long>(context->pushes));
    SDL_AudioDeviceID device = 0;
    {
        std::lock_guard lock(context->lock);
        device = std::exchange(context->device, 0);
        ClosePadDevice(*context);
    }
    CloseDevice(device);
    AudioOut2ReleasePorts(*context);
    delete context;
    return 0;
}

int APS5_VABI sceAudioOut2ContextGetQueueLevel(AudioOut2ContextHandle ctx, uint32_t* queue_level, uint32_t* available_queue) {
    auto* context = FromHandle(ctx);
    if (!context) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    std::lock_guard lock(context->lock);
    const auto level = QueueLevel(*context, Clock::now());
    if (queue_level) *queue_level = level;
    if (available_queue) *available_queue = context->queueDepth - level;
    context->queueLevelPolls++;
    context->summaryPolls++;
    if (context->queueLevelPolls <= CALL_TRACE_FULL) AUDIOOUT2_TRACE("t=%.3f GetQueueLevel ctx %p -> level %u, available %u\n", AudioOut2TraceSeconds(), static_cast<void*>(context), level, context->queueDepth - level);
    return 0;
}

int APS5_VABI sceAudioOut2ContextPush(AudioOut2ContextHandle ctx, uint32_t blocking) {
    auto* context = FromHandle(ctx);
    if (!context) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    std::unique_lock lock(context->lock);
    auto now = Clock::now();
    const auto waitStart = now;
    while (QueueLevel(*context, now) >= context->queueDepth) {
        if (!blocking) {
            context->fullRejects++;
            if (context->fullRejects <= CALL_TRACE_FULL) AUDIOOUT2_TRACE("t=%.3f Push ctx %p non-blocking on a full queue (%u): rejected\n", AudioOut2TraceSeconds(), static_cast<void*>(context), context->queueDepth);
            return SCE_AUDIO_OUT2_ERROR_QUEUE_FULL;
        }
        if (now - waitStart > FULL_WAIT_TIMEOUT) break;
        lock.unlock();
        PreciseSleepUs(std::chrono::microseconds(FULL_WAIT_STEP).count());
        lock.lock();
        now = Clock::now();
    }
    context->queued++;
    context->playHead += GrainDuration(*context);
    UpdatePadDevice(*context, now);
    auto grain = AudioOut2CaptureGrain(*context);
    const auto mixed = static_cast<std::uint32_t>(grain.size());
    if (context->device == 0) {
        Render(*context, grain);
    } else {
        context->pendingGrains.push_back(std::move(grain));
        context->pendingSince.push_back(Clock::now());
        while (context->pendingGrains.size() >= 2) {
            const auto readAt = context->pendingSince.front() + READ_DELAY;
            if (Clock::now() < readAt) {
                lock.unlock();
                while (Clock::now() < readAt) std::this_thread::yield();
                lock.lock();
                continue;
            }
            Render(*context, context->pendingGrains.front());
            context->pendingGrains.pop_front();
            context->pendingSince.pop_front();
            if (PendingMs(*context) > MAX_QUEUED_MS) {
                context->dropped++;
                continue;
            }
            context->output.insert(context->output.end(), context->mix.begin(), context->mix.end());
        }
    }
    context->pushes++;
    context->summaryPushes++;
    if (blocking) context->blockingPushes++;
    if (context->pushes <= CALL_TRACE_FULL) {
        AUDIOOUT2_TRACE("t=%.3f Push ctx %p blocking=%u: grain from %u ports, hw queue %u/%u, unread %u ms\n", AudioOut2TraceSeconds(), static_cast<void*>(context), blocking,
            mixed, QueueLevel(*context, now), context->queueDepth, PendingMs(*context));
    }
    TraceSummary(*context, now);
    return 0;
}

int APS5_VABI sceAudioOut2ContextQueryMemory(const AudioOut2ContextParam* params, size_t* memory_size) {
    if (!params || !memory_size) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    *memory_size = CONTEXT_MEMORY;
    return 0;
}

int APS5_VABI sceAudioOut2ContextResetParam(AudioOut2ContextParam* params) {
    if (!params) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    std::memset(params, 0, sizeof(*params));
    params->max_ports = DEFAULT_MAX_PORTS;
    params->queue_depth = 1;
    params->num_grains = AUDIO_OUT2_DEFAULT_GRAIN;
    return 0;
}

int APS5_VABI sceAudioOut2ContextSetAttributes(AudioOut2ContextHandle ctx, const AudioOut2Attribute* attributes, uint32_t num) {
    auto* context = FromHandle(ctx);
    if (!context) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    if (!attributes && num != 0) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    for (uint32_t index = 0; index < num; index++) {
        AUDIOOUT2_TRACE("t=%.3f ContextSetAttributes ctx %p: id=0x%x size=%zu value=%p (ignored)\n", AudioOut2TraceSeconds(), static_cast<void*>(context), attributes[index].attribute_id, attributes[index].value_size, attributes[index].value);
    }
    return 0;
}

}

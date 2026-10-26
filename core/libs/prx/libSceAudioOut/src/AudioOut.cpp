#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <prx/libc/include/General.hpp>

#include "SDL.h"
#include "SceTypes.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libSceAudioOut/include/AudioOutSdl.hpp"
#include "AudioOutShutdown.hpp"
#include "AudioOut2PadMix.hpp"
#include "prx/libScePad/include/PadState.hpp"

static constexpr int PORT_TYPE_MAIN = 0;
static constexpr int PORT_TYPE_BGM = 1;
static constexpr int PORT_TYPE_VOICE = 2;
static constexpr int PORT_TYPE_PERSONAL = 3;
static constexpr int PORT_TYPE_PADSPK = 4;
static constexpr int PORT_TYPE_EX = 6;
static constexpr int PORT_TYPE_SYS = 7;
static constexpr int PORT_TYPE_VIBRATION = 10;
static constexpr int PORT_TYPE_AUDIO3D = 126;
static constexpr int PORT_TYPE_AUX = 127;

static constexpr int PORTS_MAX = 32;
static constexpr int DEFAULT_VOLUME = 32768;
static constexpr int DEFAULT_PADSPK_MIX_LEVEL = 11626;
static constexpr std::uint32_t FORMAT_MASK = 0xFFu;
static constexpr std::uint64_t TARGET_LATENCY_US = 40000;
static constexpr std::uint64_t DRAIN_TIMEOUT_US = 200000;
static constexpr std::uint64_t DRAIN_SLEEP_US = 1000;
static constexpr int MONITOR_TYPE_HDMI = 1;
static constexpr std::uint32_t HDMI_MONITOR_INFO_SIZE = 0x180;
static constexpr std::uint32_t EX_MONITOR_INFO_SIZE = 0x400;
static constexpr std::uint32_t EX_OPEN_MODE_MAX = 10;
static constexpr std::uint32_t EX_OPEN_MODES = 0x61F;
static constexpr std::uint32_t EX_CONFIGURE_MODE_DEFAULT = 0xFF;
static constexpr std::uint32_t EX_CONFIGURE_TARGET_DEFAULT = 0xFF;
static constexpr int ERROR_EX_INVALID_MODE = -2144993259;
static constexpr int ERROR_EX_INVALID_CONTROL = -2144960503;
static constexpr std::uint32_t SYS_OPEN_MODE_TRUEHD = 5;
static constexpr std::uint32_t SYS_CONFIGURE_TYPE_HDMI = 1;
static constexpr std::uint32_t SYS_CONFIGURE_TARGET_HDMI = 1;
static constexpr std::uint32_t SYS_PORT_SAMPLES = 1024;
static constexpr std::uint32_t SYS_PORT_FRAME_RATE = 192000;

enum class Format {
    Unknown,
    S16Mono,
    S16Stereo,
    S16_8Ch,
    F32Mono,
    F32Stereo,
    F32_8Ch,
    S16_8ChStd,
    F32_8ChStd,
};

static bool formatIsFloat(Format f) {
    return f == Format::F32Mono || f == Format::F32Stereo ||
           f == Format::F32_8Ch || f == Format::F32_8ChStd;
}

static bool formatIsStd(Format f) {
    return f == Format::S16_8ChStd || f == Format::F32_8ChStd;
}

static int channelsForFormat(Format f) {
    switch (f) {
        case Format::S16Mono:
        case Format::F32Mono:
            return 1;
        case Format::S16Stereo:
        case Format::F32Stereo:
            return 2;
        case Format::S16_8Ch:
        case Format::F32_8Ch:
        case Format::S16_8ChStd:
        case Format::F32_8ChStd:
            return 8;
        default:
            throw std::runtime_error("channelsForFormat: unknown format");
    }
}

static SDL_AudioFormat sdlFormat(Format f) {
    return formatIsFloat(f) ? AUDIO_F32SYS : AUDIO_S16SYS;
}

static std::uint32_t bytesPerSample(Format f) {
    return formatIsFloat(f) ? sizeof(float) : sizeof(std::int16_t);
}

static std::uint32_t checkedBufferSize(std::uint32_t frames, std::uint32_t channels, std::uint32_t bps) {
    const std::uint64_t size = static_cast<std::uint64_t>(frames) * channels * bps;
    if (size > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::runtime_error("checkedBufferSize: audio buffer size overflows");
    }
    return static_cast<std::uint32_t>(size);
}

struct Port {
    bool used = false;
    int type = 0;
    std::uint32_t samplesNum = 0;
    std::uint32_t freq = 0;
    Format format = Format::Unknown;
    int channels = 0;
    int volume[8] = {};
    int mixLevel = DEFAULT_VOLUME;
    std::uint64_t lastDataOutputTime = 0;
    std::uint64_t virtualQueueEnd = 0;
    AudioOut2PadLayout padLayout;
    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec spec = {};
};

static std::mutex g_mutex;
static std::array<std::mutex, PORTS_MAX> g_outputMutexes;
static Port g_ports[PORTS_MAX];
static bool g_sdlInitialized = false;

static bool ensureSdlAudio() {
    if (g_sdlInitialized) {
        return true;
    }
    if (InitializeSdlAudio_nid_no_patch() < 0) {
        return false;
    }
    g_sdlInitialized = true;
    return true;
}

static bool openDevice(Port& port) {
    if (!ensureSdlAudio()) {
        return false;
    }
    SDL_AudioSpec desired{};
    desired.freq = static_cast<int>(port.freq);
    desired.format = sdlFormat(port.format);
    desired.channels = static_cast<Uint8>(port.channels);
    desired.samples = static_cast<Uint16>(port.samplesNum);
    desired.callback = nullptr;
    SDL_AudioSpec obtained{};
    port.device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (port.device == 0) {
        return false;
    }
    port.spec = obtained;
    SDL_PauseAudioDevice(port.device, 0);
    return true;
}

static bool openVibrationDevice(Port& port) {
    if (!ensureSdlAudio()) throw std::runtime_error(std::string("AudioOut: could not initialize controller audio: ") + SDL_GetError());
    const int count = SDL_GetNumAudioDevices(0);
    if (count < 0) throw std::runtime_error(std::string("AudioOut: could not enumerate controller sound cards: ") + SDL_GetError());
    for (int index = 0; index < count; ++index) {
        const char* name = SDL_GetAudioDeviceName(index, 0);
        if (!AudioOut2IsPadAudioDevice(name)) continue;
        const std::string controllerName(name);
        SDL_AudioSpec native{};
        if (SDL_GetAudioDeviceSpec(index, 0, &native) == 0 && native.channels != 0 && native.channels != AUDIO_OUT2_PAD_CHANNELS) continue;
        port.padLayout = AudioOut2PadLayoutForDriver(SDL_GetCurrentAudioDriver());
        SDL_AudioSpec desired{};
        desired.freq = static_cast<int>(port.freq);
        desired.format = AUDIO_F32SYS;
        desired.channels = static_cast<Uint8>(port.padLayout.channels);
        desired.samples = static_cast<Uint16>(port.samplesNum);
        port.device = SDL_OpenAudioDevice(controllerName.c_str(), 0, &desired, &port.spec, 0);
        if (port.device == 0) throw std::runtime_error(std::string("AudioOut: could not open the controller sound card: ") + SDL_GetError());
        SDL_PauseAudioDevice(port.device, 0);
        return true;
    }
    return false;
}

static void closeDevice(Port& port) {
    if (port.device != 0 && SDL_WasInit(SDL_INIT_AUDIO) != 0) {
        SDL_ClearQueuedAudio(port.device);
        SDL_CloseAudioDevice(port.device);
    }
    port.device = 0;
    port.spec = {};
}

static constexpr std::uint32_t STD_8CH_MAP[8] = {0, 1, 2, 3, 6, 7, 4, 5};

static const void* prepareBuffer(const Port& port, const void* data, std::vector<std::uint8_t>& buf) {
    const auto frames = port.samplesNum;
    const auto ch = static_cast<std::uint32_t>(port.channels);
    const auto bps = bytesPerSample(port.format);
    const auto size = checkedBufferSize(frames, ch, bps);

    bool volumeChanged = port.mixLevel != DEFAULT_VOLUME;
    for (std::uint32_t i = 0; i < ch; i++) {
        if (port.volume[i] != DEFAULT_VOLUME) {
            volumeChanged = true;
            break;
        }
    }

    if (!volumeChanged && !formatIsStd(port.format)) {
        return data;
    }

    buf.resize(size);
    const bool isStd = formatIsStd(port.format) && ch == 8;

    if (formatIsFloat(port.format)) {
        auto* dst = reinterpret_cast<float*>(buf.data());
        const auto* src = static_cast<const float*>(data);
        const float mixLevel = static_cast<float>(port.mixLevel) / static_cast<float>(DEFAULT_VOLUME);
        for (std::uint32_t fr = 0; fr < frames; fr++) {
            for (std::uint32_t c = 0; c < ch; c++) {
                const auto srcCh = isStd ? STD_8CH_MAP[c] : c;
                dst[fr * ch + c] = src[fr * ch + srcCh] *
                    (static_cast<float>(port.volume[c]) / static_cast<float>(DEFAULT_VOLUME)) * mixLevel;
            }
        }
    } else {
        auto* dst = reinterpret_cast<std::int16_t*>(buf.data());
        const auto* src = static_cast<const std::int16_t*>(data);
        for (std::uint32_t fr = 0; fr < frames; fr++) {
            for (std::uint32_t c = 0; c < ch; c++) {
                const auto srcCh = isStd ? STD_8CH_MAP[c] : c;
                std::int64_t s = static_cast<std::int64_t>(src[fr * ch + srcCh]) *
                    port.volume[c] * port.mixLevel / (static_cast<std::int64_t>(DEFAULT_VOLUME) * DEFAULT_VOLUME);
                s = std::clamp(s,
                    static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::min()),
                    static_cast<std::int64_t>(std::numeric_limits<std::int16_t>::max()));
                dst[fr * ch + c] = static_cast<std::int16_t>(s);
            }
        }
    }
    return buf.data();
}

static std::vector<float> prepareVibration(const Port& port, const void* data) {
    std::vector<std::uint8_t> prepared;
    const auto* samples = prepareBuffer(port, data, prepared);
    std::vector<float> pad(static_cast<std::size_t>(port.samplesNum) * AUDIO_OUT2_PAD_CHANNELS, 0.0f);
    constexpr float unity[2] = {1.0f, 1.0f};
    for (std::uint32_t frame = 0; frame < port.samplesNum; ++frame) {
        float input[2]{};
        for (int channel = 0; channel < port.channels; ++channel) {
            const auto index = static_cast<std::size_t>(frame) * port.channels + channel;
            input[channel] = formatIsFloat(port.format) ? static_cast<const float*>(samples)[index] : static_cast<const std::int16_t*>(samples)[index] / 32768.0f;
        }
        AudioOut2AccumulatePadFrame(AudioOut2Route::PadVibration, input, port.channels, unity, pad.data() + static_cast<std::size_t>(frame) * AUDIO_OUT2_PAD_CHANNELS);
    }
    AudioOut2FinishPadMix(pad.data(), port.samplesNum);
    return pad;
}

static void sleepUs(std::unique_lock<std::mutex>& lock, std::uint64_t us) {
    lock.unlock();
    PreciseSleepUs(us);
    lock.lock();
}

static void queueAudio(Port& port, const void* data, std::unique_lock<std::mutex>& lock) {
    if (data == nullptr) {
        const std::uint64_t waitStart = sceKernelGetProcessTime();
        while (SDL_GetQueuedAudioSize(port.device) > 0) {
            if (sceKernelGetProcessTime() - waitStart > DRAIN_TIMEOUT_US) {
                SDL_ClearQueuedAudio(port.device);
                break;
            }
            sleepUs(lock, DRAIN_SLEEP_US);
        }
        return;
    }

    std::vector<std::uint8_t> prepareBuf;
    std::vector<float> padFrames;
    const void* queueData = nullptr;
    std::uint32_t queueSize = 0;
    if (port.type == PORT_TYPE_VIBRATION) {
        queueSize = checkedBufferSize(port.samplesNum, port.padLayout.channels, sizeof(float));
        const auto pad = prepareVibration(port, data);
        padFrames.resize(static_cast<std::size_t>(port.samplesNum) * port.padLayout.channels);
        AudioOut2WritePadFrames(pad.data(), port.padLayout, padFrames.data(), port.samplesNum);
        queueData = padFrames.data();
    } else {
        queueData = prepareBuffer(port, data, prepareBuf);
        queueSize = checkedBufferSize(port.samplesNum, static_cast<std::uint32_t>(port.channels), bytesPerSample(port.format));
    }

    const std::uint64_t bufferUs = port.freq != 0
        ? (1000000ULL * port.samplesNum) / port.freq
        : 0;
    const std::uint32_t buffers = bufferUs != 0
        ? static_cast<std::uint32_t>((TARGET_LATENCY_US + bufferUs - 1) / bufferUs)
        : 2u;
    const std::uint32_t minQueued = queueSize * std::clamp(buffers, 2u, 16u);
    const std::uint64_t waitStart = sceKernelGetProcessTime();

    while (SDL_GetQueuedAudioSize(port.device) > minQueued) {
        if (sceKernelGetProcessTime() - waitStart > DRAIN_TIMEOUT_US) {
            SDL_ClearQueuedAudio(port.device);
            break;
        }
        sleepUs(lock, DRAIN_SLEEP_US);
    }

    if (SDL_QueueAudio(port.device, queueData, queueSize) < 0) {
        throw std::runtime_error(std::string("SDL_QueueAudio: ") + SDL_GetError());
    }
    // APS5_LOG_OUT("device=%u type=%d bytes=%u queued=%u", port.device, port.type, queueSize, SDL_GetQueuedAudioSize(port.device));
}

static void paceVirtualPort(Port& port, bool hasData, std::unique_lock<std::mutex>& lock) {
    const std::uint64_t now = sceKernelGetProcessTime();
    const auto queued = static_cast<std::int64_t>(port.virtualQueueEnd - now);
    if (queued <= 0) port.virtualQueueEnd = now;
    if (!hasData) {
        if (queued > 0) sleepUs(lock, static_cast<std::uint64_t>(queued));
        return;
    }
    if (queued > static_cast<std::int64_t>(TARGET_LATENCY_US)) sleepUs(lock, static_cast<std::uint64_t>(queued) - TARGET_LATENCY_US);
    port.virtualQueueEnd += (1000000ULL * port.samplesNum) / port.freq;
}

static void outputVirtualPort(Port& port, const void* data, std::unique_lock<std::mutex>& lock) {
    const auto pad = port.type == PORT_TYPE_VIBRATION && data != nullptr ? prepareVibration(port, data) : std::vector<float>{};
    paceVirtualPort(port, data != nullptr, lock);
    const auto source = static_cast<int>(&port - g_ports) + 1;
    if (port.type == PORT_TYPE_VIBRATION && data == nullptr) PadClearLegacyAudioHaptics_nid_postfix(source);
    if (!pad.empty()) {
        constexpr std::uint32_t grainFrames = 256;
        const auto begin = port.virtualQueueEnd - (1000000ULL * port.samplesNum) / port.freq;
        for (std::uint32_t frame = 0; frame < port.samplesNum;) {
            const auto count = std::min(grainFrames, port.samplesNum - frame);
            const auto start = begin + (1000000ULL * frame) / port.freq;
            const auto end = begin + (1000000ULL * (frame + count)) / port.freq;
            const auto rumble = AudioOut2RumbleFromPadFrames(pad.data() + static_cast<std::size_t>(frame) * AUDIO_OUT2_PAD_CHANNELS, count);
            PadQueueLegacyAudioHaptics_nid_postfix(source, start, end, rumble[0], rumble[1]);
            frame += count;
        }
    }
}

static bool portTypeValid(int type) {
    return (type >= PORT_TYPE_MAIN && type <= PORT_TYPE_PADSPK) ||
           type == PORT_TYPE_VIBRATION ||
           type == PORT_TYPE_AUDIO3D ||
           type == PORT_TYPE_AUX;
}

static Port* getPort(int handle) {
    const int idx = handle - 1;
    if (idx < 0 || idx >= PORTS_MAX || !g_ports[idx].used) {
        return nullptr;
    }
    return &g_ports[idx];
}

static int allocatePort(int type, std::uint32_t len, std::uint32_t freq, Format format) {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (int i = 0; i < PORTS_MAX; i++) {
        if (!g_ports[i].used) {
            Port& port = g_ports[i];
            port.used = true;
            port.type = type;
            port.samplesNum = len;
            port.freq = freq;
            port.format = format;
            port.channels = channelsForFormat(format);
            port.lastDataOutputTime = 0;
            port.virtualQueueEnd = sceKernelGetProcessTime();
            port.mixLevel = type == PORT_TYPE_PADSPK ? DEFAULT_PADSPK_MIX_LEVEL : DEFAULT_VOLUME;
            if (type == PORT_TYPE_VIBRATION) {
                try {
                    openVibrationDevice(port);
                } catch (...) {
                    port = Port{};
                    throw;
                }
            }
            for (int c = 0; c < port.channels; c++) {
                port.volume[c] = DEFAULT_VOLUME;
            }
            if (type != PORT_TYPE_VIBRATION && type != PORT_TYPE_EX && type != PORT_TYPE_SYS) {
                openDevice(port);
            }
            static const bool trace = std::getenv("APS5_TRACE_AUDIOOUT") != nullptr;
            if (trace) std::fprintf(stderr, "[audioout] port %d: type %d, %u samples at %u Hz, format %d, device %u\n", i + 1, type, len, freq, static_cast<int>(format), static_cast<unsigned>(port.device));
            return i + 1;
        }
    }
    return -2144993275;
}

static std::atomic<bool> g_audioOut2Initialized{false};
static std::atomic<bool> g_audioOutInitialized{false};
static constexpr int SCE_AUDIO_OUT2_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x80268004);
static constexpr int SCE_AUDIO_OUT_ERROR_ALREADY_INITIALIZED = static_cast<int>(0x8026000e);

bool AudioOut2IsInitialized_nid_no_patch() {
    return g_audioOut2Initialized.load();
}

void AudioOut2MarkInitialized_nid_no_patch() {
    g_audioOut2Initialized.store(true);
}

extern "C" {

int APS5_VABI sceAudioOut2Initialize(void) {
    return g_audioOut2Initialized.exchange(true) ? SCE_AUDIO_OUT2_ERROR_ALREADY_INITIALIZED : 0;
}

int APS5_VABI sceAudioOutInit() {
    if (g_audioOutInitialized.exchange(true)) return SCE_AUDIO_OUT_ERROR_ALREADY_INITIALIZED;
    AudioOut2MarkInitialized_nid_no_patch();
    return 0;
}

int APS5_VABI sceAudioOutOpen(int userId, int type, int index, std::uint32_t len,
    std::uint32_t freq, std::uint32_t param) {
    (void)userId;
    if (!portTypeValid(type)) {
        return -2144993270;
    }
    if (len == 0) {
        return -2144993274;
    }
    if (freq == 0) {
        return -2144993272;
    }
    if (index != 0) {
        throw std::runtime_error("sceAudioOutOpen: index != 0 not supported");
    }

    Format format = Format::Unknown;
    switch (param & FORMAT_MASK) {
        case 0: format = Format::S16Mono; break;
        case 1: format = Format::S16Stereo; break;
        case 2: format = Format::S16_8Ch; break;
        case 3: format = Format::F32Mono; break;
        case 4: format = Format::F32Stereo; break;
        case 5: format = Format::F32_8Ch; break;
        case 6: format = Format::S16_8ChStd; break;
        case 7: format = Format::F32_8ChStd; break;
        default:
            throw std::runtime_error("sceAudioOutOpen: unknown format param");
    }

    if (type == PORT_TYPE_VIBRATION && channelsForFormat(format) > 2) throw std::runtime_error("sceAudioOutOpen: vibration layouts above stereo are not implemented");
    return allocatePort(type, len, freq, format);
}

int APS5_VABI sceAudioOutClose(int handle) {
    if (handle <= 0 || handle > PORTS_MAX) return -2144993277;
    std::lock_guard<std::mutex> outputLock(g_outputMutexes[handle - 1]);
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    closeDevice(*port);
    if (port->type == PORT_TYPE_VIBRATION) PadClearLegacyAudioHaptics_nid_postfix(handle);
    *port = Port{};
    return 0;
}

int APS5_VABI sceAudioOutOutput(int handle, const void* ptr) try {
    const AudioOutCall call;
    if (handle <= 0 || handle > PORTS_MAX) return -2144993277;
    std::lock_guard<std::mutex> outputLock(g_outputMutexes[handle - 1]);
    std::unique_lock<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }

    if (port->device == 0) {
        outputVirtualPort(*port, ptr, lock);
    } else {
        queueAudio(*port, ptr, lock);
    }
    if (ptr != nullptr) port->lastDataOutputTime = sceKernelGetProcessTime();
    return static_cast<int>(port->samplesNum);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceAudioOutOutputs(AudioOutOutputParam* param, std::uint32_t num) try {
    const AudioOutCall call;
    if (param == nullptr || num == 0) {
        return -2144993276;
    }

    std::array<bool, PORTS_MAX> selected{};
    for (std::uint32_t i = 0; i < num; ++i) {
        if (param[i].handle <= 0 || param[i].handle > PORTS_MAX) return -2144993277;
        selected[param[i].handle - 1] = true;
    }
    std::array<std::unique_lock<std::mutex>, PORTS_MAX> outputLocks;
    for (int index = 0; index < PORTS_MAX; ++index) {
        if (selected[index]) outputLocks[index] = std::unique_lock<std::mutex>(g_outputMutexes[index]);
    }
    std::unique_lock<std::mutex> lock(g_mutex);

    for (std::uint32_t i = 0; i < num; i++) {
        if (getPort(param[i].handle) == nullptr) {
            return -2144993277;
        }
    }

    Port& first = *getPort(param[0].handle);
    for (std::uint32_t i = 0; i < num; i++) {
        Port& port = *getPort(param[i].handle);
        if (port.device == 0) {
            outputVirtualPort(port, param[i].ptr, lock);
        } else {
            queueAudio(port, param[i].ptr, lock);
        }
    }

    const std::uint64_t done = sceKernelGetProcessTime();
    for (std::uint32_t i = 0; i < num; i++) {
        if (param[i].ptr != nullptr) getPort(param[i].handle)->lastDataOutputTime = done;
    }

    return static_cast<int>(first.samplesNum);
} catch (const ProcessShutdown&) {
    LibcAwaitExit_nid_postfix();
}

int APS5_VABI sceAudioOutSetVolume(int handle, std::uint32_t flag, int* vol) {
    if (vol == nullptr) {
        return -2144993276;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    const bool isStd = formatIsStd(port->format);
    for (int i = 0; i < port->channels; i++, flag >>= 1u) {
        if ((flag & 1u) == 0) {
            continue;
        }
        int srcIdx = i;
        if (isStd) {
            if (i == 4) srcIdx = 6;
            else if (i == 5) srcIdx = 7;
            else if (i == 6) srcIdx = 4;
            else if (i == 7) srcIdx = 5;
        }
        port->volume[i] = vol[srcIdx];
    }
    return 0;
}

int APS5_VABI sceAudioOutGetLastOutputTime(int handle, std::uint64_t* outputTime) {
    if (outputTime == nullptr) {
        return -2144993276;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    *outputTime = port->lastDataOutputTime;
    return 0;
}

int APS5_VABI sceAudioOutGetPortState(int handle, AudioOutPortState* state) {
    if (state == nullptr) {
        return -2144993276;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    state->rerouteCounter = 0;
    state->volume = 127;
    state->flag = 0;
    state->activeState = 0;
    state->reserved[0] = 0;
    switch (port->type) {
        case PORT_TYPE_MAIN:
        case PORT_TYPE_BGM:
        case PORT_TYPE_AUDIO3D:
            state->output = 1;
            state->channel = static_cast<std::uint8_t>(port->channels > 2 ? 2 : port->channels);
            break;
        case PORT_TYPE_VOICE:
        case PORT_TYPE_PERSONAL:
            state->output = 0x40;
            state->channel = 1;
            break;
        case PORT_TYPE_PADSPK:
        case PORT_TYPE_VIBRATION:
            state->output = 4;
            state->channel = 1;
            break;
        case PORT_TYPE_AUX:
            state->output = 0x80;
            state->channel = 0;
            break;
        default:
            throw std::runtime_error("sceAudioOutGetPortState: unknown port type");
    }
    return 0;
}

int APS5_VABI sceAudioOutDeviceIdOpen(std::uint64_t deviceId, std::int32_t type, std::int32_t index, std::uint32_t length, std::int32_t frequency, std::uint32_t parameter) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOutSysClose(int handle) {
    if (handle <= 0 || handle > PORTS_MAX) {
        throw std::runtime_error("sceAudioOutSysClose: handle " + std::to_string(handle) + " is not an open Sys port");
    }
    std::lock_guard<std::mutex> outputLock(g_outputMutexes[handle - 1]);
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr || port->type != PORT_TYPE_SYS) {
        throw std::runtime_error("sceAudioOutSysClose: handle " + std::to_string(handle) + " is not an open Sys port");
    }
    *port = Port{};
    return 0;
}

int APS5_VABI sceAudioOutSysConfigureOutput(int type, std::uint64_t flags, std::uint32_t mode, std::uint32_t target, std::uint64_t option) {
    if (type != SYS_CONFIGURE_TYPE_HDMI) {
        throw std::runtime_error("sceAudioOutSysConfigureOutput: output type " + std::to_string(type) + " not supported");
    }
    if (flags != 0 || option != 0) {
        throw std::runtime_error("sceAudioOutSysConfigureOutput: flags " + std::to_string(flags) + " and option " + std::to_string(option) + " not supported");
    }
    if (mode != SYS_OPEN_MODE_TRUEHD) {
        throw std::runtime_error("sceAudioOutSysConfigureOutput: mode " + std::to_string(mode) + " not supported");
    }
    if (target != SYS_CONFIGURE_TARGET_HDMI) {
        throw std::runtime_error("sceAudioOutSysConfigureOutput: target " + std::to_string(target) + " not supported");
    }
    return 0;
}

int APS5_VABI sceAudioOutSysGetHdmiMonitorInfo(int type, void* info, std::uint32_t size) {
    if (info == nullptr) return static_cast<int>(0x80260004);
    if (size != HDMI_MONITOR_INFO_SIZE) return static_cast<int>(0x80260006);
    if (type != MONITOR_TYPE_HDMI && type != 0x10) return static_cast<int>(0x8026000C);
    if (type == 0x10) throw std::runtime_error("sceAudioOutSysGetHdmiMonitorInfo: monitor type 16 not supported");
    std::memset(info, 0, HDMI_MONITOR_INFO_SIZE);
    return 0;
}

int APS5_VABI sceAudioOutSysOpen(int userId, std::uint32_t mode) {
    (void)userId;
    if (mode != SYS_OPEN_MODE_TRUEHD) {
        throw std::runtime_error("sceAudioOutSysOpen: mode " + std::to_string(mode) + " not supported");
    }
    return allocatePort(PORT_TYPE_SYS, SYS_PORT_SAMPLES, SYS_PORT_FRAME_RATE, Format::S16_8Ch);
}

int APS5_VABI sceAudioOutSetMainOutput() {
    return 0;
}

int APS5_VABI sceAudioOutSetMixLevelPadSpk(int handle, std::uint32_t mixLevel) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr) {
        return -2144993277;
    }
    if (mixLevel > DEFAULT_VOLUME) {
        return -2144993260;
    }
    if (port->type != PORT_TYPE_PADSPK) {
        return -2144993270;
    }
    port->mixLevel = mixLevel;
    return 0;
}

int APS5_VABI sceAudioOutExClose(int handle) {
    if (handle <= 0 || handle > PORTS_MAX) {
        throw std::runtime_error("sceAudioOutExClose: handle " + std::to_string(handle) + " is not an open Ex port");
    }
    std::lock_guard<std::mutex> outputLock(g_outputMutexes[handle - 1]);
    std::lock_guard<std::mutex> lock(g_mutex);
    Port* port = getPort(handle);
    if (port == nullptr || port->type != PORT_TYPE_EX) {
        throw std::runtime_error("sceAudioOutExClose: handle " + std::to_string(handle) + " is not an open Ex port");
    }
    *port = Port{};
    return 0;
}

int APS5_VABI sceAudioOutExConfigureOutput(int control, std::uint64_t flags, std::uint32_t mode, std::uint32_t target, std::uint64_t option) {
    if (control != 0) {
        return ERROR_EX_INVALID_CONTROL;
    }
    if (flags != 0 || option != 0) {
        throw std::runtime_error("sceAudioOutExConfigureOutput: flags " + std::to_string(flags) + " and option " + std::to_string(option) + " not supported");
    }
    if (mode > EX_OPEN_MODE_MAX && mode != EX_CONFIGURE_MODE_DEFAULT) {
        throw std::runtime_error("sceAudioOutExConfigureOutput: mode " + std::to_string(mode) + " not supported");
    }
    if ((target < 1 || target > 3) && target != EX_CONFIGURE_TARGET_DEFAULT) {
        throw std::runtime_error("sceAudioOutExConfigureOutput: target " + std::to_string(target) + " not supported");
    }
    return 0;
}

int APS5_VABI sceAudioOutExGetMonitorInfo(int type, int index, void* info, std::uint32_t size) {
    if (type != 0) return static_cast<int>(0x80260003);
    if (info == nullptr) return static_cast<int>(0x80260004);
    if (size != EX_MONITOR_INFO_SIZE) return static_cast<int>(0x80260006);
    std::memset(info, 0, EX_MONITOR_INFO_SIZE);
    return 0;
}

int APS5_VABI sceAudioOutExOpen(int userId, std::uint32_t mode) {
    (void)userId;
    if (mode > EX_OPEN_MODE_MAX || ((EX_OPEN_MODES >> mode) & 1) == 0) {
        return ERROR_EX_INVALID_MODE;
    }
    const bool highRate = mode == 3 || mode == 4 || mode == 10;
    return allocatePort(PORT_TYPE_EX, highRate ? 1024 : 256, highRate ? 192000 : 48000, Format::S16Stereo);
}

int APS5_VABI sceAudioOutGetSystemState(AudioOutSystemState* state) {
    if (state == nullptr) {
        return -2144993276;
    }
    *state = {};
    state->loudness = 0.0f;
    return 0;
}

}

extern "C" {
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/OutputDevice.hpp"
#include <algorithm>
#include <cstdlib>
#include <array>
#include <atomic>
#include <cerrno>
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
#include "prx/libSceAudioOut/include/HostGain.hpp"
#include "prx/libScePad/include/HostVolumeControl.hpp"
#include "AudioOutShutdown.hpp"
#include "AudioOut2PadMix.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libScePad/include/PadRumbleIntensity.hpp"

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
static constexpr std::uint64_t DEVICE_RETRY_US = 2000000;
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
    std::uint64_t nextDeviceRetry = 0;
    AudioOut2PadLayout padLayout;
    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec spec = {};
    HostGainRamp hostGain;
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

static const char* selectedOutputDevice(const char* requested) {
    if (requested == nullptr || *requested == '\0') return nullptr;
    const int count = SDL_GetNumAudioDevices(0);
    std::vector<std::string> names;
    for (int index = 0; index < count; ++index) {
        const char* name = SDL_GetAudioDeviceName(index, 0);
        if (name != nullptr) names.emplace_back(name);
    }
    if (FindOutputDevice(names, requested)) return requested;
    std::string listed;
    for (const auto& name : names) listed += "\n  " + name;
    APS5_LOG_OUT("AudioOut: ANYPS5_AUDIO_DEVICE '%s' is not an output device; using the default. Output devices:%s\n", requested, listed.c_str());
    return nullptr;
}

static bool openDevice(Port& port) {
    if (!ensureSdlAudio() || SdlAudioReportsNoOutputEndpoint(SDL_GetCurrentAudioDriver(), SDL_GetNumAudioDevices(0))) {
        return false;
    }
    SDL_AudioSpec desired{};
    desired.freq = static_cast<int>(port.freq);
    desired.format = sdlFormat(port.format);
    desired.channels = static_cast<Uint8>(port.channels);
    desired.samples = static_cast<Uint16>(port.samplesNum);
    desired.callback = nullptr;
    SDL_AudioSpec obtained{};
    port.device = SDL_OpenAudioDevice(selectedOutputDevice(std::getenv("ANYPS5_AUDIO_DEVICE")), 0, &desired, &obtained, 0);
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

static void reopenStoppedDevice(Port& port) {
    if (port.device == 0) {
        if (port.nextDeviceRetry == 0 || sceKernelGetProcessTime() < port.nextDeviceRetry) return;
    } else {
        if (SDL_GetAudioDeviceStatus(port.device) != SDL_AUDIO_STOPPED) return;
        closeDevice(port);
    }
    if (port.type == PORT_TYPE_VIBRATION) {
        try {
            openVibrationDevice(port);
        } catch (const std::runtime_error&) {
            port.device = 0;
            port.spec = {};
        }
    } else {
        openDevice(port);
        port.nextDeviceRetry = port.device == 0 ? sceKernelGetProcessTime() + DEVICE_RETRY_US : 0;
    }
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
    checkedBufferSize(port.samplesNum, AUDIO_OUT2_PAD_CHANNELS, sizeof(float));
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
        AudioOut2WritePadFrames(pad.data(), port.padLayout, padFrames.data(), port.samplesNum, Pad::RumblePercent());
        queueData = padFrames.data();
    } else {
        queueData = prepareBuffer(port, data, prepareBuf);
        queueData = HostGainFilter(port.hostGain, HostVolumeGain_nid_no_patch(), queueData, formatIsFloat(port.format), port.samplesNum, static_cast<unsigned>(port.channels), prepareBuf);
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
    if (data != nullptr) {
        checkedBufferSize(port.samplesNum, static_cast<std::uint32_t>(port.channels), bytesPerSample(port.format));
    }
    const auto pad = port.type == PORT_TYPE_VIBRATION && data != nullptr ? prepareVibration(port, data) : std::vector<float>{};
    paceVirtualPort(port, data != nullptr, lock);
    const auto source = static_cast<int>(&port - g_ports) + 1;
    if (port.type == PORT_TYPE_VIBRATION && data == nullptr) PadClearLegacyAudioHaptics_nid_postfix(source);
    if (!pad.empty() && PadHapticStreamOpen_nid_postfix()) {
        PadSubmitHapticFrames_nid_postfix(pad.data() + AUDIO_OUT2_PAD_VIBRATION_LEFT, pad.data() + AUDIO_OUT2_PAD_VIBRATION_RIGHT, AUDIO_OUT2_PAD_CHANNELS, port.samplesNum, port.freq);
    } else if (!pad.empty()) {
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
    if (handle <= 0 || handle > PORTS_MAX) return nullptr;
    const int idx = handle - 1;
    if (!g_ports[idx].used) {
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
            port.hostGain = HostGainRamp(freq / 100);
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
static constexpr int SCE_AUDIO_OUT_ERROR_INVALID_FORMAT = static_cast<int>(0x80260007);

bool AudioOut2IsInitialized_nid_no_patch() {
    return g_audioOut2Initialized.load();
}

void AudioOut2MarkInitialized_nid_no_patch() {
    g_audioOut2Initialized.store(true);
}

bool AudioOut2TryInitialize_nid_no_patch() {
    return !g_audioOut2Initialized.exchange(true);
}


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
    (void)index;
    if (!portTypeValid(type)) {
        return -2144993270;
    }
    if (len == 0) {
        return -2144993274;
    }
    if (freq == 0) {
        return -2144993272;
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
            return SCE_AUDIO_OUT_ERROR_INVALID_FORMAT;
    }

    if (type == PORT_TYPE_VIBRATION && channelsForFormat(format) > 2) throw std::runtime_error("sceAudioOutOpen: vibration layouts above stereo are not implemented");
    return allocatePort(type, len, freq, format);
}

}

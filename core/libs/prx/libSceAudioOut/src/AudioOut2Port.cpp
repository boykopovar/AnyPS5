#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <iterator>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "AudioOut2Internal.hpp"
#include "AudioOut2PadMix.hpp"

static constexpr std::uint16_t OUTPUT_MAIN = 1;
static constexpr std::int16_t VOLUME_MAX = 127;
// Attribute sets are traced in full for the first few calls of a port, then once per this many.
static constexpr std::uint64_t ATTRIBUTE_TRACE_FULL = 8;
static constexpr std::uint64_t ATTRIBUTE_TRACE_EVERY = 2000;

// Port attributes as Demon's Souls sets them every tick (APS5_TRACE_AUDIOOUT2): id 0 carries the
// grain's PCM as an 8-byte value holding the buffer pointer, id 1 one float gain per channel. Ids 5,
// 8 and 9 (a u32, a u32 and a byte on object and voice ports) are not understood and ignored.
static constexpr std::uint32_t ATTRIBUTE_DATA = 0;
static constexpr std::uint32_t ATTRIBUTE_VOLUME = 1;
static constexpr std::size_t QUEUED_DATA_MAX = 16;

static constexpr std::uint32_t FORMAT_CHANNELS_SHIFT = 8;
static constexpr std::uint32_t FORMAT_CHANNELS_MASK = 0xFu;
static constexpr std::uint32_t FORMAT_TYPE_MASK = 0x7Fu;
static constexpr std::uint32_t FORMAT_FIELDS_MASK = 0xFFFu;
static constexpr std::uint16_t PORT_TYPE_MAIN = 0;
static constexpr int PORT_ERROR_FORMAT_FLAG_WITHOUT_8_CHANNELS = static_cast<int>(0x80268001);
static constexpr int PORT_ERROR_UNSUPPORTED_FORMAT = static_cast<int>(0x8026800E);

static std::mutex g_portsLock;
// Grows on demand: the title opens its bed ports plus max_object_ports object ports at once.
static std::vector<AudioOut2Port> g_ports;
static std::uint64_t g_portGeneration = 0;

static AudioOut2Port* FromHandle(AudioOut2PortHandle handle) {
    const auto index = handle - 1;
    if (handle == 0 || index >= g_ports.size() || !g_ports[index].used) return nullptr;
    return &g_ports[index];
}

static constexpr float FOLD_GAIN = 0.7071f;
static constexpr float FOLD_GAIN_TWICE = FOLD_GAIN * FOLD_GAIN;

struct AudioOut2StereoFold {
    std::uint32_t channels;
    float left[AUDIO_OUT2_PORT_CHANNELS_MAX];
    float right[AUDIO_OUT2_PORT_CHANNELS_MAX];
};

static constexpr AudioOut2StereoFold STEREO_FOLDS[] = {
    {1, {1.0f}, {1.0f}},
    {2, {1.0f, 0.0f}, {0.0f, 1.0f}},
    {6, {1.0f, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN, 0.0f}, {0.0f, 1.0f, FOLD_GAIN, 0.0f, 0.0f, FOLD_GAIN}},
    {8, {1.0f, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN, 0.0f}, {0.0f, 1.0f, FOLD_GAIN, 0.0f, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN}},
    {12, {1.0f, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN_TWICE, 0.0f},
        {0.0f, 1.0f, FOLD_GAIN, 0.0f, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN, 0.0f, FOLD_GAIN_TWICE}},
};

static const AudioOut2StereoFold* StereoFoldFor(std::uint32_t channels) {
    const auto found = std::find_if(std::begin(STEREO_FOLDS), std::end(STEREO_FOLDS), [channels](const AudioOut2StereoFold& fold) { return fold.channels == channels; });
    return found == std::end(STEREO_FOLDS) ? nullptr : found;
}

static void ReadFrame(const AudioOut2Port& port, const void* data, std::uint32_t frame, float* in) {
    const auto first = static_cast<std::size_t>(frame) * port.channels;
    for (std::uint32_t c = 0; c < port.channels; c++) {
        in[c] = port.int16 ? static_cast<const std::int16_t*>(data)[first + c] / 32768.0f : static_cast<const float*>(data)[first + c];
    }
}

static void AccumulatePadPort(const AudioOut2Port& port, const void* data, AudioOut2Route route, float* out, std::uint32_t frames) {
    float in[AUDIO_OUT2_PORT_CHANNELS_MAX];
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        ReadFrame(port, data, frame, in);
        AudioOut2AccumulatePadFrame(route, in, port.channels, port.volume, out + static_cast<std::size_t>(frame) * AUDIO_OUT2_PAD_CHANNELS);
    }
}

static void AccumulatePort(const AudioOut2Port& port, const void* data, float* out, std::uint32_t frames) {
    const auto& fold = *port.fold;
    float in[AUDIO_OUT2_PORT_CHANNELS_MAX];
    for (std::uint32_t frame = 0; frame < frames; frame++) {
        ReadFrame(port, data, frame, in);
        float left = 0.0f;
        float right = 0.0f;
        for (std::uint32_t c = 0; c < port.channels; c++) {
            const float sample = in[c] * port.volume[c];
            if (fold.left[c] != 0.0f) left += sample * fold.left[c];
            if (fold.right[c] != 0.0f) right += sample * fold.right[c];
        }
        out[frame * AUDIO_OUT2_OUTPUT_CHANNELS] += left;
        out[frame * AUDIO_OUT2_OUTPUT_CHANNELS + 1] += right;
    }
}

AudioOut2Grain AudioOut2CaptureGrain(const AudioOut2Context& context) {
    std::lock_guard lock(g_portsLock);
    AudioOut2Grain grain;
    for (std::size_t index = 0; index < g_ports.size(); index++) {
        auto& port = g_ports[index];
        if (!port.used || port.context != &context || port.channels == 0) continue;
        const void* data = port.repeatedData;
        if (!port.queuedData.empty()) {
            data = port.queuedData.front();
            port.queuedData.pop_front();
        }
        if (data != nullptr) grain.push_back({index, port.generation, data});
    }
    return grain;
}

std::uint32_t AudioOut2MixPorts(const AudioOut2Context& context, const AudioOut2Grain& grain, float* out, float* padOut, std::uint32_t frames) {
    std::lock_guard lock(g_portsLock);
    std::uint32_t mixed = 0;
    for (const auto& [index, generation, data] : grain) {
        if (index >= g_ports.size()) continue;
        const auto& port = g_ports[index];
        if (!port.used || port.generation != generation || port.context != &context || port.channels == 0) continue;
        auto route = AudioOut2RouteForPort(port.type, port.channels);
        if (padOut == nullptr) {
            if (route == AudioOut2Route::PadVibration) continue;
            route = AudioOut2Route::Main;
        }
        if (route == AudioOut2Route::Main) AccumulatePort(port, data, out, frames);
        else AccumulatePadPort(port, data, route, padOut, frames);
        mixed++;
    }
    return mixed;
}

bool AudioOut2HasPadPorts(const AudioOut2Context& context) {
    std::lock_guard lock(g_portsLock);
    return std::any_of(g_ports.begin(), g_ports.end(), [&context](const AudioOut2Port& port) {
        return port.used && port.context == &context && AudioOut2RouteForPort(port.type, port.channels) != AudioOut2Route::Main;
    });
}

void AudioOut2ReleasePorts(const AudioOut2Context& context) {
    std::lock_guard lock(g_portsLock);
    for (auto& port : g_ports) {
        if (port.used && port.context == &context) port = AudioOut2Port{};
    }
}

static void TraceAttribute(AudioOut2PortHandle handle, const AudioOut2Port& port, const AudioOut2Attribute& attribute, const char* verdict) {
    if (!AudioOut2TraceEnabled()) return;
    char values[128] = "";
    if (attribute.value != nullptr) {
        std::size_t used = 0;
        const auto words = std::min<std::size_t>(attribute.value_size / sizeof(std::uint32_t), 4);
        for (std::size_t index = 0; index < words && used < sizeof(values); index++) {
            std::uint32_t bits = 0;
            float value = 0.0f;
            std::memcpy(&bits, static_cast<const std::uint8_t*>(attribute.value) + index * sizeof(bits), sizeof(bits));
            std::memcpy(&value, &bits, sizeof(value));
            used += static_cast<std::size_t>(std::snprintf(values + used, sizeof(values) - used, " %08x(%g)", bits, static_cast<double>(value)));
        }
    }
    std::fprintf(stderr, "[audioout2] t=%.3f port %llu set attribute id=0x%x size=%zu value=%p%s: %s (data sets so far %llu)\n",
        AudioOut2TraceSeconds(), static_cast<unsigned long long>(handle), attribute.attribute_id, attribute.value_size, attribute.value, values, verdict,
        static_cast<unsigned long long>(port.dataSets));
}

extern "C" {

int APS5_VABI sceAudioOut2PortCreate(AudioOut2ContextHandle ctx, const AudioOut2PortParam* params, AudioOut2PortHandle* port) {
    if (!ctx) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    if (!params || !port) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    const auto channels = (params->data_format >> FORMAT_CHANNELS_SHIFT) & FORMAT_CHANNELS_MASK;
    const auto sampleType = params->data_format & FORMAT_TYPE_MASK;
    const auto* fold = StereoFoldFor(channels);
    if (params->port_type == PORT_TYPE_MAIN) {
        if ((params->data_format & 0x80u) != 0 && channels != 8) {
            *port = static_cast<AudioOut2PortHandle>(-1);
            return PORT_ERROR_FORMAT_FLAG_WITHOUT_8_CHANNELS;
        }
        if (fold == nullptr || channels == 6 || sampleType > 1) {
            *port = static_cast<AudioOut2PortHandle>(-1);
            return PORT_ERROR_UNSUPPORTED_FORMAT;
        }
    }
    std::lock_guard lock(g_portsLock);
    std::size_t index = 0;
    while (index < g_ports.size() && g_ports[index].used) index++;
    if (index == g_ports.size()) g_ports.emplace_back();
    auto& entry = g_ports[index];
    entry = AudioOut2Port{};
    entry.used = true;
    entry.generation = ++g_portGeneration;
    entry.context = reinterpret_cast<AudioOut2Context*>(ctx);
    entry.type = params->port_type;
    entry.dataFormat = params->data_format;
    entry.samplingFreq = params->sampling_freq;
    entry.flags = params->flags;
    entry.channels = channels;
    entry.fold = fold;
    if (entry.fold == nullptr || sampleType > 1 || (params->port_type != PORT_TYPE_MAIN && (params->data_format & ~FORMAT_FIELDS_MASK) != 0)) {
        entry = AudioOut2Port{};
        throw std::runtime_error("sceAudioOut2PortCreate: data format 0x" + [&] { char text[16]; std::snprintf(text, sizeof(text), "%x", params->data_format); return std::string(text); }() + " is not implemented");
    }
    entry.int16 = sampleType == 1;
    *port = static_cast<AudioOut2PortHandle>(index) + 1;
    AUDIOOUT2_TRACE("t=%.3f PortCreate ctx=%llx -> port %llu: type=0x%x data_format=0x%x (%u float ch) sampling_freq=%u flags=0x%x user=%llx\n",
        AudioOut2TraceSeconds(), static_cast<unsigned long long>(ctx), static_cast<unsigned long long>(*port), params->port_type, params->data_format,
        entry.channels, params->sampling_freq, params->flags, static_cast<unsigned long long>(params->user_handle));
    return 0;
}

int APS5_VABI sceAudioOut2PortDestroy(AudioOut2PortHandle port) {
    std::lock_guard lock(g_portsLock);
    auto* entry = FromHandle(port);
    if (!entry) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    AUDIOOUT2_TRACE("t=%.3f PortDestroy port %llu (data sets %llu)\n", AudioOut2TraceSeconds(), static_cast<unsigned long long>(port), static_cast<unsigned long long>(entry->dataSets));
    *entry = AudioOut2Port{};
    return 0;
}

int APS5_VABI sceAudioOut2PortGetState(AudioOut2PortHandle port, AudioOut2PortState* state) {
    std::lock_guard lock(g_portsLock);
    if (!FromHandle(port)) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    if (!state) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    state->output = OUTPUT_MAIN;
    state->num_channels = AUDIO_OUT2_OUTPUT_CHANNELS;
    state->volume = VOLUME_MAX;
    state->reroute_counter = 0;
    state->flags = 0;
    return 0;
}

int APS5_VABI sceAudioOut2PortSetAttributes(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num) {
    std::lock_guard lock(g_portsLock);
    auto* entry = FromHandle(port);
    if (!entry) return SCE_AUDIO_OUT2_ERROR_INVALID_HANDLE;
    if (!attributes && num != 0) return SCE_AUDIO_OUT2_ERROR_INVALID_ARGUMENT;
    for (uint32_t index = 0; index < num; index++) {
        const auto& attribute = attributes[index];
        const char* verdict = "ignored";
        if (attribute.value == nullptr) {
            verdict = "null value";
        } else if (attribute.attribute_id == ATTRIBUTE_DATA && attribute.value_size == sizeof(entry->data)) {
            std::memcpy(&entry->data, attribute.value, sizeof(entry->data));
            entry->queuedData.push_back(entry->data);
            if (entry->queuedData.size() > QUEUED_DATA_MAX) entry->queuedData.pop_front();
            entry->repeatedData = entry->data != nullptr && (entry->lastBuffer == nullptr || entry->lastBuffer == entry->data) ? entry->data : nullptr;
            if (entry->data != nullptr) entry->lastBuffer = entry->data;
            entry->dataSets++;
            verdict = "pcm data pointer";
        } else if (attribute.attribute_id == ATTRIBUTE_VOLUME && entry->channels != 0 && attribute.value_size == entry->channels * sizeof(float)) {
            std::memcpy(entry->volume, attribute.value, attribute.value_size);
            verdict = "volume";
        }
        const auto traces = entry->attributeTraces++;
        if (traces < ATTRIBUTE_TRACE_FULL || traces % ATTRIBUTE_TRACE_EVERY == 0) TraceAttribute(port, *entry, attribute, verdict);
    }
    return 0;
}

}

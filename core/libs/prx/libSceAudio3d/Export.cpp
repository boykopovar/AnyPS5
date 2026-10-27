#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <iterator>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "Mixer.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int AUDIO3D_ERROR_INVALID_PORT = static_cast<int>(0x80EA0002);
constexpr int AUDIO3D_ERROR_INVALID_OBJECT = static_cast<int>(0x80EA0003);
constexpr int AUDIO3D_ERROR_INVALID_PARAMETER = static_cast<int>(0x80EA0004);
constexpr int AUDIO3D_ERROR_NOT_SUPPORTED = static_cast<int>(0x80EA0008);
constexpr int AUDIO3D_ERROR_OUT_OF_RESOURCES = static_cast<int>(0x80EA0006);
constexpr int AUDIO3D_ERROR_NOT_READY = static_cast<int>(0x80EA0007);

constexpr int AUDIO3D_USER_ID_SYSTEM = 0xFF;
constexpr std::uint32_t AUDIO3D_PORT_ID = 0;
constexpr std::uint32_t AUDIO3D_OBJECT_INVALID = 0xFFFFFFFF;
constexpr std::uint32_t AUDIO3D_RATE_48000 = 0;
constexpr std::uint32_t AUDIO3D_SAMPLE_RATE = 48000;
constexpr std::uint32_t AUDIO3D_BUFFER_ADVANCE_AND_PUSH = 2;
constexpr int AUDIO3D_AUDIO_OUT_PORT_TYPE = 126;
constexpr std::uint32_t AUDIO3D_AUDIO_OUT_FORMAT_STEREO = 1;
constexpr std::uint32_t AUDIO3D_BLOCKING_ASYNC = 0;
constexpr std::uint32_t AUDIO3D_BLOCKING_SYNC = 1;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_RESET_STATE = 0x20000;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_LATE_REVERB_LEVEL = 0x10001;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_RADIUS = 0x10002;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_HEIGHT_AWARE = 0x10003;
constexpr std::uint32_t AUDIO3D_FORMAT_S16 = 0;
constexpr std::uint32_t AUDIO3D_FORMAT_FLOAT = 1;
constexpr std::uint32_t AUDIO3D_OUTPUT_ROUTE_BOTH = 0;
constexpr std::uint32_t AUDIO3D_OUTPUT_ROUTE_HMU_ONLY = 1;
constexpr std::uint32_t AUDIO3D_OUTPUT_ROUTE_TV_ONLY = 2;

struct Audio3dAudioOut {
    std::size_t buffer_bytes = 0;
    std::deque<std::vector<std::uint8_t>> pending;
};

struct Audio3dBed {
    std::vector<std::uint8_t> pcm;
    std::uint32_t channels = 0;
    std::uint32_t format = 0;
    std::uint32_t samples = 0;
};

struct Audio3dPort {
    bool open = false;
    std::uint32_t queue_depth = 0;
    std::uint32_t granularity = 0;
    std::uint32_t buffer_mode = 0;
    std::uint32_t max_objects = 0;
    std::uint32_t next_object_id = 0;
    std::set<std::uint32_t> objects;
    std::deque<Audio3dBed> beds;
    std::deque<std::vector<std::int16_t>> mixed;
    int audio_out_handle = 0;
    std::map<int, Audio3dAudioOut> audio_out_ports;
};

std::mutex g_mutex;
bool g_initialized = false;
Audio3dPort g_port;

[[noreturn]] void Unsupported(const char* function, const std::string& what) {
    throw std::runtime_error(std::string(function) + ": " + what);
}

bool PortIsOpen(std::uint32_t port_id) {
    return port_id == AUDIO3D_PORT_ID && g_port.open;
}

std::uint32_t QueueLevel() {
    return static_cast<std::uint32_t>(g_port.mixed.size());
}

std::size_t AudioOutBufferBytes(std::uint32_t len, std::uint32_t param) {
    static constexpr std::uint32_t channels[] = {1, 2, 8, 1, 2, 8, 8, 8};
    static constexpr std::uint32_t sample_size[] = {2, 2, 2, 4, 4, 4, 2, 4};
    const std::uint32_t format = param & 0xFF;
    if (format >= std::size(channels)) return 0;
    return static_cast<std::size_t>(len) * channels[format] * sample_size[format];
}

int DrainAssociatedAudioOuts() {
    while (true) {
        int handle = 0;
        std::vector<std::uint8_t> frame;
        {
            std::lock_guard lock(g_mutex);
            const auto entry = std::find_if(g_port.audio_out_ports.begin(), g_port.audio_out_ports.end(),
                [](const auto& candidate) { return !candidate.second.pending.empty(); });
            if (entry == g_port.audio_out_ports.end()) return 0;
            handle = entry->first;
            frame = std::move(entry->second.pending.front());
            entry->second.pending.pop_front();
        }
        const int result = sceAudioOutOutput(handle, frame.data());
        if (result < 0) {
            std::lock_guard lock(g_mutex);
            const auto entry = g_port.audio_out_ports.find(handle);
            if (entry != g_port.audio_out_ports.end()) entry->second.pending.push_front(std::move(frame));
            return result;
        }
    }
}

}

extern "C" {

void APS5_VABI sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* p) {
    if (p == nullptr) APS5_INVALID_ARG_EX;
    constexpr Audio3dOpenParameters defaults{0x20, 256, 0, 512, 2, 2, 0, 0};
    static_assert(offsetof(Audio3dOpenParameters, num_beds) == 0x20);
    static_assert(sizeof(Audio3dOpenParameters) == 0x28);
    std::memcpy(p, &defaults, defaults.size_this);
}

int APS5_VABI sceAudio3dInitialize(int64_t reserved) {
    if (reserved != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    std::lock_guard lock(g_mutex);
    if (g_initialized) Unsupported(__func__, "repeated initialization");
    g_initialized = true;
    return 0;
}

int APS5_VABI sceAudio3dTerminate() {
    std::lock_guard lock(g_mutex);
    if (!g_initialized || g_port.open) return AUDIO3D_ERROR_NOT_READY;
    g_initialized = false;
    return 0;
}

int APS5_VABI sceAudio3dPortAdvance(uint32_t port_id) {
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    if (g_port.buffer_mode != AUDIO3D_BUFFER_ADVANCE_AND_PUSH) return AUDIO3D_ERROR_NOT_SUPPORTED;
    if (QueueLevel() >= g_port.queue_depth) return AUDIO3D_ERROR_NOT_READY;
    std::vector<std::int16_t> frame;
    if (g_port.beds.empty()) {
        Audio3dInternal::MixFrame(nullptr, 0, g_port.granularity, frame);
    } else {
        const Audio3dBed& bed = g_port.beds.front();
        const Audio3dInternal::Source source{
            bed.pcm.data(),
            bed.samples,
            bed.channels,
            bed.format == AUDIO3D_FORMAT_FLOAT,
            1.0f,
            0.0f,
        };
        Audio3dInternal::MixFrame(&source, 1, g_port.granularity, frame);
        g_port.beds.pop_front();
    }
    g_port.mixed.push_back(std::move(frame));
    return 0;
}

int APS5_VABI sceAudio3dPortClose(uint32_t port_id) {
    int handle = 0;
    std::vector<int> associated;
    {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        handle = g_port.audio_out_handle;
        for (const auto& entry : g_port.audio_out_ports) associated.push_back(entry.first);
        g_port = Audio3dPort{};
    }
    for (const int port : associated) sceAudioOutClose(port);
    if (handle != 0) sceAudioOutClose(handle);
    return 0;
}

int APS5_VABI sceAudio3dPortGetQueueLevel(uint32_t port_id, uint32_t* queue_level, uint32_t* queue_available) {
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    if (queue_level == nullptr && queue_available == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    const std::uint32_t level = QueueLevel();
    if (queue_level != nullptr) *queue_level = level;
    if (queue_available != nullptr) *queue_available = g_port.queue_depth - level;
    return 0;
}

int APS5_VABI sceAudio3dPortGetAttributesSupported(uint32_t portId, uint32_t* attributes, uint32_t* numAttributes) {
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(portId)) return AUDIO3D_ERROR_INVALID_PORT;
    if (numAttributes == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    constexpr std::uint32_t supported[]{
        AUDIO3D_ATTRIBUTE_LATE_REVERB_LEVEL,
        AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_RADIUS,
        AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_HEIGHT_AWARE,
    };
    *numAttributes = std::size(supported);
    if (attributes != nullptr) std::copy(std::begin(supported), std::end(supported), attributes);
    return 0;
}

int APS5_VABI sceAudio3dPortOpen(int user_id, const Audio3dOpenParameters* parameters, uint32_t* id) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return AUDIO3D_ERROR_NOT_READY;
    if (user_id != AUDIO3D_USER_ID_SYSTEM || parameters == nullptr || id == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    std::uint32_t num_beds = 2;
    switch (parameters->size_this) {
    case 0x10:
    case 0x18:
        Unsupported(__func__, "parameter size " + std::to_string(parameters->size_this) + " selects an unsupported buffer mode");
    case 0x20:
        break;
    case 0x28:
        num_beds = parameters->num_beds;
        break;
    default:
        return AUDIO3D_ERROR_INVALID_PARAMETER;
    }
    if (parameters->rate != AUDIO3D_RATE_48000) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->granularity < 0x100 || (parameters->granularity & 0xFF) != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->max_objects == 0 || parameters->queue_depth == 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->buffer_mode > AUDIO3D_BUFFER_ADVANCE_AND_PUSH) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (num_beds != 2 && num_beds != 3) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->buffer_mode != AUDIO3D_BUFFER_ADVANCE_AND_PUSH) {
        Unsupported(__func__, "buffer mode " + std::to_string(parameters->buffer_mode));
    }
    if (num_beds != 2) Unsupported(__func__, "3 beds");
    if (g_port.open) return AUDIO3D_ERROR_OUT_OF_RESOURCES;
    g_port = Audio3dPort{};
    g_port.open = true;
    g_port.queue_depth = parameters->queue_depth;
    g_port.granularity = parameters->granularity;
    g_port.buffer_mode = parameters->buffer_mode;
    g_port.max_objects = parameters->max_objects;
    *id = AUDIO3D_PORT_ID;
    return 0;
}

int APS5_VABI sceAudio3dPortPush(uint32_t port_id, uint32_t blocking) {
    int handle = 0;
    std::uint32_t granularity = 0;
    {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        if (blocking != AUDIO3D_BLOCKING_ASYNC && blocking != AUDIO3D_BLOCKING_SYNC) {
            Unsupported(__func__, "blocking mode " + std::to_string(blocking));
        }
        handle = g_port.audio_out_handle;
        granularity = g_port.granularity;
    }
    if (handle == 0) {
        const int opened = sceAudioOutOpen(AUDIO3D_USER_ID_SYSTEM, AUDIO3D_AUDIO_OUT_PORT_TYPE, 0,
            granularity, AUDIO3D_SAMPLE_RATE, AUDIO3D_AUDIO_OUT_FORMAT_STEREO);
        if (opened < 0) return AUDIO3D_ERROR_OUT_OF_RESOURCES;
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) {
            sceAudioOutClose(opened);
            return AUDIO3D_ERROR_INVALID_PORT;
        }
        if (g_port.audio_out_handle == 0) g_port.audio_out_handle = opened;
        else sceAudioOutClose(opened);
    }
    std::deque<std::vector<std::int16_t>> frames;
    {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        handle = g_port.audio_out_handle;
        const std::size_t target = blocking == AUDIO3D_BLOCKING_SYNC ? 0 : g_port.queue_depth - 1;
        while (g_port.mixed.size() > target) {
            frames.push_back(std::move(g_port.mixed.front()));
            g_port.mixed.pop_front();
        }
    }
    for (const std::vector<std::int16_t>& frame : frames) {
        const int result = sceAudioOutOutput(handle, frame.data());
        if (result < 0) return result;
    }
    return DrainAssociatedAudioOuts();
}

int APS5_VABI sceAudio3dPortSetAttribute(uint32_t port_id, uint32_t attribute_id, const void* attribute, size_t attribute_size) {
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    if (attribute == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (attribute_size != 4) Unsupported(__func__, "attribute size " + std::to_string(attribute_size));
    switch (attribute_id) {
    case AUDIO3D_ATTRIBUTE_LATE_REVERB_LEVEL:
    case AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_RADIUS:
    case AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_HEIGHT_AWARE:
        return 0;
    default:
        Unsupported(__func__, "attribute " + std::to_string(attribute_id));
    }
}

int APS5_VABI sceAudio3dObjectReserve(uint32_t port_id, uint32_t* object_id) {
    if (object_id == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    *object_id = AUDIO3D_OBJECT_INVALID;
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    if (g_port.objects.size() >= g_port.max_objects) return AUDIO3D_ERROR_OUT_OF_RESOURCES;
    do {
        ++g_port.next_object_id;
    } while (g_port.next_object_id == 0 || g_port.next_object_id == AUDIO3D_OBJECT_INVALID || g_port.objects.contains(g_port.next_object_id));
    g_port.objects.insert(g_port.next_object_id);
    *object_id = g_port.next_object_id;
    return 0;
}

int APS5_VABI sceAudio3dObjectUnreserve(uint32_t port_id, uint32_t object_id) {
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    if (g_port.objects.erase(object_id) == 0) return AUDIO3D_ERROR_INVALID_OBJECT;
    return 0;
}

int APS5_VABI sceAudio3dObjectSetAttributes(uint32_t port_id, uint32_t object_id, uint64_t num_attributes, const Audio3dAttribute* attribute_array) {
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    if (num_attributes == 0 || attribute_array == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (!g_port.objects.contains(object_id)) return AUDIO3D_ERROR_INVALID_OBJECT;
    for (std::uint64_t i = 0; i < num_attributes; ++i) {
        if (attribute_array[i].attribute_id == AUDIO3D_ATTRIBUTE_RESET_STATE) continue;
        if (attribute_array[i].value == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    }
    return 0;
}

int APS5_VABI sceAudio3dPortFlush(uint32_t port_id) {
    int handle = 0;
    std::vector<int> associated;
    {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        g_port.mixed.clear();
        handle = g_port.audio_out_handle;
        for (auto& entry : g_port.audio_out_ports) {
            entry.second.pending.clear();
            associated.push_back(entry.first);
        }
    }
    for (const int port : associated) sceAudioOutOutput(port, nullptr);
    if (handle != 0) sceAudioOutOutput(handle, nullptr);
    return 0;
}

int APS5_VABI sceAudio3dBedWrite2(uint32_t port_id, uint32_t num_channels, uint32_t format, const void* buffer, uint32_t num_samples, uint32_t output_route, bool) {
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    if (num_channels != 2 && num_channels != 6 && num_channels != 8) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (format > AUDIO3D_FORMAT_FLOAT) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (output_route != AUDIO3D_OUTPUT_ROUTE_BOTH && output_route != AUDIO3D_OUTPUT_ROUTE_HMU_ONLY && output_route != AUDIO3D_OUTPUT_ROUTE_TV_ONLY) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (buffer == nullptr || num_samples == 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    const std::size_t sample_size = format == AUDIO3D_FORMAT_S16 ? 2u : 4u;
    if (reinterpret_cast<std::uintptr_t>(buffer) % sample_size != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    const std::uint32_t samples = std::min(num_samples, g_port.granularity);
    const std::size_t bytes = static_cast<std::size_t>(samples) * num_channels * sample_size;
    if (g_port.beds.size() >= g_port.queue_depth) return AUDIO3D_ERROR_NOT_READY;
    const auto* first = static_cast<const std::uint8_t*>(buffer);
    Audio3dBed bed;
    bed.pcm.assign(first, first + bytes);
    bed.channels = num_channels;
    bed.format = format;
    bed.samples = samples;
    g_port.beds.push_back(std::move(bed));
    return 0;
}

int APS5_VABI sceAudio3dBedWrite(uint32_t port_id, uint32_t num_channels, uint32_t format, const void* buffer, uint32_t num_samples) {
    return sceAudio3dBedWrite2(port_id, num_channels, format, buffer, num_samples, AUDIO3D_OUTPUT_ROUTE_BOTH, false);
}

int APS5_VABI sceAudio3dAudioOutOpen(uint32_t port_id, int user_id, std::int32_t type, std::int32_t index, uint32_t len, uint32_t freq, uint32_t param) {
    {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        if (len != g_port.granularity) return AUDIO3D_ERROR_INVALID_PARAMETER;
        if (index != 0 || (param & 0xFF) > 7) return AUDIO3D_ERROR_INVALID_PARAMETER;
    }
    const int handle = sceAudioOutOpen(user_id, type, index, len, freq, param);
    if (handle < 0) return AUDIO3D_ERROR_OUT_OF_RESOURCES;
    bool registered = false;
    {
        std::lock_guard lock(g_mutex);
        if (PortIsOpen(port_id)) {
            g_port.audio_out_ports.emplace(handle, Audio3dAudioOut{AudioOutBufferBytes(len, param)});
            registered = true;
        }
    }
    if (!registered) {
        sceAudioOutClose(handle);
        return AUDIO3D_ERROR_INVALID_PORT;
    }
    return handle;
}

int APS5_VABI sceAudio3dAudioOutClose(std::int32_t handle) {
    {
        std::lock_guard lock(g_mutex);
        if (g_port.audio_out_ports.erase(handle) == 0) return AUDIO3D_ERROR_INVALID_PORT;
    }
    sceAudioOutClose(handle);
    return 0;
}

int APS5_VABI sceAudio3dAudioOutOutput(std::int32_t handle, const void* ptr) {
    std::lock_guard lock(g_mutex);
    const auto port = g_port.audio_out_ports.find(handle);
    if (port == g_port.audio_out_ports.end()) return AUDIO3D_ERROR_INVALID_PORT;
    if (ptr == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (port->second.pending.size() >= g_port.queue_depth) return AUDIO3D_ERROR_NOT_READY;
    const auto* first = static_cast<const std::uint8_t*>(ptr);
    port->second.pending.emplace_back(first, first + port->second.buffer_bytes);
    return static_cast<int>(g_port.granularity);
}

int APS5_VABI sceAudio3dAudioOutOutputs(AudioOutOutputParam* param, uint32_t num) {
    if (param == nullptr || num == 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    for (std::uint32_t i = 0; i < num; ++i) {
        const int result = sceAudio3dAudioOutOutput(param[i].handle, param[i].ptr);
        if (result < 0) return result;
    }
    return 0;
}

const char* APS5_VABI sceAudio3dStrError(std::int32_t error) {
    static constexpr const char* descriptions[]{
        "port does not exist", "object does not exist", "invalid parameter",
        "out of memory", "out of resources", "not ready"
    };
    const auto index = static_cast<std::uint32_t>(error) - 0x80EA0002u;
    return index < std::size(descriptions) ? descriptions[index] : "(unknown)";
}

int APS5_VABI sceAudio3dPortOpenEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudio3dPortCreate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudio3dPortDestroy(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudio3dPortGetStatus(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudio3dReportRegisterHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudio3dReportUnregisterHandler(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

static_assert(sizeof(Audio3dSpeakerArrayParameters) == 0x20);

size_t APS5_VABI sceAudio3dGetSpeakerArrayMemorySize(uint32_t num_speakers, uint8_t is_3d) {
    return sceAudioOut2GetSpeakerArrayMemorySize(num_speakers, is_3d, 0);
}

int APS5_VABI sceAudio3dCreateSpeakerArray(AudioOut2SpeakerArrayHandle* handle, const Audio3dSpeakerArrayParameters* parameters) {
    if (!handle || !parameters) return AUDIO3D_ERROR_INVALID_PARAMETER;
    unsigned char extended[0x28] = {};
    std::memcpy(extended, parameters, sizeof(*parameters));
    return sceAudioOut2SpeakerArrayCreate(handle, extended, nullptr);
}

int APS5_VABI sceAudio3dDeleteSpeakerArray(AudioOut2SpeakerArrayHandle handle) {
    return sceAudioOut2SpeakerArrayDestroy(handle);
}

int APS5_VABI sceAudio3dGetSpeakerArrayMixCoefficients(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, uint32_t num_coefficients) {
    return sceAudioOut2GetSpeakerArrayCoefficients(handle, pos, spread, coefficients, num_coefficients, 0, 0.1f);
}

int APS5_VABI sceAudio3dGetSpeakerArrayMixCoefficients2(AudioOut2SpeakerArrayHandle handle, AudioOut2Position pos, float spread, float* coefficients, uint32_t num_coefficients, uint8_t height_aware, float downmix_spread_radius) {
    return sceAudioOut2GetSpeakerArrayCoefficients(handle, pos, spread, coefficients, num_coefficients, height_aware, downmix_spread_radius);
}

}

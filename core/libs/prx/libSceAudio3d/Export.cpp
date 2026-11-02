#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
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
#include <type_traits>
#include <vector>
#include "Mixer.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel_sys/include/Api.hpp"

namespace {

constexpr int AUDIO3D_ERROR_INVALID_PORT = static_cast<int>(0x80EA0002);
constexpr int AUDIO3D_ERROR_INVALID_OBJECT = static_cast<int>(0x80EA0003);
constexpr int AUDIO3D_ERROR_INVALID_PARAMETER = static_cast<int>(0x80EA0004);
constexpr int AUDIO3D_ERROR_NOT_SUPPORTED = static_cast<int>(0x80EA0008);
constexpr int AUDIO3D_ERROR_OUT_OF_RESOURCES = static_cast<int>(0x80EA0006);
constexpr int AUDIO3D_ERROR_NOT_READY = static_cast<int>(0x80EA0007);
constexpr int AUDIO_OUT_ERROR_INVALID_FORMAT = static_cast<int>(0x80260007);
constexpr int AUDIO_OUT_ERROR_INVALID_PORT_TYPE = static_cast<int>(0x8026000A);
constexpr int AUDIO_OUT_ERROR_ALREADY_INIT = static_cast<int>(0x8026000E);

constexpr int AUDIO3D_USER_ID_SYSTEM = 0xFF;
constexpr std::uint32_t AUDIO3D_PORT_ID = 0;
constexpr std::uint32_t AUDIO3D_OBJECT_INVALID = 0xFFFFFFFF;
constexpr std::uint32_t AUDIO3D_RATE_48000 = 0;
constexpr std::uint32_t AUDIO3D_SAMPLE_RATE = 48000;
constexpr std::uint32_t AUDIO3D_DEFAULT_MAX_OBJECTS = 512;
constexpr std::uint32_t AUDIO3D_DEFAULT_QUEUE_DEPTH = 2;
constexpr std::uint32_t AUDIO3D_BUFFER_NO_ADVANCE = 0;
constexpr std::uint32_t AUDIO3D_BUFFER_ADVANCE_NO_PUSH = 1;
constexpr std::uint32_t AUDIO3D_BUFFER_ADVANCE_AND_PUSH = 2;
constexpr int AUDIO3D_AUDIO_OUT_PORT_TYPE = 126;
constexpr int AUDIO_OUT_PORT_TYPE_COUNT = 5;
constexpr int AUDIO_OUT_PORT_TYPE_PERSONAL = 3;
constexpr std::uint32_t AUDIO_OUT_PARAM_BIT_17 = 1u << 17;
constexpr std::uint32_t AUDIO_OUT_FORMAT_S16_8CH_STD = 6;
constexpr std::uint32_t AUDIO_OUT_FORMAT_FLOAT_8CH_STD = 7;
constexpr int AUDIO_OUT_PORT_TYPE_AUX = 127;
constexpr std::uint32_t AUDIO_OUT_FORMAT_LAST = 7;
constexpr std::uint32_t AUDIO3D_AUDIO_OUT_FORMAT_STEREO = 1;
constexpr std::uint32_t AUDIO3D_BLOCKING_ASYNC = 0;
constexpr std::uint32_t AUDIO3D_BLOCKING_SYNC = 1;
constexpr std::uint32_t AUDIO3D_OBJECT_ATTRIBUTE_PCM = 1;
constexpr std::uint32_t AUDIO3D_OBJECT_ATTRIBUTE_RESET_STATE = 7;
constexpr std::uint32_t AUDIO3D_OBJECT_ATTRIBUTE_OUTPUT_ROUTE = 11;
constexpr std::uint32_t AUDIO3D_OBJECT_ID_MASK = 0x7FFFFFFF;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_LATE_REVERB_LEVEL = 0x10001;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_RADIUS = 0x10002;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_HEIGHT_AWARE = 0x10003;
constexpr std::uint32_t AUDIO3D_FORMAT_S16 = 0;
constexpr std::uint32_t AUDIO3D_FORMAT_FLOAT = 1;
constexpr std::uint32_t AUDIO3D_OUTPUT_ROUTE_BOTH = 0;
constexpr std::uint32_t AUDIO3D_OUTPUT_ROUTE_HMU_ONLY = 1;
constexpr std::uint32_t AUDIO3D_OUTPUT_ROUTE_TV_ONLY = 2;
constexpr std::uint32_t AUDIO3D_STATUS_DOWNMIX = 1;
constexpr std::uint32_t AUDIO3D_PORT_INVALID = 0xFFFFFFFF;
constexpr std::uint32_t AUDIO3D_REPORT_TYPES = 4;
constexpr std::uint32_t AUDIO3D_REPORT_SLOTS = 8;
constexpr std::uint32_t AUDIO3D_REPORT_TYPE_CALL = 0;
constexpr std::uint64_t AUDIO3D_REPORT_HANDLE_INVALID = 0xFFFFFFFF;
constexpr int AUDIO3D_REPORT_ERROR_INVALID_HANDLE = -1;
constexpr int AUDIO3D_REPORT_ERROR_INVALID_HANDLER = -2;
constexpr int AUDIO3D_REPORT_ERROR_FULL = -3;
constexpr int AUDIO3D_REPORT_ERROR_INVALID_TYPE = -4;

static_assert(sizeof(Audio3dReport) == 0x40);

static_assert(offsetof(Audio3dPcm, num_samples) == 0x10);
struct Audio3dReportSlot {
    Audio3dReportHandler handler = nullptr;
    void* user_data = nullptr;
};

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
    std::uint32_t num_beds = 0;
    std::uint32_t next_object_id = 0;
    std::set<std::uint32_t> objects;
    std::deque<Audio3dBed> beds;
    std::deque<std::vector<std::int16_t>> mixed;
    int audio_out_handle = 0;
    std::map<int, Audio3dAudioOut> audio_out_ports;
};

std::mutex g_mutex;
bool g_initialized = false;
std::atomic<bool> g_ps4_process{false};
Audio3dPort g_port;
std::set<int> g_closed_port_audio_outs;
std::mutex g_report_mutex;
Audio3dReportSlot g_report_slots[AUDIO3D_REPORT_TYPES][AUDIO3D_REPORT_SLOTS];
std::atomic<std::uint32_t> g_call_report_handlers{0};

[[noreturn]] void Unsupported(const char* function, const std::string& what) {
    throw std::runtime_error(std::string(function) + ": " + what);
}

bool PortIsOpen(std::uint32_t port_id) {
    return port_id == AUDIO3D_PORT_ID && g_port.open;
}

bool Ps4Process() {
    return g_ps4_process.load();
}

bool CallReportsRegistered() {
    return g_call_report_handlers.load() != 0;
}

std::uint32_t ReportedPortId(const std::uint32_t* id) {
    return id != nullptr ? *id : AUDIO3D_PORT_INVALID;
}

void StorePort(std::uint32_t queue_depth, std::uint32_t granularity, std::uint32_t buffer_mode, std::uint32_t max_objects, std::uint32_t num_beds, std::uint32_t* id) {
    g_port = Audio3dPort{};
    g_port.open = true;
    g_port.queue_depth = queue_depth;
    g_port.granularity = granularity;
    g_port.buffer_mode = buffer_mode;
    g_port.max_objects = max_objects;
    g_port.num_beds = num_beds;
    *id = AUDIO3D_PORT_ID;
}

int OpenPs5Port(int user_id, const Audio3dOpenParameters* parameters, std::uint32_t* id) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return AUDIO3D_ERROR_NOT_READY;
    if (user_id != AUDIO3D_USER_ID_SYSTEM || parameters == nullptr || id == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->size_this != 0x20 && parameters->size_this != 0x28) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->rate != AUDIO3D_RATE_48000) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->granularity < 0x100 || (parameters->granularity & 0xFF) != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->max_objects == 0 || parameters->queue_depth == 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->buffer_mode != AUDIO3D_BUFFER_ADVANCE_AND_PUSH) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (g_port.open) return AUDIO3D_ERROR_OUT_OF_RESOURCES;
    const std::uint32_t num_beds = parameters->size_this == 0x28 ? parameters->num_beds : 2;
    StorePort(parameters->queue_depth, parameters->granularity, parameters->buffer_mode, parameters->max_objects, num_beds, id);
    return 0;
}

int OpenPs4Port(const char* function, int user_id, const Audio3dOpenParameters* parameters, std::uint32_t* id) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return AUDIO3D_ERROR_NOT_READY;
    if (user_id != AUDIO3D_USER_ID_SYSTEM || parameters == nullptr || id == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    std::uint32_t max_objects = AUDIO3D_DEFAULT_MAX_OBJECTS;
    std::uint32_t queue_depth = AUDIO3D_DEFAULT_QUEUE_DEPTH;
    std::uint32_t buffer_mode = AUDIO3D_BUFFER_NO_ADVANCE;
    std::uint32_t num_beds = 2;
    switch (parameters->size_this) {
    case 0x10:
        break;
    case 0x18:
        max_objects = parameters->max_objects;
        queue_depth = parameters->queue_depth;
        buffer_mode = AUDIO3D_BUFFER_ADVANCE_NO_PUSH;
        break;
    case 0x28:
        num_beds = parameters->num_beds;
        [[fallthrough]];
    case 0x20:
        max_objects = parameters->max_objects;
        queue_depth = parameters->queue_depth;
        buffer_mode = parameters->buffer_mode;
        break;
    default:
        return AUDIO3D_ERROR_INVALID_PARAMETER;
    }
    if (parameters->rate != AUDIO3D_RATE_48000) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (parameters->granularity < 0x100 || (parameters->granularity & 0xFF) != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (max_objects == 0 || queue_depth == 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (buffer_mode > AUDIO3D_BUFFER_ADVANCE_AND_PUSH) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (num_beds != 2 && num_beds != 3) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (g_port.open) return AUDIO3D_ERROR_OUT_OF_RESOURCES;
    if (buffer_mode != AUDIO3D_BUFFER_ADVANCE_AND_PUSH) Unsupported(function, "buffer mode " + std::to_string(buffer_mode));
    if (num_beds != 2) Unsupported(function, "3 beds");
    StorePort(queue_depth, parameters->granularity, buffer_mode, max_objects, num_beds, id);
    return 0;
}

std::uint32_t QueueLevel() {
    return static_cast<std::uint32_t>(g_port.mixed.size());
}

int CheckFloatAttribute(const void* attribute, std::size_t attribute_size, float minimum, float maximum) {
    if (attribute == nullptr || attribute_size != sizeof(float)) return AUDIO3D_ERROR_INVALID_PARAMETER;
    float value = 0.0f;
    std::memcpy(&value, attribute, sizeof(value));
    if (!std::isfinite(value) || value < minimum || value > maximum) return AUDIO3D_ERROR_INVALID_PARAMETER;
    return 0;
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

void APS5_VABI DispatchReport_nid_no_patch(std::uint32_t type, std::uint32_t port_id, std::uint32_t object_id, int result, const char* function, const char* argument_types, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, argument_types);
#else
    std::va_list arguments;
    va_start(arguments, argument_types);
#endif
    static_assert(sizeof(arguments) == sizeof(Audio3dReportArguments));
    Audio3dReport report{sizeof(Audio3dReport), port_id, object_id, type, result, function, argument_types, {}};
    std::memcpy(&report.arguments, arguments, sizeof(report.arguments));
    Audio3dReportSlot slots[AUDIO3D_REPORT_SLOTS];
    {
        std::lock_guard lock(g_report_mutex);
        std::copy(std::begin(g_report_slots[type]), std::end(g_report_slots[type]), slots);
    }
    for (const Audio3dReportSlot& slot : slots) {
        if (slot.handler != nullptr) slot.handler(&report, slot.user_data);
    }
#ifdef _WIN32
    __builtin_sysv_va_end(arguments);
#else
    va_end(arguments);
#endif
}

template <typename T>
std::uint64_t ReportArgument(T value) {
    if constexpr (std::is_pointer_v<T>) {
        return reinterpret_cast<std::uintptr_t>(value);
    } else {
        return static_cast<std::make_unsigned_t<T>>(value);
    }
}

template <typename... Arguments>
void Report(std::uint32_t port_id, std::uint32_t object_id, int result, const char* function, const char* argument_types, Arguments... arguments) {
    if (g_call_report_handlers.load() == 0) return;
    DispatchReport_nid_no_patch(AUDIO3D_REPORT_TYPE_CALL, port_id, object_id, result, function, argument_types, ReportArgument(arguments)...);
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
    const int result = [&] {
        g_ps4_process = sceKernelIsPs4Process() != 0;
        std::lock_guard lock(g_mutex);
        if (g_ps4_process && g_initialized) return AUDIO3D_ERROR_NOT_READY;
        if (reserved != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
        if (g_initialized) Unsupported("sceAudio3dInitialize", "repeated initialization");
        g_initialized = true;
        return 0;
    }();
    Report(AUDIO3D_PORT_INVALID, AUDIO3D_OBJECT_INVALID, result, __func__, "");
    return result;
}

int APS5_VABI sceAudio3dTerminate() {
    const int result = [&] {
        std::lock_guard lock(g_mutex);
        if (!g_initialized || g_port.open) return AUDIO3D_ERROR_NOT_READY;
        g_initialized = false;
        return 0;
    }();
    Report(AUDIO3D_PORT_INVALID, AUDIO3D_OBJECT_INVALID, result, __func__, "");
    return result;
}

int APS5_VABI sceAudio3dPortAdvance(uint32_t port_id) {
    const int result = [&] {
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
    }();
    Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "H", port_id);
    return result;
}

int APS5_VABI sceAudio3dPortClose(uint32_t port_id) {
    const int result = [&] {
        int handle = 0;
        {
            std::lock_guard lock(g_mutex);
            if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
            handle = g_port.audio_out_handle;
            for (const auto& entry : g_port.audio_out_ports) g_closed_port_audio_outs.insert(entry.first);
            g_port = Audio3dPort{};
        }
        if (handle != 0) sceAudioOutClose(handle);
        return 0;
    }();
    Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "H", port_id);
    return result;
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
    const int result = Ps4Process() ? OpenPs4Port(__func__, user_id, parameters, id) : OpenPs5Port(user_id, parameters, id);
    if (CallReportsRegistered()) Report(ReportedPortId(id), AUDIO3D_OBJECT_INVALID, result, __func__, "I&HD", user_id, id, parameters);
    return result;
}

int APS5_VABI sceAudio3dPortPush(uint32_t port_id, uint32_t blocking) {
    const int result = [&] {
        int handle = 0;
        std::uint32_t granularity = 0;
        {
            std::lock_guard lock(g_mutex);
            if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
            if (blocking != AUDIO3D_BLOCKING_ASYNC && blocking != AUDIO3D_BLOCKING_SYNC) {
                Unsupported("sceAudio3dPortPush", "blocking mode " + std::to_string(blocking));
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
    }();
    Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "HI", port_id, blocking);
    return result;
}

int APS5_VABI sceAudio3dPortSetAttribute(uint32_t port_id, uint32_t attribute_id, const void* attribute, size_t attribute_size) {
    const int result = [&] {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        switch (attribute_id) {
        case AUDIO3D_ATTRIBUTE_LATE_REVERB_LEVEL:
            return CheckFloatAttribute(attribute, attribute_size, 0.0f, 1.0f);
        case AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_RADIUS:
            return CheckFloatAttribute(attribute, attribute_size, 0.1f, 2.0f);
        case AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_HEIGHT_AWARE:
            return attribute == nullptr ? AUDIO3D_ERROR_INVALID_PARAMETER : 0;
        default:
            return AUDIO3D_ERROR_INVALID_PARAMETER;
        }
    }();
    Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "HUM", port_id, attribute_id, attribute, attribute_size);
    return result;
}

int APS5_VABI sceAudio3dObjectReserve(uint32_t port_id, uint32_t* object_id) {
    const int result = [&] {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        if (object_id == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
        *object_id = AUDIO3D_OBJECT_INVALID;
        if (g_port.objects.size() >= g_port.max_objects) return AUDIO3D_ERROR_OUT_OF_RESOURCES;
        do {
            ++g_port.next_object_id;
        } while (g_port.next_object_id == 0 || g_port.next_object_id == AUDIO3D_OBJECT_INVALID || g_port.objects.contains(g_port.next_object_id));
        g_port.objects.insert(g_port.next_object_id);
        *object_id = g_port.next_object_id;
        return 0;
    }();
    Report(port_id, result == 0 ? *object_id : AUDIO3D_OBJECT_INVALID, result, __func__, "H&O", port_id, object_id);
    return result;
}

int APS5_VABI sceAudio3dObjectUnreserve(uint32_t port_id, uint32_t object_id) {
    const int result = [&] {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        if (g_port.objects.erase(object_id) == 0) return AUDIO3D_ERROR_INVALID_OBJECT;
        return 0;
    }();
    Report(port_id, object_id, result, __func__, "HO", port_id, object_id);
    return result;
}

int APS5_VABI sceAudio3dObjectSetAttributes(uint32_t port_id, uint32_t object_id, uint64_t num_attributes, const Audio3dAttribute* attribute_array) {
    const int result = [&] {
        if (num_attributes == 0 || attribute_array == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        for (std::uint64_t i = 0; i < num_attributes; ++i) {
            const Audio3dAttribute& attribute = attribute_array[i];
            if (attribute.attribute_id < AUDIO3D_OBJECT_ATTRIBUTE_PCM || attribute.attribute_id > AUDIO3D_OBJECT_ATTRIBUTE_OUTPUT_ROUTE) return AUDIO3D_ERROR_INVALID_PARAMETER;
            if (attribute.attribute_id != AUDIO3D_OBJECT_ATTRIBUTE_PCM) continue;
            const auto* pcm = static_cast<const Audio3dPcm*>(attribute.value);
            if (pcm == nullptr || pcm->num_samples == 0 || pcm->format > AUDIO3D_FORMAT_FLOAT) return AUDIO3D_ERROR_INVALID_PARAMETER;
        }
        if (!g_port.objects.contains(object_id & AUDIO3D_OBJECT_ID_MASK)) return AUDIO3D_ERROR_INVALID_OBJECT;
        for (std::uint64_t i = 0; i < num_attributes; ++i) {
            if (attribute_array[i].attribute_id == AUDIO3D_OBJECT_ATTRIBUTE_RESET_STATE) continue;
            if (attribute_array[i].value == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
        }
        return 0;
    }();
    Report(port_id, object_id, result, __func__, "HOA", port_id, object_id, num_attributes, attribute_array);
    return result;
}

int APS5_VABI sceAudio3dPortFlush(uint32_t port_id) {
    const int result = [&] {
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
    }();
    Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "H", port_id);
    return result;
}

int APS5_VABI sceAudio3dBedWrite2(uint32_t port_id, uint32_t num_channels, uint32_t format, const void* buffer, uint32_t num_samples, uint32_t output_route, bool) {
    std::uint32_t reported_format = format;
    const int result = [&] {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        if (num_channels != 2 && num_channels != 6 && num_channels != 8) return AUDIO3D_ERROR_INVALID_PARAMETER;
        if (format > AUDIO3D_FORMAT_FLOAT) return AUDIO3D_ERROR_INVALID_PARAMETER;
        const bool route_supported = Ps4Process()
            ? output_route <= AUDIO3D_OUTPUT_ROUTE_TV_ONLY && !(output_route == AUDIO3D_OUTPUT_ROUTE_HMU_ONLY && g_port.num_beds == 2)
            : output_route == AUDIO3D_OUTPUT_ROUTE_BOTH;
        if (!route_supported) return AUDIO3D_ERROR_INVALID_PARAMETER;
        if (buffer == nullptr || num_samples == 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
        const std::size_t sample_size = format == AUDIO3D_FORMAT_S16 ? 2u : 4u;
        if (reinterpret_cast<std::uintptr_t>(buffer) % sample_size != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
        reported_format = format ^ 1;
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
    }();
    Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "HUUPU", port_id, num_channels, reported_format, buffer, num_samples);
    return result;
}

int APS5_VABI sceAudio3dBedWrite(uint32_t port_id, uint32_t num_channels, uint32_t format, const void* buffer, uint32_t num_samples) {
    const int result = sceAudio3dBedWrite2(port_id, num_channels, format, buffer, num_samples, AUDIO3D_OUTPUT_ROUTE_BOTH, false);
    Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "HUUPU", port_id, num_channels, format, buffer, num_samples);
    return result;
}

int APS5_VABI sceAudio3dAudioOutOpen(uint32_t port_id, int user_id, std::int32_t type, std::int32_t index, uint32_t len, uint32_t freq, uint32_t param) {
    const int result = [&] {
        {
            std::lock_guard lock(g_mutex);
            if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
            if (type < 0 || len != g_port.granularity) return AUDIO3D_ERROR_INVALID_PARAMETER;
        }
        const int initialized = sceAudioOutInit();
        if (initialized != 0 && initialized != AUDIO_OUT_ERROR_ALREADY_INIT) return initialized;
        if (type >= AUDIO_OUT_PORT_TYPE_COUNT && type != AUDIO_OUT_PORT_TYPE_AUX) return AUDIO_OUT_ERROR_INVALID_PORT_TYPE;
        const bool ps4 = Ps4Process();
        if (ps4 && type == AUDIO_OUT_PORT_TYPE_PERSONAL && (param & AUDIO_OUT_PARAM_BIT_17) != 0) return AUDIO_OUT_ERROR_INVALID_FORMAT;
        const std::uint32_t format = param & 0xFF;
        if (format > AUDIO_OUT_FORMAT_LAST) return AUDIO_OUT_ERROR_INVALID_FORMAT;
        if (ps4 && (format == AUDIO_OUT_FORMAT_S16_8CH_STD || format == AUDIO_OUT_FORMAT_FLOAT_8CH_STD)) {
            Unsupported("sceAudio3dAudioOutOpen", "PS4 format " + std::to_string(format) + " depends on the PS4 SDK version");
        }
        const int handle = sceAudioOutOpen(user_id, type, index, len, freq, param);
        if (handle < 0) return handle;
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
    }();
    Report(port_id, AUDIO3D_OBJECT_INVALID, result < 0 ? result : 0, __func__, "HUIIUUU", port_id, user_id, type, index, len, freq, param);
    return result;
}

int APS5_VABI sceAudio3dAudioOutClose(std::int32_t handle) {
    {
        std::lock_guard lock(g_mutex);
        const bool opened = g_port.audio_out_ports.erase(handle) != 0;
        if (g_closed_port_audio_outs.erase(handle) == 0 && !opened) return AUDIO3D_ERROR_INVALID_PORT;
    }
    const int result = sceAudioOutClose(handle);
    Report(AUDIO3D_PORT_INVALID, AUDIO3D_OBJECT_INVALID, result, __func__, "I", handle);
    return result;
}

int APS5_VABI sceAudio3dAudioOutOutput(std::int32_t handle, const void* ptr) {
    const int result = [&] {
        if (ptr == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
        std::lock_guard lock(g_mutex);
        const auto port = g_port.audio_out_ports.find(handle);
        if (port == g_port.audio_out_ports.end()) return AUDIO3D_ERROR_INVALID_PORT;
        if (port->second.pending.size() >= g_port.queue_depth) return AUDIO3D_ERROR_NOT_READY;
        const auto* first = static_cast<const std::uint8_t*>(ptr);
        port->second.pending.emplace_back(first, first + port->second.buffer_bytes);
        return 0;
    }();
    Report(AUDIO3D_PORT_INVALID, AUDIO3D_OBJECT_INVALID, result, __func__, "IP", handle, ptr);
    return result;
}

int APS5_VABI sceAudio3dAudioOutOutputs(AudioOutOutputParam* param, uint32_t num) {
    std::uint32_t i = 0;
    const int result = [&] {
        if (param == nullptr || num == 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
        for (; i < num; ++i) {
            const int output = sceAudio3dAudioOutOutput(param[i].handle, param[i].ptr);
            if (output < 0) return output;
        }
        return 0;
    }();
    Report(AUDIO3D_PORT_INVALID, AUDIO3D_OBJECT_INVALID, result, __func__, "PU", param != nullptr ? param + i : nullptr, num);
    return result;
}

const char* APS5_VABI sceAudio3dStrError(std::int32_t error) {
    static constexpr const char* descriptions[]{
        "port does not exist", "object does not exist", "invalid parameter",
        "out of memory", "out of resources", "not ready"
    };
    const auto index = static_cast<std::uint32_t>(error) - 0x80EA0002u;
    return index < std::size(descriptions) ? descriptions[index] : "(unknown)";
}

int APS5_VABI sceAudio3dPortOpenEx(int user_id, const Audio3dOpenParameters* parameters, uint32_t* id) {
    return sceAudio3dPortOpen(user_id, parameters, id);
}

int APS5_VABI sceAudio3dPortCreate(uint32_t granularity, uint32_t rate, int64_t reserved, uint32_t* id) {
    const int result = [&] {
        if (reserved != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
        Audio3dOpenParameters parameters{};
        parameters.size_this = 0x10;
        parameters.granularity = granularity;
        parameters.rate = rate;
        return sceAudio3dPortOpen(AUDIO3D_USER_ID_SYSTEM, &parameters, id);
    }();
    if (CallReportsRegistered()) {
        const std::uint32_t port_id = ReportedPortId(id);
        Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "H", port_id);
    }
    return result;
}

int APS5_VABI sceAudio3dPortDestroy(uint32_t port_id) {
    const int result = [&] {
        {
            std::lock_guard lock(g_mutex);
            if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        }
        return sceAudio3dPortClose(port_id);
    }();
    Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "H", port_id);
    return result;
}

int APS5_VABI sceAudio3dPortGetStatus(uint32_t port_id, uint32_t* status, char* description, size_t description_size) {
    const int result = [&] {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        if (status != nullptr) *status = AUDIO3D_STATUS_DOWNMIX;
        if (description != nullptr && description_size != 0) {
            constexpr char text[] = "Downmix ()";
            const std::size_t length = std::min(description_size - 1, sizeof(text) - 1);
            std::memcpy(description, text, length);
            description[length] = '\0';
        }
        return 0;
    }();
    Report(port_id, AUDIO3D_OBJECT_INVALID, result, __func__, "H", port_id);
    return result;
}

int APS5_VABI sceAudio3dReportRegisterHandler(uint32_t type, Audio3dReportHandler handler, void* user_data, uint64_t* handle) {
    if (type >= AUDIO3D_REPORT_TYPES) return AUDIO3D_REPORT_ERROR_INVALID_TYPE;
    if (handler == nullptr) return AUDIO3D_REPORT_ERROR_INVALID_HANDLER;
    int result = AUDIO3D_REPORT_ERROR_FULL;
    std::uint64_t registered = AUDIO3D_REPORT_HANDLE_INVALID;
    {
        std::lock_guard lock(g_report_mutex);
        Audio3dReportSlot* const slot = std::find_if(std::begin(g_report_slots[type]), std::end(g_report_slots[type]),
            [](const Audio3dReportSlot& candidate) { return candidate.handler == nullptr; });
        if (slot != std::end(g_report_slots[type])) {
            *slot = Audio3dReportSlot{handler, user_data};
            if (type == AUDIO3D_REPORT_TYPE_CALL) ++g_call_report_handlers;
            registered = reinterpret_cast<std::uintptr_t>(slot);
            result = 0;
        }
    }
    if (handle != nullptr) *handle = registered;
    return result;
}

int APS5_VABI sceAudio3dReportUnregisterHandler(uint64_t handle) {
    if (handle == AUDIO3D_REPORT_HANDLE_INVALID) return AUDIO3D_REPORT_ERROR_INVALID_HANDLE;
    std::lock_guard lock(g_report_mutex);
    for (std::uint32_t type = 0; type < AUDIO3D_REPORT_TYPES; ++type) {
        for (Audio3dReportSlot& slot : g_report_slots[type]) {
            if (reinterpret_cast<std::uintptr_t>(&slot) != handle || slot.handler == nullptr) continue;
            slot = Audio3dReportSlot{};
            if (type == AUDIO3D_REPORT_TYPE_CALL) --g_call_report_handlers;
            return 0;
        }
    }
    return AUDIO3D_REPORT_ERROR_INVALID_HANDLE;
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

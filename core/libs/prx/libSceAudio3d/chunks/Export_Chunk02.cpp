extern "C" {
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudio3d/include/Audio3d.hpp"
#include <algorithm>
#include <cerrno>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cerrno>
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
    std::uint32_t portId = AUDIO3D_PORT_INVALID;
    if (id != nullptr) {
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(&portId, id, sizeof(portId));
        errno = savedErrno;
        if (copied) return portId;
    }
    return AUDIO3D_PORT_INVALID;
}

bool StorePort(std::uint32_t queue_depth, std::uint32_t granularity, std::uint32_t buffer_mode, std::uint32_t max_objects, std::uint32_t num_beds, std::uint32_t* id) {
    constexpr std::uint32_t portId = AUDIO3D_PORT_ID;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(id, &portId, sizeof(portId));
    errno = savedErrno;
    if (!copied) {
        return false;
    }
    g_port = Audio3dPort{};
    g_port.open = true;
    g_port.queue_depth = queue_depth;
    g_port.granularity = granularity;
    g_port.buffer_mode = buffer_mode;
    g_port.max_objects = max_objects;
    g_port.num_beds = num_beds;
    return true;
}

int OpenPs5Port(int user_id, const Audio3dOpenParameters* parameters, std::uint32_t* id) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return AUDIO3D_ERROR_NOT_READY;
    if (user_id != AUDIO3D_USER_ID_SYSTEM || parameters == nullptr || id == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    std::uint64_t sizeThis = 0;
    const int savedErrno = errno;
    const bool copiedSize = GuestCopyIn_nid_no_patch(&sizeThis, parameters, sizeof(sizeThis));
    errno = savedErrno;
    if (!copiedSize) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (sizeThis != 0x20 && sizeThis != 0x28) return AUDIO3D_ERROR_INVALID_PARAMETER;
    Audio3dOpenParameters localParameters{};
    const bool copiedParameters = GuestCopyIn_nid_no_patch(&localParameters, parameters, static_cast<std::size_t>(sizeThis));
    errno = savedErrno;
    if (!copiedParameters) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (localParameters.rate != AUDIO3D_RATE_48000) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (localParameters.granularity < 0x100 || (localParameters.granularity & 0xFF) != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (localParameters.max_objects == 0 || localParameters.queue_depth == 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (localParameters.buffer_mode != AUDIO3D_BUFFER_ADVANCE_AND_PUSH) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (g_port.open) return AUDIO3D_ERROR_OUT_OF_RESOURCES;
    const std::uint32_t num_beds = sizeThis == 0x28 ? localParameters.num_beds : 2;
    if (!StorePort(localParameters.queue_depth, localParameters.granularity, localParameters.buffer_mode, localParameters.max_objects, num_beds, id)) {
        return AUDIO3D_ERROR_INVALID_PARAMETER;
    }
    return 0;
}

int OpenPs4Port(const char* function, int user_id, const Audio3dOpenParameters* parameters, std::uint32_t* id) {
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return AUDIO3D_ERROR_NOT_READY;
    if (user_id != AUDIO3D_USER_ID_SYSTEM || parameters == nullptr || id == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    std::uint64_t sizeThis = 0;
    const int savedErrno = errno;
    const bool copiedSize = GuestCopyIn_nid_no_patch(&sizeThis, parameters, sizeof(sizeThis));
    errno = savedErrno;
    if (!copiedSize) return AUDIO3D_ERROR_INVALID_PARAMETER;
    Audio3dOpenParameters localParameters{};
    if (sizeThis != 0x10 && sizeThis != 0x18 && sizeThis != 0x20 && sizeThis != 0x28) return AUDIO3D_ERROR_INVALID_PARAMETER;
    const bool copiedParameters = GuestCopyIn_nid_no_patch(&localParameters, parameters, static_cast<std::size_t>(sizeThis));
    errno = savedErrno;
    if (!copiedParameters) return AUDIO3D_ERROR_INVALID_PARAMETER;
    std::uint32_t max_objects = AUDIO3D_DEFAULT_MAX_OBJECTS;
    std::uint32_t queue_depth = AUDIO3D_DEFAULT_QUEUE_DEPTH;
    std::uint32_t buffer_mode = AUDIO3D_BUFFER_NO_ADVANCE;
    std::uint32_t num_beds = 2;
    switch (sizeThis) {
    case 0x10:
        break;
    case 0x18:
        max_objects = localParameters.max_objects;
        queue_depth = localParameters.queue_depth;
        buffer_mode = AUDIO3D_BUFFER_ADVANCE_NO_PUSH;
        break;
    case 0x28:
        num_beds = localParameters.num_beds;
        [[fallthrough]];
    case 0x20:
        max_objects = localParameters.max_objects;
        queue_depth = localParameters.queue_depth;
        buffer_mode = localParameters.buffer_mode;
        break;
    default:
        return AUDIO3D_ERROR_INVALID_PARAMETER;
    }
    if (localParameters.rate != AUDIO3D_RATE_48000) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (localParameters.granularity < 0x100 || (localParameters.granularity & 0xFF) != 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (max_objects == 0 || queue_depth == 0) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (buffer_mode > AUDIO3D_BUFFER_ADVANCE_AND_PUSH) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (num_beds != 2 && num_beds != 3) return AUDIO3D_ERROR_INVALID_PARAMETER;
    if (g_port.open) return AUDIO3D_ERROR_OUT_OF_RESOURCES;
    if (buffer_mode != AUDIO3D_BUFFER_ADVANCE_AND_PUSH) Unsupported(function, "buffer mode " + std::to_string(buffer_mode));
    if (num_beds != 2) Unsupported(function, "3 beds");
    if (!StorePort(queue_depth, localParameters.granularity, buffer_mode, max_objects, num_beds, id)) {
        return AUDIO3D_ERROR_INVALID_PARAMETER;
    }
    return 0;
}

std::uint32_t QueueLevel() {
    return static_cast<std::uint32_t>(g_port.mixed.size());
}

int CheckFloatAttribute(const void* attribute, std::size_t attribute_size, float minimum, float maximum) {
    if (attribute == nullptr || attribute_size != sizeof(float)) return AUDIO3D_ERROR_INVALID_PARAMETER;
    float value = 0.0f;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, attribute, sizeof(value));
    errno = savedErrno;
    if (!copied) return AUDIO3D_ERROR_INVALID_PARAMETER;
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

bool CopyOutU32(std::uint32_t* dst, std::uint32_t value) {
    if (dst == nullptr) return true;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(dst, &value, sizeof(value));
    errno = savedErrno;
    return copied;
}

}


int APS5_VABI sceAudio3dAudioOutOutput(std::int32_t handle, const void* ptr) {
    const int result = [&] {
        if (ptr == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
        std::lock_guard lock(g_mutex);
        const auto port = g_port.audio_out_ports.find(handle);
        if (port == g_port.audio_out_ports.end()) return AUDIO3D_ERROR_INVALID_PORT;
        if (port->second.pending.size() >= g_port.queue_depth) return AUDIO3D_ERROR_NOT_READY;
        std::vector<std::uint8_t> block(port->second.buffer_bytes);
        const int savedErrno = errno;
        const bool copied = GuestCopyIn_nid_no_patch(block.data(), ptr, block.size());
        errno = savedErrno;
        if (!copied) return AUDIO3D_ERROR_INVALID_PARAMETER;
        port->second.pending.push_back(std::move(block));
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
            AudioOutOutputParam entry{};
            const int savedErrno = errno;
            const bool copied = GuestCopyIn_nid_no_patch(&entry, param + i, sizeof(entry));
            errno = savedErrno;
            if (!copied) return AUDIO3D_ERROR_INVALID_PARAMETER;
            const int output = sceAudio3dAudioOutOutput(entry.handle, entry.ptr);
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
        const int savedErrno = errno;
        if (status != nullptr) {
            const uint32_t val = AUDIO3D_STATUS_DOWNMIX;
            const bool copied = GuestCopyOut_nid_no_patch(status, &val, sizeof(val));
            errno = savedErrno;
            if (!copied) return AUDIO3D_ERROR_INVALID_PARAMETER;
        }
        if (description != nullptr && description_size != 0) {
            constexpr char text[] = "Downmix ()";
            const std::size_t length = std::min(description_size - 1, sizeof(text) - 1);
            char localDesc[sizeof(text)]{};
            std::memcpy(localDesc, text, length);
            localDesc[length] = '\0';
            const bool copied = GuestCopyOut_nid_no_patch(description, localDesc, length + 1);
            errno = savedErrno;
            if (!copied) return AUDIO3D_ERROR_INVALID_PARAMETER;
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
        if (handle != nullptr) {
            const int savedErrno = errno;
            const bool copied = GuestCopyOut_nid_no_patch(handle, &registered, sizeof(registered));
            errno = savedErrno;
            if (!copied) {
                if (result == 0) {
                    *slot = Audio3dReportSlot{};
                    if (type == AUDIO3D_REPORT_TYPE_CALL) --g_call_report_handlers;
                }
                throw std::runtime_error("sceAudio3dReportRegisterHandler: unwritable guest handle");
            }
        }
    }
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
    const int saved = errno;
    const bool copiedIn = GuestCopyIn_nid_no_patch(extended, parameters, sizeof(*parameters));
    errno = saved;
    if (!copiedIn) return AUDIO3D_ERROR_INVALID_PARAMETER;
    AudioOut2SpeakerArrayHandle localHandle = nullptr;
    const int result = sceAudioOut2SpeakerArrayCreate(&localHandle, extended, nullptr);
    if (result != 0) return result;
    const int savedOut = errno;
    const bool copiedOut = GuestCopyOut_nid_no_patch(handle, &localHandle, sizeof(localHandle));
    errno = savedOut;
    if (!copiedOut) {
        sceAudioOut2SpeakerArrayDestroy(localHandle);
        return AUDIO3D_ERROR_INVALID_PARAMETER;
    }
    return 0;
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

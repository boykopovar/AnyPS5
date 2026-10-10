#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/PreciseWait.hpp"

namespace {

constexpr int AUDIO3D_ERROR_INVALID_PORT = static_cast<int>(0x80EA0002);
constexpr int AUDIO3D_ERROR_INVALID_OBJECT = static_cast<int>(0x80EA0003);
constexpr int AUDIO3D_ERROR_INVALID_PARAMETER = static_cast<int>(0x80EA0004);
constexpr int AUDIO3D_ERROR_OUT_OF_RESOURCES = static_cast<int>(0x80EA0006);
constexpr int AUDIO3D_ERROR_NOT_READY = static_cast<int>(0x80EA0007);

constexpr int AUDIO3D_USER_ID_SYSTEM = 0xFF;
constexpr std::uint32_t AUDIO3D_PORT_ID = 0;
constexpr std::uint32_t AUDIO3D_OBJECT_INVALID = 0xFFFFFFFF;
constexpr std::uint32_t AUDIO3D_RATE_48000 = 0;
constexpr std::uint32_t AUDIO3D_SAMPLE_RATE = 48000;
constexpr std::uint32_t AUDIO3D_BUFFER_ADVANCE_AND_PUSH = 2;
constexpr std::uint32_t AUDIO3D_BLOCKING_ASYNC = 0;
constexpr std::uint32_t AUDIO3D_BLOCKING_SYNC = 1;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_RESET_STATE = 0x20000;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_LATE_REVERB_LEVEL = 0x10001;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_RADIUS = 0x10002;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_HEIGHT_AWARE = 0x10003;

using Clock = std::chrono::steady_clock;

struct Audio3dPort {
    bool open = false;
    std::uint32_t queue_depth = 0;
    Clock::duration frame_duration{};
    std::uint32_t advanced = 0;
    std::deque<Clock::time_point> playing;
    std::uint32_t max_objects = 0;
    std::uint32_t next_object_id = 0;
    std::set<std::uint32_t> objects;
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

std::uint32_t QueueLevel(Clock::time_point now) {
    while (!g_port.playing.empty() && g_port.playing.front() <= now) g_port.playing.pop_front();
    return g_port.advanced + static_cast<std::uint32_t>(g_port.playing.size());
}

}

extern "C" {

void APS5_VABI sceAudio3dGetDefaultOpenParameters(Audio3dOpenParameters* p) {
    if (p == nullptr) APS5_INVALID_ARG_EX;
    constexpr Audio3dOpenParameters defaults{0x20, 256, 0, 512, 2, 2, 0, 0};
    static_assert(offsetof(Audio3dOpenParameters, num_beds) == 0x20);
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
    if (QueueLevel(Clock::now()) >= g_port.queue_depth) Unsupported(__func__, "queue is full");
    ++g_port.advanced;
    return 0;
}

int APS5_VABI sceAudio3dPortClose(uint32_t port_id) {
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    g_port = Audio3dPort{};
    return 0;
}

int APS5_VABI sceAudio3dPortGetQueueLevel(uint32_t port_id, uint32_t* queue_level, uint32_t* queue_available) {
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    if (queue_level == nullptr && queue_available == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    const std::uint32_t level = QueueLevel(Clock::now());
    if (queue_level != nullptr) *queue_level = level;
    if (queue_available != nullptr) *queue_available = g_port.queue_depth - level;
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
    g_port.max_objects = parameters->max_objects;
    g_port.frame_duration = std::chrono::duration_cast<Clock::duration>(
        std::chrono::nanoseconds(std::uint64_t{parameters->granularity} * 1000000000u / AUDIO3D_SAMPLE_RATE));
    *id = AUDIO3D_PORT_ID;
    return 0;
}

int APS5_VABI sceAudio3dPortPush(uint32_t port_id, uint32_t blocking) {
    Clock::time_point wait_until;
    {
        std::lock_guard lock(g_mutex);
        if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
        if (blocking != AUDIO3D_BLOCKING_ASYNC && blocking != AUDIO3D_BLOCKING_SYNC) {
            Unsupported(__func__, "blocking mode " + std::to_string(blocking));
        }
        const Clock::time_point now = Clock::now();
        const std::uint32_t level = QueueLevel(now);
        Clock::time_point end = g_port.playing.empty() ? now : g_port.playing.back();
        for (; g_port.advanced != 0; --g_port.advanced) {
            end += g_port.frame_duration;
            g_port.playing.push_back(end);
        }
        if (blocking == AUDIO3D_BLOCKING_ASYNC || level < g_port.queue_depth) return 0;
        wait_until = g_port.playing[level - g_port.queue_depth];
    }
    PreciseSleepUntil(wait_until);
    return 0;
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
    std::lock_guard lock(g_mutex);
    if (!PortIsOpen(port_id)) return AUDIO3D_ERROR_INVALID_PORT;
    g_port.advanced = 0;
    g_port.playing.clear();
    return 0;
}

}

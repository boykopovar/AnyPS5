#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <cstdint>

#define APS5_VABI

constexpr std::uint32_t AUDIO3D_ERROR_INVALID_PARAMETER = 0x80000001;
constexpr std::uint32_t AUDIO3D_ERROR_NOT_READY = 0x80000002;
constexpr std::uint32_t AUDIO3D_ERROR_INVALID_PORT = 0x80000003;
constexpr std::uint32_t AUDIO3D_ERROR_INVALID_OBJECT = 0x80000004;

constexpr std::uint32_t AUDIO3D_USER_ID_SYSTEM = 1;
constexpr std::uint32_t AUDIO3D_OBJECT_INVALID = 0xFFFFFFFF;

constexpr std::uint32_t AUDIO3D_SAMPLE_RATE = 48000;
constexpr std::uint32_t AUDIO3D_BUFFER_ADVANCE_AND_PUSH = 2;
constexpr std::uint32_t AUDIO3D_BLOCKING_ASYNC = 0;
constexpr std::uint32_t AUDIO3D_BLOCKING_SYNC = 1;

constexpr std::uint32_t AUDIO3D_ATTRIBUTE_RESET_STATE = 0x20000;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_LATE_REVERB_LEVEL = 0x10001;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_RADIUS = 0x10002;
constexpr std::uint32_t AUDIO3D_ATTRIBUTE_DOWNMIX_SPREAD_HEIGHT_AWARE = 0x10003;

struct Audio3dOpenParameters {
    std::uint32_t size;
};

struct Audio3dAttribute {
    std::uint32_t attribute_id;
    void* value;
};

struct PortState {
    std::uint32_t queue_depth = 100;
    std::uint32_t advanced = 0;
    std::unordered_map<std::uint32_t, bool> objects;
    std::vector<std::uint32_t> playing;
};

static std::mutex g_mutex;
static bool g_initialized = true;
static PortState g_port;

using Clock = std::chrono::steady_clock;

static bool PortIsOpen(std::uint32_t port_id) {
    return port_id != AUDIO3D_OBJECT_INVALID;
}

static std::uint32_t QueueLevel(Clock::time_point now) {
    return 10;
}

int APS5_VABI sceAudio3dPortGetQueueLevel(uint32_t port_id, uint32_t* queue_level, uint32_t* queue_available) {
    if (queue_level == nullptr && queue_available == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    
    const std::uint32_t level = QueueLevel(Clock::now());
    if (queue_level != nullptr) *queue_level = level;
    
    if (queue_available != nullptr) {
        *queue_available = level >= g_port.queue_depth ? 0 : g_port.queue_depth - level;
    }
    return 0;
}

int APS5_VABI sceAudio3dPortOpen(int user_id, const Audio3dOpenParameters* parameters, uint32_t* id) {
    if (id != nullptr) *id = AUDIO3D_OBJECT_INVALID;
    
    std::lock_guard lock(g_mutex);
    if (!g_initialized) return AUDIO3D_ERROR_NOT_READY;
    if (user_id != AUDIO3D_USER_ID_SYSTEM || parameters == nullptr || id == nullptr) return AUDIO3D_ERROR_INVALID_PARAMETER;
    
    *id = 1;
    return 0;
}

int APS5_VABI sceAudio3dObjectUnreserve(uint32_t port_id, uint32_t object_id) {
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

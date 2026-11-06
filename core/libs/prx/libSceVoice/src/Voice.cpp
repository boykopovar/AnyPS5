#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <vector>
#include "SceTypes.hpp"

namespace {

constexpr int SCE_VOICE_ERROR_NOT_INIT = static_cast<int>(0x8054000C);
constexpr int SCE_VOICE_ERROR_ARGUMENT_INVALID = static_cast<int>(0x80540001);
constexpr int SCE_VOICE_ERROR_TOPOLOGY = static_cast<int>(0x80540004);
constexpr int SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT = static_cast<int>(0x80540005);
constexpr int SCE_VOICE_ERROR_PORT_INVALID = static_cast<int>(0x80540006);
constexpr int SCE_VOICE_ERROR_NOT_SUPPORTED = static_cast<int>(0x8054000E);

constexpr std::int32_t PortTypeInDevice = 0;
constexpr std::int32_t PortTypeInPcmAudio = 1;
constexpr std::int32_t PortTypeInVoice = 2;
constexpr std::int32_t PortTypeOutPcmAudio = 3;
constexpr std::int32_t PortTypeOutVoice = 4;
constexpr std::int32_t PortTypeOutDevice = 5;

constexpr std::int32_t PortStateIdle = 0;
constexpr std::int32_t PortStateReady = 1;
constexpr std::int32_t PortStateRunning = 3;

constexpr std::int32_t AttributeVolume = 0;
constexpr std::int32_t AttributeMute = 1;
constexpr std::int32_t AttributeBitRate = 2;

constexpr std::uint32_t MaxPorts = 64;
constexpr std::uint32_t DefaultFrameSize = 320;
constexpr std::int32_t DefaultBitRate = 12200;
constexpr std::size_t MaxQueuedBytes = 1u << 20u;

struct Port {
    std::uint32_t id = 0;
    VoicePortParam param{};
    float volume = 1.0f;
    std::uint16_t mute = 0;
    std::int32_t bitrate = DefaultBitRate;
    std::deque<std::uint8_t> queue;
    std::set<std::uint32_t> sinks;
    std::set<std::uint32_t> sources;
};

std::mutex g_lock;
bool g_initialised = false;
bool g_started = false;
bool g_chatEnabled = false;
std::map<std::uint32_t, Port> g_ports;
std::uint32_t g_nextPort = 1;

bool isInput(const std::int32_t type) {
    return type == PortTypeInDevice || type == PortTypeInPcmAudio || type == PortTypeInVoice;
}

bool isOutput(const std::int32_t type) {
    return type == PortTypeOutPcmAudio || type == PortTypeOutVoice || type == PortTypeOutDevice;
}

bool isPassThrough(const std::int32_t type) {
    return type == PortTypeInPcmAudio || type == PortTypeOutPcmAudio;
}

Port* find(const std::uint32_t id) {
    const auto entry = g_ports.find(id);
    return entry == g_ports.end() ? nullptr : &entry->second;
}

}

extern "C" {
int APS5_VABI sceVoiceGetResourceInfo(VoiceResourceInfo* info) {
    if (info == nullptr) throw std::invalid_argument("sceVoiceGetResourceInfo: invalid argument");
    std::lock_guard lock(g_lock);
    if (!g_initialised) throw std::logic_error("sceVoiceGetResourceInfo: library not initialized");
    info->max_voice_input_ports = 7;
    info->max_voice_output_ports = 7;
    info->max_device_input_ports = 1;
    info->max_device_output_ports = 1;
    info->max_ports = 64;
    return 0;
}


int APS5_VABI sceVoiceInit(VoiceInitParam* param, std::int32_t version) {
    (void)version;
    if (param == nullptr) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    g_initialised = true;
    g_started = false;
    g_chatEnabled = false;
    g_ports.clear();
    g_nextPort = 1;
    return 0;
}

int APS5_VABI sceVoiceEnd_nid_postfix() {
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    g_initialised = false;
    g_started = false;
    g_chatEnabled = false;
    g_ports.clear();
    return 0;
}

int APS5_VABI sceVoiceEnableChat() {
    std::lock_guard lock(g_lock);
    if (!g_initialised) throw std::logic_error("sceVoiceEnableChat: library not initialized");
    g_chatEnabled = true;
    return 0;
}

int APS5_VABI sceVoiceDisableChat() {
    std::lock_guard lock(g_lock);
    if (!g_initialised) throw std::logic_error("sceVoiceDisableChat: library not initialized");
    g_chatEnabled = false;
    return 0;
}

int APS5_VABI sceVoiceStart(const VoiceStartParam* param) {
    if (param == nullptr) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    g_started = true;
    return 0;
}

int APS5_VABI sceVoiceStop() {
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    g_started = false;
    return 0;
}

int APS5_VABI sceVoiceCreatePort(std::uint32_t* port_id, const VoicePortParam* param) {
    if (port_id == nullptr || param == nullptr) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    if (!isInput(param->port_type) && !isOutput(param->port_type)) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    if (!isPassThrough(param->port_type)) return SCE_VOICE_ERROR_NOT_SUPPORTED;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    if (g_ports.size() >= MaxPorts) return SCE_VOICE_ERROR_RESOURCE_INSUFFICIENT;
    Port port;
    port.id = g_nextPort++;
    port.param = *param;
    port.volume = param->volume;
    port.mute = param->mute;
    g_ports[port.id] = std::move(port);
    *port_id = g_ports.rbegin()->first;
    return 0;
}

int APS5_VABI sceVoiceDeletePort(std::uint32_t port_id) {
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    Port* port = find(port_id);
    if (port == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    for (const auto sink : port->sinks) {
        if (Port* other = find(sink); other != nullptr) other->sources.erase(port_id);
    }
    for (const auto source : port->sources) {
        if (Port* other = find(source); other != nullptr) other->sinks.erase(port_id);
    }
    g_ports.erase(port_id);
    return 0;
}

int APS5_VABI sceVoiceConnectIPortToOPort(std::uint32_t input_port_id, std::uint32_t output_port_id) {
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    Port* input = find(input_port_id);
    Port* output = find(output_port_id);
    if (input == nullptr || output == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    if (!isInput(input->param.port_type) || !isOutput(output->param.port_type)) return SCE_VOICE_ERROR_TOPOLOGY;
    if (!input->sinks.insert(output_port_id).second) return SCE_VOICE_ERROR_TOPOLOGY;
    output->sources.insert(input_port_id);
    return 0;
}

int APS5_VABI sceVoiceDisconnectIPortFromOPort(std::uint32_t input_port_id, std::uint32_t output_port_id) {
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    Port* input = find(input_port_id);
    Port* output = find(output_port_id);
    if (input == nullptr || output == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    if (input->sinks.erase(output_port_id) == 0) return SCE_VOICE_ERROR_TOPOLOGY;
    output->sources.erase(input_port_id);
    return 0;
}

int APS5_VABI sceVoiceWriteToIPort(std::uint32_t input_port_id, const void* data, std::uint32_t* size, std::int16_t frame_gaps) {
    (void)frame_gaps;
    if (data == nullptr || size == nullptr) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    Port* input = find(input_port_id);
    if (input == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    if (!isInput(input->param.port_type)) return SCE_VOICE_ERROR_TOPOLOGY;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint32_t written = 0;
    for (const auto sink : input->sinks) {
        Port* output = find(sink);
        if (output == nullptr) continue;
        const std::size_t room = MaxQueuedBytes - std::min<std::size_t>(output->queue.size(), MaxQueuedBytes);
        const std::uint32_t accepted = static_cast<std::uint32_t>(std::min<std::size_t>(*size, room));
        if (input->mute == 0) output->queue.insert(output->queue.end(), bytes, bytes + accepted);
        written = std::max(written, accepted);
    }
    input->queue.clear();
    *size = input->sinks.empty() ? 0 : written;
    return 0;
}

int APS5_VABI sceVoiceReadFromOPort(std::uint32_t output_port_id, void* data, std::uint32_t* size) {
    if (data == nullptr || size == nullptr) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    Port* output = find(output_port_id);
    if (output == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    if (!isOutput(output->param.port_type)) return SCE_VOICE_ERROR_TOPOLOGY;
    const std::uint32_t available = static_cast<std::uint32_t>(output->queue.size());
    const std::uint32_t served = std::min(available, *size);
    auto* bytes = static_cast<std::uint8_t*>(data);
    for (std::uint32_t index = 0; index < served; ++index) {
        bytes[index] = output->queue.front();
        output->queue.pop_front();
    }
    *size = served;
    return 0;
}

int APS5_VABI sceVoiceGetPortInfo(std::uint32_t port_id, VoicePortInfo* info) {
    if (info == nullptr) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    const Port* port = find(port_id);
    if (port == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    std::memset(info, 0, sizeof(VoicePortInfo));
    info->port_type = port->param.port_type;
    info->state = g_started ? PortStateRunning : (port->queue.empty() ? PortStateIdle : PortStateReady);
    info->byte_count = static_cast<std::uint32_t>(port->queue.size());
    info->frame_size = DefaultFrameSize;
    info->edge_count = static_cast<std::uint16_t>(isInput(port->param.port_type) ? port->sinks.size() : port->sources.size());
    return 0;
}

int APS5_VABI sceVoiceGetVolume(std::uint32_t port_id, float* volume) {
    if (volume == nullptr) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    const Port* port = find(port_id);
    if (port == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    *volume = port->volume;
    return 0;
}

int APS5_VABI sceVoiceSetVolume(std::uint32_t port_id, float volume) {
    if (volume < 0.0f) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    Port* port = find(port_id);
    if (port == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    port->volume = volume;
    return 0;
}

int APS5_VABI sceVoiceSetMuteFlag(std::uint32_t port_id, bool muted) {
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    Port* port = find(port_id);
    if (port == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    port->mute = muted ? 1 : 0;
    return 0;
}

int APS5_VABI sceVoiceGetMuteFlag(std::uint32_t port_id, bool* muted) {
    if (muted == nullptr) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    const Port* port = find(port_id);
    if (port == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    *muted = port->mute != 0;
    return 0;
}

int APS5_VABI sceVoiceGetBitRate(std::uint32_t port_id, std::uint32_t* bitrate) {
    if (bitrate == nullptr) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    const Port* port = find(port_id);
    if (port == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    *bitrate = static_cast<std::uint32_t>(port->bitrate);
    return 0;
}

int APS5_VABI sceVoiceGetPortAttr(std::uint32_t port_id, std::int32_t attr, void* value, std::int32_t size) {
    if (value == nullptr || size <= 0) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    const Port* port = find(port_id);
    if (port == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    switch (attr) {
    case AttributeVolume:
        if (static_cast<std::size_t>(size) < sizeof(float)) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
        std::memcpy(value, &port->volume, sizeof(float));
        return 0;
    case AttributeMute:
        if (static_cast<std::size_t>(size) < sizeof(std::uint16_t)) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
        std::memcpy(value, &port->mute, sizeof(std::uint16_t));
        return 0;
    case 1001:
        if (size != 1) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
        std::memset(value, 0, 1);
        return 0;
    case AttributeBitRate:
        if (static_cast<std::size_t>(size) < sizeof(std::int32_t)) return SCE_VOICE_ERROR_ARGUMENT_INVALID;
        std::memcpy(value, &port->bitrate, sizeof(std::int32_t));
        return 0;
    default:
        return SCE_VOICE_ERROR_NOT_SUPPORTED;
    }
}

// Reset preserves the port identity, attributes, and connections.
int APS5_VABI sceVoiceResetPort(std::uint32_t port_id) {
    std::lock_guard lock(g_lock);
    if (!g_initialised) return SCE_VOICE_ERROR_NOT_INIT;
    auto* port = find(port_id);
    if (port == nullptr) return SCE_VOICE_ERROR_PORT_INVALID;
    port->queue.clear();
    return 0;
}

int APS5_VABI sceVoicePausePort(std::uint32_t port_id) {
    std::lock_guard lock(g_lock);
    if (!g_initialised) throw std::logic_error("sceVoicePausePort: library not initialized");
    if (find(port_id) == nullptr) throw std::logic_error("sceVoicePausePort: unknown port " + std::to_string(port_id));
    return 0;
}

int APS5_VABI sceVoiceResumePort(std::uint32_t port_id) {
    std::lock_guard lock(g_lock);
    if (!g_initialised) throw std::logic_error("sceVoiceResumePort: library not initialized");
    if (find(port_id) == nullptr) throw std::logic_error("sceVoiceResumePort: unknown port " + std::to_string(port_id));
    return 0;
}

}

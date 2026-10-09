#include <cstdint>
#include <cstddef>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t PortInDevice = 0;
constexpr std::int32_t PortInPcm = 1;
constexpr std::int32_t PortInVoice = 2;
constexpr std::int32_t PortOutPcm = 3;
constexpr std::int32_t PortOutVoice = 4;
constexpr std::int32_t PortOutDevice = 5;
constexpr std::int32_t PortStateReady = 1;
constexpr std::uint32_t VoiceFrameMs = 20;

struct Port {
    std::int32_t type;
    float volume;
    std::uint32_t bitrate;
    bool muted = false;
};

struct Voice {
    std::mutex mutex;
    bool initialized = false;
    bool started = false;
    std::uint32_t nextId = 0;
    std::map<std::uint32_t, Port> ports;
    std::set<std::pair<std::uint32_t, std::uint32_t>> connections;
};

Voice& State() {
    static Voice voice;
    return voice;
}

[[noreturn]] void Fail(const char* function, const std::string& why) {
    throw std::logic_error(std::string(function) + ": " + why);
}

Port& RequirePort(Voice& voice, std::uint32_t id, const char* function) {
    if (!voice.initialized) Fail(function, "library not initialized");
    const auto found = voice.ports.find(id);
    if (found == voice.ports.end()) Fail(function, "unknown port " + std::to_string(id));
    return found->second;
}

bool IsInput(std::int32_t type) { return type == PortInDevice || type == PortInPcm || type == PortInVoice; }

}

extern "C" {

int APS5_VABI sceVoiceInit(VoiceInitParam* param, int32_t version) {
    (void)version;
    if (param == nullptr) APS5_INVALID_ARG_EX;
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (voice.initialized) Fail(__func__, "already initialized");
    voice.initialized = true;
    return 0;
}

int APS5_VABI sceVoiceSetThreadsParams(void* params) {
    if (params == nullptr) APS5_INVALID_ARG_EX;
    return 0;
}

int APS5_VABI sceVoiceStart(const VoiceStartParam* param) {
    if (param == nullptr) APS5_INVALID_ARG_EX;
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) Fail(__func__, "library not initialized");
    voice.started = true;
    return 0;
}

int APS5_VABI sceVoiceStop(void) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) Fail(__func__, "library not initialized");
    voice.started = false;
    return 0;
}

int APS5_VABI sceVoiceEnd_nid_postfix(void) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) Fail(__func__, "library not initialized");
    voice.ports.clear();
    voice.connections.clear();
    voice.started = false;
    voice.initialized = false;
    return 0;
}

int APS5_VABI sceVoiceCreatePort(uint32_t* port_id, const VoicePortParam* param) {
    if (port_id == nullptr || param == nullptr) APS5_INVALID_ARG_EX;
    if (param->port_type < PortInDevice || param->port_type > PortOutDevice) Fail(__func__, "unknown port type " + std::to_string(param->port_type));
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) Fail(__func__, "library not initialized");
    const bool voicePort = param->port_type == PortInVoice || param->port_type == PortOutVoice;
    const auto id = voice.nextId++;
    voice.ports.emplace(id, Port{param->port_type, param->volume, voicePort ? static_cast<std::uint32_t>(param->voice.bitrate) : 0u});
    *port_id = id;
    return 0;
}

int APS5_VABI sceVoiceDeletePort(uint32_t port_id) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    RequirePort(voice, port_id, __func__);
    std::erase_if(voice.connections, [port_id](const auto& link) { return link.first == port_id || link.second == port_id; });
    voice.ports.erase(port_id);
    return 0;
}

int APS5_VABI sceVoiceConnectIPortToOPort(uint32_t input_port_id, uint32_t output_port_id) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!IsInput(RequirePort(voice, input_port_id, __func__).type)) Fail(__func__, "first port is not an input port");
    if (IsInput(RequirePort(voice, output_port_id, __func__).type)) Fail(__func__, "second port is not an output port");
    voice.connections.emplace(input_port_id, output_port_id);
    return 0;
}

int APS5_VABI sceVoiceDisconnectIPortFromOPort(uint32_t input_port_id, uint32_t output_port_id) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    RequirePort(voice, input_port_id, __func__);
    RequirePort(voice, output_port_id, __func__);
    if (voice.connections.erase({input_port_id, output_port_id}) == 0) Fail(__func__, "ports are not connected");
    return 0;
}

int APS5_VABI sceVoiceGetBitRate(uint32_t port_id, uint32_t* bitrate) {
    if (bitrate == nullptr) APS5_INVALID_ARG_EX;
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    const auto& port = RequirePort(voice, port_id, __func__);
    if (port.type != PortInVoice && port.type != PortOutVoice) Fail(__func__, "port does not carry encoded voice");
    *bitrate = port.bitrate;
    return 0;
}

int APS5_VABI sceVoiceSetVolume(uint32_t port_id, float volume) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    RequirePort(voice, port_id, __func__).volume = volume;
    return 0;
}

int APS5_VABI sceVoiceGetVolume(uint32_t port_id, float* volume) {
    if (volume == nullptr) APS5_INVALID_ARG_EX;
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    *volume = RequirePort(voice, port_id, __func__).volume;
    return 0;
}

int APS5_VABI sceVoiceGetPortInfo(uint32_t port_id, VoicePortInfo* info) {
    if (info == nullptr) APS5_INVALID_ARG_EX;
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    const auto& port = RequirePort(voice, port_id, __func__);
    *info = VoicePortInfo{};
    info->port_type = port.type;
    info->state = PortStateReady;
    if (port.bitrate != 0) info->frame_size = (port.bitrate * VoiceFrameMs + 7999u) / 8000u;
    return 0;
}

int APS5_VABI sceVoiceGetPortAttr(uint32_t port_id, int32_t attr, void* value, int32_t size) {
    (void)value;
    (void)size;
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    RequirePort(voice, port_id, __func__);
    Fail(__func__, "attribute " + std::to_string(attr) + " not implemented");
}

int APS5_VABI sceVoiceReadFromOPort(uint32_t output_port_id, void* data, uint32_t* size) {
    if (data == nullptr || size == nullptr) APS5_INVALID_ARG_EX;
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (IsInput(RequirePort(voice, output_port_id, __func__).type)) Fail(__func__, "port is not an output port");
    *size = 0;
    return 0;
}

int APS5_VABI sceVoiceWriteToIPort(uint32_t input_port_id, const void* data, uint32_t* size, int16_t frame_gaps) {
    (void)input_port_id;
    (void)data;
    (void)size;
    (void)frame_gaps;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceSetMuteFlag(uint32_t port_id, bool muted) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    RequirePort(voice, port_id, __func__).muted = muted;
    return 0;
}

int APS5_VABI sceVoiceGetMuteFlag(uint32_t port_id, bool* muted) {
    if (muted == nullptr) APS5_INVALID_ARG_EX;
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    *muted = RequirePort(voice, port_id, __func__).muted;
    return 0;
}

int APS5_VABI sceVoiceGetResourceInfo(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceEnableChat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceResetPort(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceDisableChat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

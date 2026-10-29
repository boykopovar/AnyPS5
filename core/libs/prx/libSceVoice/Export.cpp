#include "prx/libSceVoice/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <deque>
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
constexpr int VoiceErrorNotInitialized = static_cast<int>(0x804E0801u);
constexpr auto NotInitialized = static_cast<std::int32_t>(0x804e0801u);
constexpr auto InvalidPort = static_cast<std::int32_t>(0x804e0804u);
constexpr auto InvalidArgument = static_cast<std::int32_t>(0x804e0805u);
constexpr auto PortNotFound = static_cast<std::int32_t>(0x804e0807u);
constexpr std::int32_t ThreadPriorityHighest = 256;
constexpr std::int32_t ThreadPriorityLowest = 767;
constexpr std::uint64_t ThreadAffinityAll = 0x1fff;

struct Port {
    std::int32_t type;
    float volume;
    std::uint32_t bitrate;
    std::uint16_t bufferSize;
    std::uint16_t frameSize;
    bool muted = false;
    bool paused = false;
};

struct Voice {
    std::mutex mutex;
    bool initialized = false;
    bool started = false;
    std::int32_t appType = 0;
    bool mutedAll = false;
    std::deque<std::uint32_t> freePorts;
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

bool IsThreadSetting(std::int32_t priority, std::uint64_t affinity) {
    return priority >= ThreadPriorityHighest && priority <= ThreadPriorityLowest && affinity != 0 && (affinity & ~ThreadAffinityAll) == 0;
}

Port& RequirePort(Voice& voice, std::uint32_t id, const char* function) {
    if (!voice.initialized) Fail(function, "library not initialized");
    const auto found = voice.ports.find(id);
    if (found == voice.ports.end()) Fail(function, "unknown port " + std::to_string(id));
    return found->second;
}

bool IsInput(std::int32_t type) { return type == PortInDevice || type == PortInPcm || type == PortInVoice; }

bool VoiceFrame(std::int32_t bitrate, std::uint16_t& frameSize, std::uint16_t& frameDuration) {
    switch (bitrate) {
    case 3850: frameSize = 20; frameDuration = 40; return true;
    case 4650: frameSize = 24; frameDuration = 40; return true;
    case 5700: frameSize = 15; frameDuration = 20; return true;
    case 7300: frameSize = 19; frameDuration = 20; return true;
    case 11800: frameSize = 15; frameDuration = 10; return true;
    case 12200: frameSize = 16; frameDuration = 10; return true;
    case 14400: frameSize = 27; frameDuration = 15; return true;
    case 16000: frameSize = 20; frameDuration = 10; return true;
    case 22533: frameSize = 43; frameDuration = 15; return true;
    default: return false;
    }
}

}

extern "C" {

int APS5_VABI sceVoiceInit(const VoiceInitParam* param, std::int32_t version) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (voice.initialized) return static_cast<std::int32_t>(0x804e0802u);
    if (!param || version != 100) return InvalidArgument;
    if (param->app_type != 4 && param->app_type != 0x08000000 && param->app_type != 0x10000000 && param->app_type != 0x20000000) return InvalidArgument;
    voice.appType = param->app_type;
    voice.freePorts.clear();
    for (std::uint32_t id = 0; id < 128; ++id) voice.freePorts.push_back(id);
    voice.initialized = true;
    return 0;
}

int APS5_VABI sceVoiceSetThreadsParams(const VoiceThreadsParams* params) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (!params) return InvalidArgument;
    if (voice.started) return static_cast<std::int32_t>(0x804e080bu);
    if (params->mode == 0) return 0;
    if (params->mode != 1) return InvalidArgument;
    if (!IsThreadSetting(params->firstPriority, params->firstAffinity) || !IsThreadSetting(params->secondPriority, params->secondAffinity) || !IsThreadSetting(params->thirdPriority, params->thirdAffinity)) {
        Fail(__func__, "the result for a priority outside 256-767 or an affinity mask outside 0x1fff is unknown");
    }
    return 0;
}

int APS5_VABI sceVoiceStart(const VoiceStartParam* param) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (!param || !param->container) return InvalidArgument;
    if (voice.started) return 0;
    const std::uint32_t memorySize = voice.appType == 0x10000000 || voice.appType == 0x20000000 ? 0x40000 : 0xc0000;
    if (param->mem_size != memorySize) return static_cast<std::int32_t>(0x804e0806u);
    voice.started = true;
    return 0;
}

int APS5_VABI sceVoiceStop(void) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    voice.started = false;
    return 0;
}

int APS5_VABI sceVoiceEnd_nid_postfix(void) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    voice.ports.clear();
    voice.connections.clear();
    voice.started = false;
    voice.appType = 0;
    voice.mutedAll = false;
    voice.initialized = false;
    return 0;
}

int APS5_VABI sceVoiceCreatePort(std::uint32_t* portId, const VoicePortParam* param) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (!portId || !param) return InvalidArgument;
    *portId = 255;
    std::uint16_t frameSize = 512;
    std::uint16_t frameDuration = 16;
    const auto type = param->port_type;
    if (type < PortInDevice || type > PortOutDevice) return InvalidArgument;
    if (type == PortInPcm || type == PortOutPcm) {
        if (param->pcmaudio.sample_rate != 16000 || param->pcmaudio.data_type != 0) return InvalidArgument;
    } else if (type == PortInVoice || type == PortOutVoice) {
        if (!VoiceFrame(param->voice.bitrate, frameSize, frameDuration)) return InvalidArgument;
    }
    constexpr auto resourceFull = static_cast<std::int32_t>(0x804e0808u);
    if (voice.freePorts.empty()) return resourceFull;
    std::uint32_t count = 0;
    std::uint32_t pcmCount = 0;
    std::uint32_t pcmBytes = 0;
    for (const auto& [id, port] : voice.ports) {
        if (port.type == type) ++count;
        if (port.type == PortInPcm || port.type == PortOutPcm) {
            ++pcmCount;
            pcmBytes += 2u * port.bufferSize;
        }
    }
    std::uint16_t bufferSize = 2048;
    if (type == PortInDevice || type == PortOutDevice) {
        const std::uint32_t limit = type == PortOutDevice && voice.appType == 4 ? 5 : 4;
        if (count == limit) return resourceFull;
        if (type == PortInDevice ? param->device.type != 0 : param->device.type != 2 && param->device.type != 14) return InvalidArgument;
        bufferSize = 4096;
    } else if (type == PortInPcm || type == PortOutPcm) {
        bufferSize = static_cast<std::uint16_t>(param->pcmaudio.buffer_size - param->pcmaudio.buffer_size % frameSize);
        if (bufferSize < 1024 || bufferSize > 64512) return InvalidArgument;
        if (type == PortInPcm && bufferSize / frameSize * frameDuration < param->threshold) return resourceFull;
        const std::uint32_t limit = voice.appType == 4 ? 128 : voice.appType == 0x08000000 ? 30 : 14;
        const std::uint32_t byteLimit = voice.appType == 4 || voice.appType == 0x08000000 ? 0x80000 : voice.appType == 0x10000000 ? 0x5000 : 0x10000;
        if (pcmCount == limit || pcmBytes + 2u * bufferSize > byteLimit) return resourceFull;
    } else {
        if (type == PortInVoice && bufferSize / frameSize * frameDuration < param->threshold) return resourceFull;
        const std::uint32_t limit = voice.appType == 4 ? 0 : type == PortOutVoice ? 4 : voice.appType == 0x20000000 ? 7 : 15;
        if (count == limit) return resourceFull;
    }
    const auto id = voice.freePorts.front();
    const bool voicePort = type == PortInVoice || type == PortOutVoice;
    voice.ports.emplace(id, Port{type, param->volume, voicePort ? static_cast<std::uint32_t>(param->voice.bitrate) : 0u, bufferSize, frameSize, param->mute != 0});
    voice.freePorts.pop_front();
    *portId = id;
    return 0;
}

int APS5_VABI sceVoiceDeletePort(uint32_t port_id) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (port_id > 127) return InvalidPort;
    if (!voice.ports.contains(port_id)) return PortNotFound;
    std::erase_if(voice.connections, [port_id](const auto& link) { return link.first == port_id || link.second == port_id; });
    voice.ports.erase(port_id);
    voice.freePorts.push_back(port_id);
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

int APS5_VABI sceVoiceGetBitRate(std::uint32_t portId, std::uint32_t* bitrate) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    if (!bitrate) return InvalidArgument;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    const auto& port = found->second;
    if (port.type != PortInVoice && port.type != PortOutVoice) return PortNotFound;
    *bitrate = port.bitrate;
    return 0;
}

int APS5_VABI sceVoiceSetVolume(std::uint32_t portId, float volume) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    found->second.volume = std::isfinite(volume) ? volume : 0.0f;
    return 0;
}

int APS5_VABI sceVoiceGetVolume(std::uint32_t portId, float* volume) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    if (!volume) return InvalidArgument;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    *volume = found->second.volume;
    return 0;
}

int APS5_VABI sceVoiceGetPortInfo(uint32_t port_id, VoicePortInfo* info) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return VoiceErrorNotInitialized;
    if (info == nullptr) APS5_INVALID_ARG_EX;
    const auto& port = RequirePort(voice, port_id, __func__);
    info->port_type = port.type;
    info->state = PortStateReady;
    info->byte_count = 0;
    info->frame_size = port.bitrate != 0 ? port.frameSize : 2;
    return 0;
}

int APS5_VABI sceVoiceGetPortAttr(std::uint32_t portId, std::int32_t attr, void* value, std::int32_t size) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    if (value == nullptr) return InvalidArgument;
    std::int32_t expectedSize;
    switch (attr) {
    case 1000:
    case 1001:
    case 2000:
    case 2002:
        expectedSize = 1;
        break;
    case 1002:
    case 2001:
        expectedSize = 4;
        break;
    case 1003:
        expectedSize = 2;
        break;
    default:
        return InvalidArgument;
    }
    if (size != expectedSize) return InvalidArgument;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return InvalidPort;
    const auto requiredType = attr == 1003 ? PortOutDevice : PortInDevice;
    if (found->second.type != requiredType) return InvalidArgument;
    Fail(__func__, "device attribute backend is not implemented");
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

int APS5_VABI sceVoiceSetMuteFlag(std::uint32_t portId, std::uint8_t muted) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    found->second.muted = muted != 0;
    return 0;
}

int APS5_VABI sceVoiceGetMuteFlag(std::uint32_t portId, bool* muted) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    if (!muted) return InvalidArgument;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    *muted = found->second.muted || voice.mutedAll;
    return 0;
}

int APS5_VABI sceVoiceGetResourceInfo(VoiceResourceInfo* info) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (info == nullptr) return InvalidArgument;
    switch (voice.appType) {
    case 4:
        *info = {0, 0, 4, 5};
        break;
    case 0x08000000:
    case 0x10000000:
        *info = {15, 4, 4, 4};
        break;
    case 0x20000000:
        *info = {7, 4, 4, 4};
        break;
    default:
        Fail(__func__, "unsupported initialization mode");
    }
    return 0;
}

int APS5_VABI sceVoiceEnableChat(void) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    Fail(__func__, "audio chat backend is not implemented");
}

int APS5_VABI sceVoiceResetPort(std::uint32_t portId) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    if (voice.started && (found->second.type == PortInDevice || found->second.type == PortOutDevice)) Fail(__func__, "device reset backend is not implemented");
    return 0;
}

int APS5_VABI sceVoicePausePort(std::uint32_t portId) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    found->second.paused = true;
    return 0;
}

int APS5_VABI sceVoiceResumePort(std::uint32_t portId) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    found->second.paused = false;
    return 0;
}

int APS5_VABI sceVoiceDisableChat(void) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    Fail(__func__, "audio chat backend is not implemented");
}

int APS5_VABI sceVoiceInitHQ(const VoiceInitParam* param, std::int32_t version) {
    return sceVoiceInit(param, version);
}

int APS5_VABI sceVoiceGetMemorySize(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceDeviceInCustomerData(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceQAEnableRecording(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceVADAdjustment(float, float) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceVADPVTAdjustment(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceVADSetVersion(std::int32_t version) {
    return version == 0 || version == 1 ? 0 : InvalidArgument;
}

int APS5_VABI sceVoiceSetMuteFlagAll(bool muted) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    voice.mutedAll = muted;
    return 0;
}

int APS5_VABI sceVoiceSetBitRate(std::uint32_t portId, std::uint32_t bitrate) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    auto& port = found->second;
    if (port.type != PortInVoice && port.type != PortOutVoice) Fail(__func__, "port does not carry encoded voice");
    std::uint16_t frameSize = 0;
    std::uint16_t frameDuration = 0;
    if (bitrate > INT32_MAX || !VoiceFrame(static_cast<std::int32_t>(bitrate), frameSize, frameDuration)) Fail(__func__, "unsupported bitrate " + std::to_string(bitrate));
    port.bitrate = bitrate;
    port.frameSize = frameSize;
    return 0;
}

int APS5_VABI sceVoiceUpdatePort(std::uint32_t portId, const VoicePortParam* param) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return NotInitialized;
    if (portId > 127) return InvalidPort;
    if (!param) Fail(__func__, "null port parameters");
    const auto found = voice.ports.find(portId);
    if (found == voice.ports.end()) return PortNotFound;
    auto& port = found->second;
    if (param->port_type != port.type) Fail(__func__, "port type " + std::to_string(param->port_type) + " differs from the port's " + std::to_string(port.type));
    std::uint16_t frameSize = port.frameSize;
    std::uint16_t frameDuration = 0;
    const bool voicePort = port.type == PortInVoice || port.type == PortOutVoice;
    if (voicePort && !VoiceFrame(param->voice.bitrate, frameSize, frameDuration)) Fail(__func__, "unsupported bitrate " + std::to_string(param->voice.bitrate));
    port.muted = param->mute != 0;
    port.volume = std::isfinite(param->volume) ? param->volume : 0.0f;
    if (voicePort) {
        port.bitrate = static_cast<std::uint32_t>(param->voice.bitrate);
        port.frameSize = frameSize;
    }
    return 0;
}

int APS5_VABI sceVoicePausePortAll(void) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return VoiceErrorNotInitialized;
    return 0;
}

int APS5_VABI sceVoiceResumePortAll(void) {
    auto& voice = State();
    std::lock_guard lock(voice.mutex);
    if (!voice.initialized) return VoiceErrorNotInitialized;
    return 0;
}

}

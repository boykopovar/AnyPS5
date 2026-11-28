#include "prx/libSceVoiceQoS/include/Api.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <map>
#include <mutex>
#include <utility>
#include "SceTypes.hpp"
#include "prx/libSceVoice/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

namespace {

constexpr std::int32_t VOICE_QOS_APP_TYPE_GAME = 0x20000000;
constexpr std::int32_t VOICE_QOS_APP_TYPE_10000000 = 0x10000000;
constexpr std::uint32_t VOICE_QOS_MEMORY_SIZE = 0x40000;
constexpr int SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT = static_cast<int>(0x804E0901);
constexpr int SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID = static_cast<int>(0x804E0902);
constexpr int SCE_VOICE_ERROR_LIBVOICEQOS_LIMIT = static_cast<int>(0x804E0907);
constexpr int SCE_VOICE_ERROR_LIBVOICEQOS_INITIALIZED = static_cast<int>(0x804E0905);
constexpr std::int32_t VOICE_PORT_IN_DEVICE = 0;
constexpr std::int32_t VOICE_PORT_OUT_DEVICE = 5;
constexpr std::int32_t VOICE_PORT_IN_VOICE = 2;
constexpr std::uint16_t VOICE_PORT_THRESHOLD = 100;
constexpr std::int32_t VOICE_DEVICE_INPUT = 0;
constexpr std::int32_t VOICE_DEVICE_OUTPUT = 2;
constexpr int GUEST_CLOCK_MONOTONIC = 4;
constexpr float VOICE_QOS_STATUS_TIMEOUT_SECONDS = 120.0f;

struct Endpoint {
    std::uint32_t inPort = 0;
    std::uint32_t outPort = 0;
};

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

float MonotonicSeconds() {
    KernelTimespec now{};
    const int savedErrno = errno;
    const int result = clock_gettime_nid_postfix(GUEST_CLOCK_MONOTONIC, &now);
    errno = savedErrno;
    if (result != 0) throw std::runtime_error("sceVoiceQoS: clock_gettime(CLOCK_MONOTONIC) failed");
    return static_cast<float>(static_cast<double>(now.tv_sec) + static_cast<double>(now.tv_nsec) / 1e9);
}

struct Connection {
    std::int32_t local;
    std::int32_t remote;
    float resetTime;
};

constexpr std::int32_t VOICE_QOS_CONNECTION_ATTR_MAX_BITRATE = 4;
constexpr std::int32_t VOICE_QOS_CONNECTION_ATTR_BITRATE = 9;
constexpr std::size_t VOICE_QOS_CONNECTION_RECORDS = 15;
constexpr std::int32_t VoiceQoSBitrates[] = {3850, 4650, 5700, 7300, 14400, 16000, 22533, 24000, 28000, 32000, 36000, 40000, 44000, 48000};
struct ConnectionBitrate {
    std::int32_t maximum = 0;
    std::int32_t current = 0;
};
std::int32_t QuantizeBitrate(std::int32_t bitrate) {
    std::int32_t result = VoiceQoSBitrates[0];
    for (const std::int32_t supported : VoiceQoSBitrates) {
        if (bitrate >= supported) result = supported;
    }
    return result;
}
struct QoS {
    std::mutex mutex;
    bool initialized = false;
    std::int32_t appType = 0;
    std::int32_t endpointLimit = 0;
    std::array<ConnectionBitrate, VOICE_QOS_CONNECTION_RECORDS> bitrates{};
    std::map<std::int32_t, Endpoint> locals;
    std::map<std::int32_t, Endpoint> remotes;
    std::map<std::int32_t, Connection> connections;
    bool packetTraffic = false;
};

QoS& State() {
    static QoS qos;
    return qos;
}

std::int32_t ConnectionBitrateCap(const QoS& qos) {
    return qos.appType == VOICE_QOS_APP_TYPE_10000000 ? 22533 : 7300;
}

template<typename TValue>
int CopyStatus(void* destination, TValue value) {
    return CopyOut(destination, &value, sizeof(value)) ? 0 : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
}

int CreateEndpoint(std::int32_t* id, std::map<std::int32_t, Endpoint>& endpoints, std::int32_t limit) {
    if (!id) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    const int savedErrno = errno;
    const GuestArena::HostWrite write(id, sizeof(*id));
    if (!write.Open() || !GuestWritable_nid_no_patch(id, sizeof(*id))) {
        errno = savedErrno;
        return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    }
    errno = savedErrno;
    for (std::int32_t candidate = 0; candidate < limit; ++candidate) {
        if (!endpoints.contains(candidate)) {
            if (!CopyOut(id, &candidate, sizeof(candidate))) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            endpoints.emplace(candidate, Endpoint{});
            return 0;
        }
    }
    const std::int32_t invalidId = -1;
    return CopyOut(id, &invalidId, sizeof(invalidId)) ? SCE_VOICE_ERROR_LIBVOICEQOS_LIMIT : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
}

int DeleteEndpoint(std::int32_t id, std::map<std::int32_t, Endpoint>& endpoints) {
    return endpoints.erase(id) != 0 ? 0 : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
}

const Endpoint* LocalEndpoint(const QoS& qos) {
    const auto found = qos.locals.find(0);
    return found == qos.locals.end() ? nullptr : &found->second;
}

template<typename TBody>
int Locked(TBody&& body) {
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    if (!qos.initialized) return SCE_VOICE_ERROR_LIBVOICEQOS_NOT_INIT;
    return body(qos);
}

}

extern "C" {

int APS5_VABI sceVoiceQoSInit(void* memBlock, uint32_t memSize, int32_t appType) {
    auto& qos = State();
    std::lock_guard lock(qos.mutex);
    if (qos.initialized) return SCE_VOICE_ERROR_LIBVOICEQOS_INITIALIZED;
    if (!memBlock || memSize != VOICE_QOS_MEMORY_SIZE || (appType != VOICE_QOS_APP_TYPE_GAME && appType != VOICE_QOS_APP_TYPE_10000000)) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    VoiceInitParam initParam{};
    initParam.app_type = appType;
    int result = sceVoiceInit(&initParam, 100);
    if (result != 0) return result;
    VoiceStartParam startParam{};
    startParam.container = memBlock;
    startParam.mem_size = memSize;
    result = sceVoiceStart(&startParam);
    if (result != 0) {
        sceVoiceEnd_nid_postfix();
        return result;
    }
    qos.endpointLimit = appType == VOICE_QOS_APP_TYPE_10000000 ? 15 : 7;
    qos.appType = appType;
    const ConnectionBitrate initial = appType == VOICE_QOS_APP_TYPE_10000000 ? ConnectionBitrate{22533, 3850} : ConnectionBitrate{7300, 3850};
    qos.bitrates.fill(initial);
    qos.initialized = true;
    return 0;
}

int APS5_VABI sceVoiceQoSEnd() {
    return Locked([](QoS& qos) {
        for (const auto& [id, endpoint] : qos.locals) {
            sceVoiceDeletePort(endpoint.inPort);
            sceVoiceDeletePort(endpoint.outPort);
        }
        for (const auto& [id, endpoint] : qos.remotes) sceVoiceDeletePort(endpoint.inPort);
        qos.connections.clear();
        qos.locals.clear();
        qos.remotes.clear();
        qos.initialized = false;
        sceVoiceStop();
        return sceVoiceEnd_nid_postfix();
    });
}

int APS5_VABI sceVoiceQoSCreateLocalEndpoint(int32_t* localId, int32_t userId, int32_t deviceInId, int32_t deviceOutId) {
    return Locked([&](QoS& qos) -> int {
        if (!localId) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const int savedErrno = errno;
        const GuestArena::HostWrite write(localId, sizeof(*localId));
        if (!write.Open() || !GuestWritable_nid_no_patch(localId, sizeof(*localId))) {
            errno = savedErrno;
            return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        }
        errno = savedErrno;
        if (userId == -1 || !qos.locals.empty()) {
            const std::int32_t invalidId = -1;
            return CopyOut(localId, &invalidId, sizeof(invalidId)) ? SCE_VOICE_ERROR_LIBVOICEQOS_LIMIT : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        }
        VoicePortParam inParam{};
        inParam.port_type = VOICE_PORT_IN_DEVICE;
        inParam.volume = 1.0f;
        inParam.device.user_id = userId;
        inParam.device.type = VOICE_DEVICE_INPUT;
        inParam.device.index = deviceInId;
        std::uint32_t inPort = 0;
        int result = sceVoiceCreatePort(&inPort, &inParam);
        if (result != 0) return result;
        VoicePortParam outParam = inParam;
        outParam.port_type = VOICE_PORT_OUT_DEVICE;
        outParam.device.type = VOICE_DEVICE_OUTPUT;
        outParam.device.index = deviceOutId;
        std::uint32_t outPort = 0;
        result = sceVoiceCreatePort(&outPort, &outParam);
        if (result != 0) {
            sceVoiceDeletePort(inPort);
            return result;
        }
        const std::int32_t createdId = 0;
        if (!CopyOut(localId, &createdId, sizeof(createdId))) {
            sceVoiceDeletePort(inPort);
            sceVoiceDeletePort(outPort);
            return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        }
        qos.locals.emplace(0, Endpoint{inPort, outPort});
        return 0;
    });
}

int APS5_VABI sceVoiceQoSDeleteLocalEndpoint(int32_t localId) {
    return Locked([&](QoS& qos) -> int {
        if (localId != 0) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const auto found = qos.locals.find(localId);
        if (found == qos.locals.end()) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        sceVoiceDeletePort(found->second.inPort);
        const int result = sceVoiceDeletePort(found->second.outPort);
        qos.locals.erase(found);
        return result;
    });
}

int APS5_VABI sceVoiceQoSCreateRemoteEndpoint(int32_t* remoteId) {
    return Locked([&](QoS& qos) -> int {
        if (!remoteId) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        std::int32_t slot = -1;
        for (std::int32_t id = 0; id < qos.endpointLimit; ++id) {
            if (!qos.remotes.contains(id)) {
                slot = id;
                break;
            }
        }
        if (slot < 0) {
            const int savedErrno = errno;
            const GuestArena::HostWrite write(remoteId, sizeof(*remoteId));
            const std::int32_t invalidId = -1;
            const bool copied = write.Open() && CopyOut(remoteId, &invalidId, sizeof(invalidId));
            errno = savedErrno;
            return copied ? SCE_VOICE_ERROR_LIBVOICEQOS_LIMIT : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        }
        VoicePortParam param{};
        param.port_type = VOICE_PORT_IN_VOICE;
        param.threshold = VOICE_PORT_THRESHOLD;
        param.volume = 1.0f;
        param.voice.bitrate = ConnectionBitrateCap(qos);
        std::uint32_t inPort = 0;
        const int result = sceVoiceCreatePort(&inPort, &param);
        if (result != 0) return result;
        const int created = CreateEndpoint(remoteId, qos.remotes, qos.endpointLimit);
        if (created != 0) {
            sceVoiceDeletePort(inPort);
            return created;
        }
        qos.remotes[slot].inPort = inPort;
        return 0;
    });
}

int APS5_VABI sceVoiceQoSDeleteRemoteEndpoint(int32_t remoteId) {
    return Locked([&](QoS& qos) -> int {
        const auto found = qos.remotes.find(remoteId);
        if (found == qos.remotes.end()) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        sceVoiceDeletePort(found->second.inPort);
        const int result = DeleteEndpoint(remoteId, qos.remotes);
        for (auto it = qos.connections.begin(); it != qos.connections.end(); ++it) {
            if (it->second.remote == remoteId) {
                qos.connections.erase(it);
                break;
            }
        }
        return result;
    });
}

int APS5_VABI sceVoiceQoSConnect(int32_t* connectionId, int32_t localId, int32_t remoteId) {
    return Locked([&](QoS& qos) {
        if (!connectionId || localId != 0 || remoteId < 0 || remoteId >= qos.endpointLimit) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const int savedErrno = errno;
        const GuestArena::HostWrite write(connectionId, sizeof(*connectionId));
        if (!write.Open() || !GuestWritable_nid_no_patch(connectionId, sizeof(*connectionId))) {
            errno = savedErrno;
            return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        }
        errno = savedErrno;
        if (!qos.locals.contains(localId) || !qos.remotes.contains(remoteId)) throw std::runtime_error("sceVoiceQoSConnect: missing voice port error behavior is not implemented");
        for (std::int32_t candidate = 0; candidate < qos.endpointLimit; ++candidate) {
            if (!qos.connections.contains(candidate)) {
                const float resetTime = MonotonicSeconds();
                if (!CopyOut(connectionId, &candidate, sizeof(candidate))) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
                qos.connections.emplace(candidate, Connection{localId, remoteId, resetTime});
                return 0;
            }
        }
        const std::int32_t invalidId = -1;
        return CopyOut(connectionId, &invalidId, sizeof(invalidId)) ? SCE_VOICE_ERROR_LIBVOICEQOS_LIMIT : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    });
}

int APS5_VABI sceVoiceQoSDisconnect(int32_t connectionId) {
    return Locked([&](QoS& qos) {
        if (connectionId < 0 || connectionId >= qos.endpointLimit) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        qos.connections.erase(connectionId);
        return 0;
    });
}

int APS5_VABI sceVoiceQoSSetConnectionAttribute(std::int32_t connectionId, std::int32_t attributeId, const void* value, std::int32_t size) {
    return Locked([&](QoS& qos) {
        if (connectionId < 0 || connectionId >= qos.endpointLimit || !value) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        if ((attributeId != VOICE_QOS_CONNECTION_ATTR_MAX_BITRATE && attributeId != VOICE_QOS_CONNECTION_ATTR_BITRATE) || size != 4) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        std::int32_t requested = 0;
        if (!CopyIn(&requested, value, sizeof(requested))) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        ConnectionBitrate& record = qos.bitrates[static_cast<std::size_t>(connectionId)];
        if (attributeId == VOICE_QOS_CONNECTION_ATTR_MAX_BITRATE) {
            const std::int32_t cap = qos.appType == VOICE_QOS_APP_TYPE_10000000 ? 22533 : 7300;
            record.maximum = std::min(std::max(requested, record.current), cap);
        } else {
            record.current = QuantizeBitrate(std::min(std::max(requested, 3850), record.maximum));
        }
        return 0;
    });
}

int APS5_VABI sceVoiceQoSSetLocalEndpointAttribute(int32_t localId, int32_t attributeId, const void* value, int32_t size) {
    return Locked([&](QoS& qos) -> int {
        if (!value || localId != 0 || attributeId < 0 || attributeId > 3) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const Endpoint* endpoint = LocalEndpoint(qos);
        if (!endpoint) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const std::uint32_t inPort = endpoint->inPort;
        const std::uint32_t outPort = endpoint->outPort;
        switch (attributeId) {
        case 0:
        case 2: {
            if (size != 4) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            float volume = 0;
            if (!CopyIn(&volume, value, sizeof(volume))) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            return sceVoiceSetVolume(attributeId == 0 ? inPort : outPort, volume);
        }
        default: {
            if (size != 1) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            std::uint8_t mute = 0;
            if (!CopyIn(&mute, value, sizeof(mute))) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            return sceVoiceSetMuteFlag(attributeId == 1 ? inPort : outPort, mute);
        }
        }
    });
}

int APS5_VABI sceVoiceQoSSetRemoteEndpointAttribute(int32_t remoteId, int32_t attributeId, const void* value, int32_t size) {
    return Locked([&](QoS& qos) -> int {
        if (remoteId < 0 || remoteId >= qos.endpointLimit || !value || attributeId != 7 || size != 1) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const auto found = qos.remotes.find(remoteId);
        if (found == qos.remotes.end()) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        std::uint8_t mute = 0;
        if (!CopyIn(&mute, value, sizeof(mute))) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        return sceVoiceSetMuteFlag(found->second.inPort, mute);
    });
}

int APS5_VABI sceVoiceQoSReadPacket(int32_t connectionId, void* buffer, uint32_t* size) {
    return Locked([&](QoS& qos) {
        qos.packetTraffic = true;
        if (!buffer || !size || !qos.connections.contains(connectionId)) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const int savedErrno = errno;
        const GuestArena::HostWrite write(size, sizeof(*size));
        if (!write.Open() || !GuestWritable_nid_no_patch(size, sizeof(*size))) {
            errno = savedErrno;
            return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        }
        errno = savedErrno;
        const std::uint32_t zero = 0;
        return CopyOut(size, &zero, sizeof(zero)) ? 0 : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    });
}

int APS5_VABI sceVoiceQoSWritePacket(int32_t connectionId, const void* buffer, uint32_t* size) {
    return Locked([&](QoS& qos) {
        qos.packetTraffic = true;
        if (!buffer || !size || !qos.connections.contains(connectionId)) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        std::uint32_t localSize = 0;
        if (!CopyIn(&localSize, size, sizeof(localSize))) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        return 0;
    });
}

int APS5_VABI sceVoiceQoSGetLocalEndpointAttribute(int32_t localId, int32_t attributeId, void* value, int32_t size) {
    return Locked([&](QoS& qos) -> int {
        if (!value || localId != 0 || attributeId < 0 || attributeId > 8) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const Endpoint* endpoint = LocalEndpoint(qos);
        if (!endpoint) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const std::uint32_t inPort = endpoint->inPort;
        const std::uint32_t outPort = endpoint->outPort;
        switch (attributeId) {
        case 0:
            if (size != 4) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            return sceVoiceGetVolume(inPort, static_cast<float*>(value));
        case 1:
            if (size != 1) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            return sceVoiceGetMuteFlag(inPort, static_cast<bool*>(value));
        case 2:
            if (size != 4) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            return sceVoiceGetVolume(outPort, static_cast<float*>(value));
        case 3:
            if (size != 1) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            return sceVoiceGetMuteFlag(outPort, static_cast<bool*>(value));
        case 5:
            if (size != 1) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            return sceVoiceGetPortAttr(inPort, 1001, value, size);
        case 6:
            if (size != 4) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            return sceVoiceGetPortAttr(inPort, 1002, value, size);
        case 8:
            if (size != 2) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
            return sceVoiceGetPortAttr(outPort, 1003, value, size);
        default:
            return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        }
    });
}

int APS5_VABI sceVoiceQoSGetRemoteEndpointAttribute(int32_t remoteId, int32_t attributeId, void* value, int32_t size) {
    return Locked([&](QoS& qos) -> int {
        if (!value || remoteId < 0 || remoteId >= qos.endpointLimit || attributeId != 7 || size != 1) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const auto found = qos.remotes.find(remoteId);
        if (found == qos.remotes.end()) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        return sceVoiceGetMuteFlag(found->second.inPort, static_cast<bool*>(value));
    });
}

int APS5_VABI sceVoiceQoSGetConnectionAttribute(int32_t connectionId, int32_t attributeId, void* value, int32_t size) {
    return Locked([&](QoS& qos) {
        if (connectionId < 0 || connectionId >= qos.endpointLimit || !value) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        if ((attributeId != VOICE_QOS_CONNECTION_ATTR_MAX_BITRATE && attributeId != VOICE_QOS_CONNECTION_ATTR_BITRATE) || size != 4) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const ConnectionBitrate& record = qos.bitrates[static_cast<std::size_t>(connectionId)];
        const std::int32_t result = attributeId == VOICE_QOS_CONNECTION_ATTR_MAX_BITRATE ? record.maximum : record.current;
        return CopyOut(value, &result, sizeof(result)) ? 0 : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    });
}

int APS5_VABI sceVoiceQoSGetLocalEndpoint(int32_t connectionId, int32_t* localId) {
    return Locked([&](QoS& qos) {
        if (!localId || connectionId < 0 || connectionId >= qos.endpointLimit) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const int savedErrno = errno;
        const GuestArena::HostWrite write(localId, sizeof(*localId));
        if (!write.Open() || !GuestWritable_nid_no_patch(localId, sizeof(*localId))) {
            errno = savedErrno;
            return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        }
        const auto it = qos.connections.find(connectionId);
        const std::int32_t value = it == qos.connections.end() ? -1 : it->second.local;
        const bool copied = GuestCopyOut_nid_no_patch(localId, &value, sizeof(value));
        errno = savedErrno;
        return copied ? 0 : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    });
}

int APS5_VABI sceVoiceQoSGetRemoteEndpoint(int32_t connectionId, int32_t* remoteId) {
    return Locked([&](QoS& qos) {
        if (!remoteId || connectionId < 0 || connectionId >= qos.endpointLimit) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        const int savedErrno = errno;
        const GuestArena::HostWrite write(remoteId, sizeof(*remoteId));
        if (!write.Open() || !GuestWritable_nid_no_patch(remoteId, sizeof(*remoteId))) {
            errno = savedErrno;
            return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        }
        const auto it = qos.connections.find(connectionId);
        const std::int32_t value = it == qos.connections.end() ? -1 : it->second.remote;
        const bool copied = GuestCopyOut_nid_no_patch(remoteId, &value, sizeof(value));
        errno = savedErrno;
        return copied ? 0 : SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    });
}

int APS5_VABI sceVoiceQoSEnableChat() {
    return Locked([](QoS&) -> int {
        throw std::runtime_error("sceVoiceQoSEnableChat: audio chat backend is not implemented");
    });
}

int APS5_VABI sceVoiceQoSDisableChat() {
    return Locked([](QoS&) -> int {
        throw std::runtime_error("sceVoiceQoSDisableChat: audio chat backend is not implemented");
    });
}

int APS5_VABI sceVoiceQoSGetStatus(int32_t connectionId, int32_t statusId, void* value, int32_t size) {
    return Locked([&](QoS& qos) -> int {
        if (connectionId < 0 || !value || connectionId >= qos.endpointLimit) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        if (statusId < 0 || statusId > 4 || size != 4) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        if (qos.packetTraffic) throw std::runtime_error("sceVoiceQoSGetStatus: status after voice packet traffic is not implemented");
        if (statusId == 2) return CopyStatus(value, ConnectionBitrateCap(qos));
        if (statusId == 3) return CopyStatus(value, 0.0f);
        const auto found = qos.connections.find(connectionId);
        if (found == qos.connections.end()) throw std::runtime_error("sceVoiceQoSGetStatus: remote endpoint status of an unconnected connection is not implemented");
        if (statusId != 4) return CopyStatus(value, ConnectionBitrateCap(qos));
        const float elapsed = MonotonicSeconds() - found->second.resetTime;
        return CopyStatus(value, std::int32_t{elapsed > VOICE_QOS_STATUS_TIMEOUT_SECONDS ? 0 : 1});
    });
}

int APS5_VABI sceVoiceQoSDebugGetStatus(std::int32_t connectionId, std::int32_t statusId, void* value, std::int32_t size) {
    return Locked([&](QoS& qos) -> int {
        if (connectionId < 0 || !value || connectionId >= qos.endpointLimit) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        if (statusId < 200 || statusId > 210 || size != 4) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
        if (qos.packetTraffic) throw std::runtime_error("sceVoiceQoSDebugGetStatus: status after voice packet traffic is not implemented");
        if (statusId == 210) return CopyStatus(value, 1.0f);
        return CopyStatus(value, std::int32_t{0});
    });
}

int APS5_VABI sceVoiceQoSInitHQ(void* memory, std::uint32_t memorySize, std::int32_t appType) {
    return sceVoiceQoSInit(memory, memorySize, appType);
}

int APS5_VABI sceVoiceQoSSetMode() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceVoiceQoSTerminate() {
    return sceVoiceQoSEnd();
}

}

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

struct LocalEndpoint {
    int32_t user_id;
    int32_t device_in_id;
    int32_t device_out_id;
    std::map<int32_t, std::vector<std::uint8_t>> attributes;
};

struct RemoteEndpoint {
    std::map<int32_t, std::vector<std::uint8_t>> attributes;
};

struct Connection {
    int32_t local_id;
    int32_t remote_id;
};

constexpr std::int32_t APP_TYPE_GAME = 0x20000000;
constexpr std::int32_t APP_TYPE_10000000 = 0x10000000;
constexpr std::uint32_t MEMORY_SIZE = 0x40000;
constexpr int ERROR_ARGUMENT_INVALID = static_cast<int>(0x804E0902);
constexpr int ERROR_INITIALIZED = static_cast<int>(0x804E0905);

std::mutex g_mutex;
bool g_initialized = false;
void* g_mem_block = nullptr;
std::uint32_t g_mem_size = 0;
int32_t g_app_type = 0;
int32_t g_next_local_id = 1;
int32_t g_next_remote_id = 1;
int32_t g_next_connection_id = 1;
std::map<int32_t, LocalEndpoint> g_local_endpoints;
std::map<int32_t, RemoteEndpoint> g_remote_endpoints;
std::map<int32_t, Connection> g_connections;

void RequireInitialized(const char* function) {
    if (!g_initialized) throw std::runtime_error(std::string(function) + ": not initialized");
}

std::map<int32_t, LocalEndpoint>::iterator FindLocal(int32_t local_id, const char* function) {
    const auto it = g_local_endpoints.find(local_id);
    if (it == g_local_endpoints.end()) throw std::invalid_argument(std::string(function) + ": unknown local endpoint");
    return it;
}

std::map<int32_t, RemoteEndpoint>::iterator FindRemote(int32_t remote_id, const char* function) {
    const auto it = g_remote_endpoints.find(remote_id);
    if (it == g_remote_endpoints.end()) throw std::invalid_argument(std::string(function) + ": unknown remote endpoint");
    return it;
}

std::map<int32_t, Connection>::iterator FindConnection(int32_t connection_id, const char* function) {
    const auto it = g_connections.find(connection_id);
    if (it == g_connections.end()) throw std::invalid_argument(std::string(function) + ": unknown connection");
    return it;
}

void RemoveConnectionsForLocal(int32_t local_id) {
    for (auto it = g_connections.begin(); it != g_connections.end();) {
        it = it->second.local_id == local_id ? g_connections.erase(it) : std::next(it);
    }
}

void RemoveConnectionsForRemote(int32_t remote_id) {
    for (auto it = g_connections.begin(); it != g_connections.end();) {
        it = it->second.remote_id == remote_id ? g_connections.erase(it) : std::next(it);
    }
}

void StoreAttribute(std::map<int32_t, std::vector<std::uint8_t>>& attributes, int32_t attribute_id, const void* value, int32_t size) {
    if (size < 0) throw std::invalid_argument("sceVoiceQoS: negative attribute size");
    if (size > 0 && !value) throw std::invalid_argument("sceVoiceQoS: null attribute value");
    attributes[attribute_id] = value && size > 0
        ? std::vector<std::uint8_t>(static_cast<const std::uint8_t*>(value), static_cast<const std::uint8_t*>(value) + size)
        : std::vector<std::uint8_t>{};
}

void LoadAttribute(const std::map<int32_t, std::vector<std::uint8_t>>& attributes, int32_t attribute_id, void* value, int32_t size, const char* function) {
    if (size < 0) throw std::invalid_argument(std::string(function) + ": negative attribute size");
    if (size > 0 && !value) throw std::invalid_argument(std::string(function) + ": null attribute value");
    const auto it = attributes.find(attribute_id);
    if (it == attributes.end()) throw std::invalid_argument(std::string(function) + ": unknown attribute");
    if (size > 0) std::memcpy(value, it->second.data(), static_cast<std::size_t>(size));
}

}

extern "C" {

int APS5_VABI sceVoiceQoSInit(void* pMemBlock, std::uint32_t memSize, int32_t appType) {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return ERROR_INITIALIZED;
    if (!pMemBlock || memSize != MEMORY_SIZE || (appType != APP_TYPE_10000000 && appType != APP_TYPE_GAME)) return ERROR_ARGUMENT_INVALID;
    g_initialized = true;
    g_mem_block = pMemBlock;
    g_mem_size = memSize;
    g_app_type = appType;
    g_next_local_id = 1;
    g_next_remote_id = 1;
    g_next_connection_id = 1;
    g_local_endpoints.clear();
    g_remote_endpoints.clear();
    g_connections.clear();
    return 0;
}

int APS5_VABI sceVoiceQoSEnd(void) {
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSEnd");
    g_local_endpoints.clear();
    g_remote_endpoints.clear();
    g_connections.clear();
    g_mem_block = nullptr;
    g_mem_size = 0;
    g_app_type = 0;
    g_initialized = false;
    return 0;
}

int APS5_VABI sceVoiceQoSCreateLocalEndpoint(int32_t* pLocalId, int32_t userId, int32_t deviceInId, int32_t deviceOutId) {
    if (!pLocalId) throw std::invalid_argument("sceVoiceQoSCreateLocalEndpoint: invalid argument");
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSCreateLocalEndpoint");
    const int32_t local_id = g_next_local_id++;
    g_local_endpoints[local_id] = LocalEndpoint{userId, deviceInId, deviceOutId, {}};
    *pLocalId = local_id;
    return 0;
}

int APS5_VABI sceVoiceQoSDeleteLocalEndpoint(int32_t localId) {
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSDeleteLocalEndpoint");
    FindLocal(localId, "sceVoiceQoSDeleteLocalEndpoint");
    g_local_endpoints.erase(localId);
    RemoveConnectionsForLocal(localId);
    return 0;
}

int APS5_VABI sceVoiceQoSCreateRemoteEndpoint(int32_t* pRemoteId) {
    if (!pRemoteId) throw std::invalid_argument("sceVoiceQoSCreateRemoteEndpoint: invalid argument");
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSCreateRemoteEndpoint");
    const int32_t remote_id = g_next_remote_id++;
    g_remote_endpoints[remote_id] = RemoteEndpoint{};
    *pRemoteId = remote_id;
    return 0;
}

int APS5_VABI sceVoiceQoSDeleteRemoteEndpoint(int32_t remoteId) {
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSDeleteRemoteEndpoint");
    FindRemote(remoteId, "sceVoiceQoSDeleteRemoteEndpoint");
    g_remote_endpoints.erase(remoteId);
    RemoveConnectionsForRemote(remoteId);
    return 0;
}

int APS5_VABI sceVoiceQoSConnect(int32_t* pConnectionId, int32_t localId, int32_t remoteId) {
    if (!pConnectionId) throw std::invalid_argument("sceVoiceQoSConnect: invalid argument");
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSConnect");
    FindLocal(localId, "sceVoiceQoSConnect");
    FindRemote(remoteId, "sceVoiceQoSConnect");
    const int32_t connection_id = g_next_connection_id++;
    g_connections[connection_id] = Connection{localId, remoteId};
    *pConnectionId = connection_id;
    return 0;
}

int APS5_VABI sceVoiceQoSDisconnect(int32_t connectionId) {
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSDisconnect");
    FindConnection(connectionId, "sceVoiceQoSDisconnect");
    g_connections.erase(connectionId);
    return 0;
}

int APS5_VABI sceVoiceQoSSetLocalEndpointAttribute(int32_t localId, int32_t attributeId, const void* pAttributeValue, int32_t attributeSize) {
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSSetLocalEndpointAttribute");
    StoreAttribute(FindLocal(localId, "sceVoiceQoSSetLocalEndpointAttribute")->second.attributes, attributeId, pAttributeValue, attributeSize);
    return 0;
}

int APS5_VABI sceVoiceQoSSetRemoteEndpointAttribute(int32_t remoteId, int32_t attributeId, const void* pAttributeValue, int32_t attributeSize) {
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSSetRemoteEndpointAttribute");
    StoreAttribute(FindRemote(remoteId, "sceVoiceQoSSetRemoteEndpointAttribute")->second.attributes, attributeId, pAttributeValue, attributeSize);
    return 0;
}

int APS5_VABI sceVoiceQoSGetLocalEndpointAttribute(int32_t localId, int32_t attributeId, void* pAttributeValue, int32_t attributeSize) {
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSGetLocalEndpointAttribute");
    LoadAttribute(FindLocal(localId, "sceVoiceQoSGetLocalEndpointAttribute")->second.attributes, attributeId, pAttributeValue, attributeSize, "sceVoiceQoSGetLocalEndpointAttribute");
    return 0;
}

int APS5_VABI sceVoiceQoSReadPacket(int32_t connectionId, void* pData, std::uint32_t* pSize) {
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSReadPacket");
    FindConnection(connectionId, "sceVoiceQoSReadPacket");
    if (!pSize) throw std::invalid_argument("sceVoiceQoSReadPacket: invalid argument");
    if (*pSize > 0 && !pData) throw std::invalid_argument("sceVoiceQoSReadPacket: invalid argument");
    *pSize = 0;
    return 0;
}

int APS5_VABI sceVoiceQoSWritePacket(int32_t connectionId, const void* pData, std::uint32_t* pSize) {
    std::lock_guard lock(g_mutex);
    RequireInitialized("sceVoiceQoSWritePacket");
    FindConnection(connectionId, "sceVoiceQoSWritePacket");
    if (!pData || !pSize) throw std::invalid_argument("sceVoiceQoSWritePacket: invalid argument");
    return 0;
}

}
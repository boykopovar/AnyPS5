#include <cstdint>
#include <cstddef>
#include <mutex>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr std::int32_t VOICE_QOS_APP_TYPE_GAME = 0x20000000;
constexpr std::int32_t VOICE_QOS_APP_TYPE_10000000 = 0x10000000;
constexpr std::uint32_t VOICE_QOS_MEMORY_SIZE = 0x40000;
constexpr int SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID = static_cast<int>(0x804E0902);
constexpr int SCE_VOICE_ERROR_LIBVOICEQOS_INITIALIZED = static_cast<int>(0x804E0905);

std::mutex g_mutex;
bool g_initialized = false;

}

extern "C" {

int APS5_VABI sceVoiceQoSInit(void* mem_block, uint32_t mem_size, int32_t app_type) {
    std::lock_guard lock(g_mutex);
    if (g_initialized) return SCE_VOICE_ERROR_LIBVOICEQOS_INITIALIZED;
    if (!mem_block || mem_size != VOICE_QOS_MEMORY_SIZE || (app_type != VOICE_QOS_APP_TYPE_GAME && app_type != VOICE_QOS_APP_TYPE_10000000)) return SCE_VOICE_ERROR_LIBVOICEQOS_ARGUMENT_INVALID;
    g_initialized = true;
    return 0;
}

int APS5_VABI sceVoiceQoSEnd() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSCreateLocalEndpoint() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSDeleteLocalEndpoint() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSCreateRemoteEndpoint() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSDeleteRemoteEndpoint() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSConnect() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSDisconnect() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSSetLocalEndpointAttribute() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSSetRemoteEndpointAttribute() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSReadPacket() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSWritePacket() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceVoiceQoSGetLocalEndpointAttribute() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

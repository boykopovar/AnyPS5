extern "C" {
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceNetCtl/include/InventoryExports.hpp"
#include "prx/libSceNetCtl/include/NetCtl.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <net/route.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#ifdef __linux__
#include <netpacket/packet.h>
#endif
#ifdef __APPLE__
#include <net/if_dl.h>
#include <sys/sysctl.h>
#endif
#endif
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// Network-control state reflects host interface addressing, not PSN sign-in status.

static constexpr int SCE_NET_CTL_ERROR_NOT_INITIALIZED = static_cast<int>(0x80412101);
static constexpr int SCE_NET_CTL_ERROR_ID_NOT_FOUND = static_cast<int>(0x80412104);
static constexpr int SCE_NET_CTL_ERROR_INVALID_SIZE = static_cast<int>(0x80412111);
static constexpr int SCE_NET_CTL_ERROR_CALLBACK_MAX = static_cast<int>(0x80412103);
static constexpr int SCE_NET_CTL_ERROR_INVALID_ID = static_cast<int>(0x80412105);
static constexpr int SCE_NET_CTL_ERROR_INVALID_CODE = static_cast<int>(0x80412106);
static constexpr int SCE_NET_CTL_ERROR_INVALID_ADDR = static_cast<int>(0x80412107);
static constexpr int SCE_NET_CTL_ERROR_NOT_CONNECTED = static_cast<int>(0x80412108);
static constexpr int SCE_NET_CTL_ERROR_RESULT_EVENT_REFUSED = static_cast<int>(0x8041210F);
static constexpr int NET_CTL_STATE_DISCONNECTED = 0;
static constexpr int NET_CTL_STATE_IPOBTAINED = 3;
static constexpr int NET_CTL_EVENT_DISCONNECTED = 1;
static constexpr int NET_CTL_EVENT_DISCONNECT_REQ_FINISHED = 2;
static constexpr int NET_CTL_EVENT_IPOBTAINED = 3;
static constexpr int NET_CTL_STUN_STATUS_FAILED = 1;
static constexpr int NET_CTL_STUN_STATUS_OK = 2;
static constexpr int NET_CTL_NAT_TYPE_NONE = 0;
static constexpr int NET_CTL_NAT_TYPE_1 = 1;
static constexpr int NET_CTL_NAT_TYPE_2 = 2;
static constexpr int NET_CTL_NAT_TYPE_3 = 3;
static constexpr std::uint16_t STUN_BINDING_REQUEST = 0x0001;
static constexpr std::uint16_t STUN_BINDING_RESPONSE = 0x0101;
static constexpr std::uint32_t STUN_MAGIC_COOKIE = 0x2112A442u;
static constexpr std::uint16_t STUN_ATTR_MAPPED_ADDRESS = 0x0001;
static constexpr std::uint16_t STUN_ATTR_CHANGED_ADDRESS = 0x0005;
static constexpr std::uint16_t STUN_ATTR_XOR_MAPPED_ADDRESS = 0x0020;
static constexpr std::uint16_t STUN_ATTR_XOR_MAPPED_ADDRESS_LEGACY = 0x8020;
static constexpr std::uint16_t STUN_ATTR_OTHER_ADDRESS = 0x802c;
static constexpr std::size_t STUN_HEADER_SIZE = 20;
static constexpr std::size_t STUN_TRANSACTION_ID_SIZE = 12;
static constexpr int STUN_DEFAULT_TIMEOUT_MS = 300;
static constexpr int STUN_TOTAL_BUDGET_MS = 1000;
static constexpr int STUN_CACHE_TTL_MS = 60000;
static constexpr int NET_CTL_INFO_DEVICE = 1;
static constexpr int NET_CTL_INFO_ETHER_ADDR = 2;
static constexpr int NET_CTL_INFO_MTU = 3;
static constexpr int NET_CTL_INFO_LINK = 4;
static constexpr int NET_CTL_INFO_BSSID = 5;
static constexpr int NET_CTL_INFO_SSID = 6;
static constexpr int NET_CTL_INFO_WIFI_SECURITY = 7;
static constexpr int NET_CTL_INFO_RSSI_DBM = 8;
static constexpr int NET_CTL_INFO_RSSI_PERCENTAGE = 9;
static constexpr int NET_CTL_INFO_CHANNEL = 10;
static constexpr int NET_CTL_INFO_IP_CONFIG = 11;
static constexpr int NET_CTL_INFO_DHCP_HOSTNAME = 12;
static constexpr int NET_CTL_INFO_PPPOE_AUTH_NAME = 13;
static constexpr int NET_CTL_INFO_IP_ADDRESS = 14;
static constexpr int NET_CTL_INFO_NETMASK = 15;
static constexpr int NET_CTL_INFO_DEFAULT_ROUTE = 16;
static constexpr int NET_CTL_INFO_PRIMARY_DNS = 17;
static constexpr int NET_CTL_INFO_SECONDARY_DNS = 18;
static constexpr int NET_CTL_INFO_HTTP_PROXY_CONFIG = 19;
static constexpr int NET_CTL_INFO_HTTP_PROXY_SERVER = 20;
static constexpr int NET_CTL_INFO_HTTP_PROXY_PORT = 21;
static constexpr int NET_CTL_INFO_V6_IP_ADDRESS = 1;
static constexpr int NET_CTL_INFO_V6_LINK_LOCAL = 2;
static constexpr int NET_CTL_INFO_V6_SECONDARY_DNS = 5;
static constexpr std::uint32_t NET_CTL_DEVICE_WIRED = 0;
static constexpr std::uint32_t NET_CTL_LINK_CONNECTED = 1;
static constexpr std::uint32_t NET_CTL_IP_DHCP = 0;
static constexpr std::uint32_t NET_CTL_HTTP_PROXY_OFF = 0;
static constexpr std::uint8_t NET_CTL_ETHER_ADDR[6] = {0x02, 0x50, 0x53, 0x35, 0x00, 0x01};
static constexpr std::size_t NET_CTL_IPV4_ADDR_STR_LEN = 16;
static constexpr int MAX_CALLBACKS = 8;

struct CallbackSlot {
    NetCtlCallback func = nullptr;
    void* arg = nullptr;
    int lastEvent = 0;
};

static std::atomic_bool g_initialized{true};
static std::mutex g_callbackLock;
static CallbackSlot g_callbacks[MAX_CALLBACKS] = {};
static CallbackSlot g_callbacksV6[MAX_CALLBACKS] = {};
static CallbackSlot g_callbacksNpToolkit[MAX_CALLBACKS] = {};
static CallbackSlot g_callbacksForLib[MAX_CALLBACKS] = {};

static int CheckCallbacks(CallbackSlot (&callbacks)[MAX_CALLBACKS], bool includeV6 = false) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    const bool primary = HasCallback(callbacks);
    const bool ipv6 = includeV6 && HasCallback(g_callbacksV6);
    if (!primary && !ipv6) return 0;
    const int primaryEvent = primary ? (HostHasAddress(AF_UNSPEC) ? NET_CTL_EVENT_IPOBTAINED : NET_CTL_EVENT_DISCONNECTED) : 0;
    const int ipv6Event = ipv6 ? (HostHasAddress(AF_INET6) ? NET_CTL_EVENT_IPOBTAINED : NET_CTL_EVENT_DISCONNECTED) : 0;
    struct Delivery {
        CallbackSlot callback;
        int event;
    };
    Delivery due[2 * MAX_CALLBACKS];
    int count = 0;
    {
        std::lock_guard lock(g_callbackLock);
        const auto collect = [&](CallbackSlot (&slots)[MAX_CALLBACKS], int event) {
            if (event == 0) return;
            for (auto& slot : slots) {
                if (!slot.func || slot.lastEvent == event) continue;
                slot.lastEvent = event;
                due[count++] = {slot, event};
            }
        };
        collect(callbacks, primaryEvent);
        if (includeV6) collect(g_callbacksV6, ipv6Event);
    }
    for (int index = 0; index < count; ++index)
        due[index].callback.func(due[index].event, due[index].callback.arg);
    return 0;
}


int APS5_VABI sceNetCtlCheckCallback(void) {
    return CheckCallbacks(g_callbacks, true);
}

int APS5_VABI sceNetCtlGetIfStat(NetCtlIfStat* stat) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!stat) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (!HostHasAddress(AF_UNSPEC)) return SCE_NET_CTL_ERROR_NOT_CONNECTED;
    NetCtlIfStat localStat{};
    HostTraffic(localStat.tx_bytes, localStat.rx_bytes);
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(stat, &localStat, sizeof(localStat));
    errno = savedErrno;
    if (!copied) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    return 0;
}

int APS5_VABI sceNetCtlGetInfo(int code, NetCtlInfo* info) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!info) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (code < NET_CTL_INFO_DEVICE || code > NET_CTL_INFO_HTTP_PROXY_PORT) return SCE_NET_CTL_ERROR_INVALID_CODE;
    HostIpv4Interface host;
    if (!FindHostIpv4Interface(host)) return SCE_NET_CTL_ERROR_NOT_CONNECTED;
    bool copied = false;
    switch (code) {
        case NET_CTL_INFO_DEVICE: copied = WriteInfoWord(info, NET_CTL_DEVICE_WIRED); break;
        case NET_CTL_INFO_ETHER_ADDR: copied = GuestCopyOut_nid_no_patch(info->opaque, NET_CTL_ETHER_ADDR, sizeof(NET_CTL_ETHER_ADDR)); break;
        case NET_CTL_INFO_MTU: copied = WriteInfoWord(info, host.mtu); break;
        case NET_CTL_INFO_LINK: copied = WriteInfoWord(info, NET_CTL_LINK_CONNECTED); break;
        case NET_CTL_INFO_IP_CONFIG: copied = WriteInfoWord(info, NET_CTL_IP_DHCP); break;
        case NET_CTL_INFO_IP_ADDRESS: copied = WriteInfoAddress(info, host.address); break;
        case NET_CTL_INFO_NETMASK: copied = WriteInfoAddress(info, host.netmask); break;
        case NET_CTL_INFO_DEFAULT_ROUTE: copied = WriteInfoAddress(info, host.gateway); break;
        case NET_CTL_INFO_PRIMARY_DNS: copied = WriteInfoAddress(info, host.dns.size() > 0 ? host.dns[0] : in_addr{}); break;
        case NET_CTL_INFO_SECONDARY_DNS: copied = WriteInfoAddress(info, host.dns.size() > 1 ? host.dns[1] : in_addr{}); break;
        case NET_CTL_INFO_HTTP_PROXY_CONFIG: copied = WriteInfoWord(info, NET_CTL_HTTP_PROXY_OFF); break;
        case NET_CTL_INFO_BSSID:
        case NET_CTL_INFO_SSID:
        case NET_CTL_INFO_DHCP_HOSTNAME:
        case NET_CTL_INFO_PPPOE_AUTH_NAME:
        case NET_CTL_INFO_HTTP_PROXY_SERVER: {
            const NetCtlInfo zero{};
            copied = GuestCopyOut_nid_no_patch(info->opaque, zero.opaque, sizeof(info->opaque));
            break;
        }
        case NET_CTL_INFO_WIFI_SECURITY:
        case NET_CTL_INFO_RSSI_DBM:
        case NET_CTL_INFO_RSSI_PERCENTAGE:
        case NET_CTL_INFO_CHANNEL:
        case NET_CTL_INFO_HTTP_PROXY_PORT: copied = WriteInfoWord(info, 0); break;
        default: throw std::runtime_error("sceNetCtlGetInfo: info code " + std::to_string(code) + " is not implemented");
    }
    if (!copied) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    return 0;
}

int APS5_VABI sceNetCtlGetNatInfo(NetCtlNatInfo* info) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!info) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    std::uint32_t size = 0;
    const int savedErrno = errno;
    const bool copiedSize = GuestCopyIn_nid_no_patch(&size, info, sizeof(size));
    errno = savedErrno;
    if (!copiedSize) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (size != sizeof(NetCtlNatInfo)) return SCE_NET_CTL_ERROR_INVALID_SIZE;
    HostIpv4Interface host;
    if (!FindHostIpv4Interface(host)) return SCE_NET_CTL_ERROR_NOT_CONNECTED;
    auto& config = GetStunConfig();
    std::uint64_t generation = 0;
    const auto now = std::chrono::steady_clock::now();
    NetCtlNatInfo cached{};
    bool hasCached = false;
    {
        std::lock_guard guard(config.lock);
        generation = config.configGeneration;
        if (config.hasCachedResult && config.cachedConfigGeneration == generation &&
            config.cachedLocalAddr == host.address.s_addr && now < config.cachedUntil) {
            cached = config.cachedResult;
            hasCached = true;
        }
    }
    if (hasCached) {
        const int copyErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(info, &cached, sizeof(cached));
        errno = copyErrno;
        return copied ? 0 : SCE_NET_CTL_ERROR_INVALID_ADDR;
    }
    const NetCtlNatInfo discovered = DiscoverNatInfo(host.address);
    if (discovered.stun_status == NET_CTL_STUN_STATUS_OK) {
        std::lock_guard guard(config.lock);
        if (config.configGeneration == generation) {
            config.cachedConfigGeneration = generation;
            config.cachedLocalAddr = host.address.s_addr;
            config.cachedUntil = std::chrono::steady_clock::now() + std::chrono::milliseconds(STUN_CACHE_TTL_MS);
            config.cachedResult = discovered;
            config.hasCachedResult = true;
        }
    }
    const int copyErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(info, &discovered, sizeof(discovered));
    errno = copyErrno;
    return copied ? 0 : SCE_NET_CTL_ERROR_INVALID_ADDR;
}

int APS5_VABI sceNetCtlGetResult(int event, int* result) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!result) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (event == NET_CTL_EVENT_DISCONNECT_REQ_FINISHED || event == NET_CTL_EVENT_IPOBTAINED) return SCE_NET_CTL_ERROR_RESULT_EVENT_REFUSED;
    const int val = 0;
    if (!GuestCopyOut_nid_no_patch(result, &val, sizeof(val))) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (event != NET_CTL_EVENT_DISCONNECTED) {
        throw std::runtime_error("sceNetCtlGetResult: event type " + std::to_string(event) + " is not implemented");
    }
    return 0;
}

int APS5_VABI sceNetCtlGetState(int* state) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!state) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    const int val = HostHasAddress(AF_UNSPEC) ? NET_CTL_STATE_IPOBTAINED : NET_CTL_STATE_DISCONNECTED;
    if (!GuestCopyOut_nid_no_patch(state, &val, sizeof(val))) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    return 0;
}

int APS5_VABI sceNetCtlGetStateV6(int* state) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!state) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    const int val = HostHasAddress(AF_INET6) ? NET_CTL_STATE_IPOBTAINED : NET_CTL_STATE_DISCONNECTED;
    if (!GuestCopyOut_nid_no_patch(state, &val, sizeof(val))) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    return 0;
}

int APS5_VABI sceNetCtlInit(void) {
    g_initialized.store(true);
    return 0;
}

int APS5_VABI sceNetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
    return RegisterCallback(g_callbacks, func, arg, cid);
}

int APS5_VABI sceNetCtlRegisterCallbackV6(NetCtlCallback func, void* arg, int* cid) {
    return RegisterCallback(g_callbacksV6, func, arg, cid);
}

int APS5_VABI sceNetCtlGetInfoV6(std::uint32_t code, NetCtlInfoV6* info) {
    if (!g_initialized.load()) return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
    if (!info) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (code < NET_CTL_INFO_V6_IP_ADDRESS || code > NET_CTL_INFO_V6_SECONDARY_DNS) return SCE_NET_CTL_ERROR_INVALID_CODE;
    if (!HostHasAddress(AF_INET6)) return SCE_NET_CTL_ERROR_NOT_CONNECTED;
    if (code == NET_CTL_INFO_V6_IP_ADDRESS || code == NET_CTL_INFO_V6_LINK_LOCAL) {
        const HostIpv6Selection selection = QueryHostIpv6Selection();
        HostIpv6Address chosen;
        if (code == NET_CTL_INFO_V6_IP_ADDRESS) {
            chosen = selection.global;
        } else if (selection.linkLocalOfGlobal.valid) {
            chosen = selection.linkLocalOfGlobal;
        } else {
            chosen = selection.linkLocal;
        }
        if (!chosen.valid) return SCE_NET_CTL_ERROR_NOT_CONNECTED;
        if (!WriteInfoV6Address(info, chosen)) return SCE_NET_CTL_ERROR_INVALID_ADDR;
        return 0;
    }
    throw std::runtime_error("sceNetCtlGetInfoV6: info code " + std::to_string(code) + " is not implemented");
}

void APS5_VABI sceNetCtlTerm(void) {
}

int APS5_VABI sceNetCtlUnregisterCallback(int cid) {
    return UnregisterCallback(g_callbacks, cid);
}

int APS5_VABI sceNetCtlUnregisterCallbackV6(int cid) {
    return UnregisterCallback(g_callbacksV6, cid);
}

int APS5_VABI sceNetCtlRegisterCallbackForNpToolkit(NetCtlCallback callback, void* argument, int* callbackId) {
    return RegisterCallback(g_callbacksNpToolkit, callback, argument, callbackId);
}

int APS5_VABI sceNetCtlRegisterCallbackIpcInt(NetCtlCallback callback, void* argument, int* callbackId) {
    return sceNetCtlRegisterCallback(callback, argument, callbackId);
}

int APS5_VABI sceNetCtlRegisterCallbackForLibIpcInt(NetCtlCallback callback, void* argument, int* callbackId) {
    return RegisterCallback(g_callbacksForLib, callback, argument, callbackId);
}

int APS5_VABI sceNetCtlUnregisterCallbackForNpToolkit(int callbackId) {
    return UnregisterCallback(g_callbacksNpToolkit, callbackId);
}

int APS5_VABI sceNetCtlUnregisterCallbackIpcInt(int callbackId) {
    return sceNetCtlUnregisterCallback(callbackId);
}

int APS5_VABI sceNetCtlUnregisterCallbackForLibIpcInt(int callbackId) {
    return UnregisterCallback(g_callbacksForLib, callbackId);
}

int APS5_VABI sceNetCtlCheckCallbackForNpToolkit(void) {
    return CheckCallbacks(g_callbacksNpToolkit);
}

int APS5_VABI sceNetCtlCheckCallbackForLibIpcInt(void) {
    return CheckCallbacks(g_callbacksForLib);
}

int APS5_VABI sceNetCtlClearEventIpcInt(int event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlClearEventForNpToolkit(int event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlClearEventForLibIpcInt(int event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetResultIpcInt(int event, int* result) {
    return sceNetCtlGetResult(event, result);
}

int APS5_VABI sceNetCtlGetStateIpcInt(int* state) {
    return sceNetCtlGetState(state);
}

int APS5_VABI sceNetCtlGetState2IpcInt(int* state) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetInfoIpcInt(std::uint32_t code, NetCtlInfo* info) {
    return sceNetCtlGetInfo(static_cast<int>(code), info);
}

int APS5_VABI sceNetCtlGetEtherLinkMode(int* mode) {
    const int val = 0;
    if (!GuestCopyOut_nid_no_patch(mode, &val, sizeof(val))) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetWifiType(int* type) {
    const int val = 0;
    if (!GuestCopyOut_nid_no_patch(type, &val, sizeof(val))) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlConnectIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlConnectWithRetryIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlConnectConfIpcInt(const NetCtlConnectionConfiguration* configuration) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlDisconnectIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlScanIpcInt(const char* const* ssids, std::uint32_t count, const NetCtlScanOptions* options) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoBssidIpcInt(NetCtlScanBssidList* bssids, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoByBssidIpcInt(const std::uint8_t* bssid, NetCtlScanInfo* info, void* buffer, std::size_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoForSsidScanIpcInt(NetCtlScanInfo* info, void* buffer, std::size_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoBssidForSsidListScanIpcInt(NetCtlScanBssidList* bssids, std::uint32_t capacity) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetScanInfoForSsidListScanIpcInt(const std::uint8_t* bssid, NetCtlScanInfo* info, void* buffer, std::size_t* size) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlSetErrorNotificationEnabledIpcInt(int enabled) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlRegisterCallbackV6IpcInt(NetCtlCallback callback, void* argument, int* callbackId) {
    return sceNetCtlRegisterCallbackV6(callback, argument, callbackId);
}

int APS5_VABI sceNetCtlUnregisterCallbackV6IpcInt(int callbackId) {
    return sceNetCtlUnregisterCallbackV6(callbackId);
}

int APS5_VABI sceNetCtlGetResultV6(int event_type, int* error_code) {
    if (!error_code) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    if (event_type < NET_CTL_EVENT_DISCONNECTED || event_type > NET_CTL_EVENT_IPOBTAINED) {
        throw std::runtime_error("sceNetCtlGetResultV6: event type " + std::to_string(event_type) + " is not implemented");
    }
    const int val = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(error_code, &val, sizeof(val));
    errno = savedErrno;
    if (!copied) return SCE_NET_CTL_ERROR_INVALID_ADDR;
    return 0;
}

int APS5_VABI sceNetCtlGetResultV6IpcInt(int event, int* result) {
    return sceNetCtlGetResultV6(event, result);
}

int APS5_VABI sceNetCtlGetStateV6IpcInt(int* state) {
    return sceNetCtlGetStateV6(state);
}

int APS5_VABI sceNetCtlGetInfoV6IpcInt(std::uint32_t code, NetCtlInfoV6* info) {
    return sceNetCtlGetInfoV6(code, info);
}

int APS5_VABI sceNetCtlGetNatInfoIpcInt(NetCtlNatInfoInternal* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlSetStunWithPaddingFlagIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlUnsetStunWithPaddingFlagIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlIsBandwidthManagementEnabledIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlDisableBandwidthManagementIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlEnableBandwidthManagementIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

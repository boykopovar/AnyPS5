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

int APS5_VABI sceNetCtlGetBandwidthInfoIpcInt(NetCtlBandwidthInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetNetEvConfigInfoIpcInt(NetCtlNetEvConfiguration* configuration) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetConnectionElapsedTimeIpcInt(std::uint64_t* elapsedTime) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApRpCheckCallback(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpClearEvent(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpRegisterCallback(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpUnregisterCallback(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpGetResult(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpGetState(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpGetInfo(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpStart(void) {
    return 0;
}

int APS5_VABI sceNetCtlApCpStart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApAppStartWithRetryPid(std::int32_t processId, const char* passcode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApAppStartWithRetry(const char* passcode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApRpStartWithRetry(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpStartConf(void) {
    return 0;
}

int APS5_VABI sceNetCtlApRpStop(void) {
    return 0;
}

int APS5_VABI sceNetCtlApCpStop(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApAppInitWpaKey(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApAppInitWpaKeyForQa(std::uint8_t mode) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlApRestart(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweCheckCallbackIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweClearEventIpcInt(int event) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweRegisterCallbackIpcInt(NetCtlCallback callback, void* argument, int* callbackId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweUnregisterCallbackIpcInt(int callbackId) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweGetInfoIpcInt(NetBweInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweStartInternetConnectionTestIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweFinishInternetConnectionTestIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweStartInternetConnectionTestBandwidthTestIpcInt(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetBweSetInternetConnectionTestResultIpcInt(std::int32_t firstResult, std::int32_t secondResult) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceNetCtlGetWifiInfoIpcInt(NetCtlWifiInfo* info) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI sceInetStatInit() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceInetStatTerm() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceNetConnSwitcherStop() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libSceNetCtlUnknown_09YMz3Vp5us_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("09YMz3Vp5us", libSceNetCtlUnknown_09YMz3Vp5us_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_1PGApDf96ao_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1PGApDf96ao", libSceNetCtlUnknown_1PGApDf96ao_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_1mrTXfVGn_plus_g_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1mrTXfVGn+g", libSceNetCtlUnknown_1mrTXfVGn_plus_g_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_2Ny2lzU3o9w_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("2Ny2lzU3o9w", libSceNetCtlUnknown_2Ny2lzU3o9w_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_3cNKxJXXYYk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("3cNKxJXXYYk", libSceNetCtlUnknown_3cNKxJXXYYk_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_AIu_minus_tRC_minus_mSM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("AIu-tRC-mSM", libSceNetCtlUnknown_AIu_minus_tRC_minus_mSM_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_BSERkV_minus_a9e8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("BSERkV-a9e8", libSceNetCtlUnknown_BSERkV_minus_a9e8_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_FwX_plus_Js8qXNA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("FwX+Js8qXNA", libSceNetCtlUnknown_FwX_plus_Js8qXNA_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_I14Npt_minus_awQg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("I14Npt-awQg", libSceNetCtlUnknown_I14Npt_minus_awQg_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_KUdWtCC6w_minus_0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KUdWtCC6w-0", libSceNetCtlUnknown_KUdWtCC6w_minus_0_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_QnQ4RBu546Y_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QnQ4RBu546Y", libSceNetCtlUnknown_QnQ4RBu546Y_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_e5_minus_Xj0wEzbI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("e5-Xj0wEzbI", libSceNetCtlUnknown_e5_minus_Xj0wEzbI_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_lScJhO3Rnpk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lScJhO3Rnpk", libSceNetCtlUnknown_lScJhO3Rnpk_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_mfWDyuk_minus_M0I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mfWDyuk-M0I", libSceNetCtlUnknown_mfWDyuk_minus_M0I_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_on7RqSRy8pU_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("on7RqSRy8pU", libSceNetCtlUnknown_on7RqSRy8pU_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_sngJX5nUjVM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("sngJX5nUjVM", libSceNetCtlUnknown_sngJX5nUjVM_nid_no_patch);

int APS5_VABI libSceNetCtlUnknown_wVXoAlD7Fsg_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wVXoAlD7Fsg", libSceNetCtlUnknown_wVXoAlD7Fsg_nid_no_patch);

}

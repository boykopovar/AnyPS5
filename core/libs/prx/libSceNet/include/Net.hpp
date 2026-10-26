#pragma once

#include "SceTypes.hpp"

struct NetRoutingRule {
    std::int32_t protocol;
    std::uint32_t firstAddress;
    std::uint16_t firstPort;
    std::uint16_t reserved;
    std::uint32_t secondAddress;
    std::uint16_t secondPort;
    std::uint16_t flags;
};

struct NetIovec {
    void* base;
    std::uint64_t length;
};

struct NetResolverRecord {
    std::uint32_t address;
    std::uint8_t address6[12];
    std::int32_t family;
    std::int32_t reserved[3];
};

struct NetWlanInformationElement;
struct NetWlanWakeParameter;
struct NetWlanScanParameter;
struct NetWlanScanResult;
struct NetWlanAdhocScanParameter;
struct NetWlanInfraScanParameter;
struct NetWlanJoinResult;
struct NetDumpFilterParameter;
struct NetWlanAdhocParameter;
struct NetWlanWakeInfo;
struct NetWlanPspWakeInfo;
struct NetWlanPspWakeParameter;
struct NetWlanApParameter;
struct NetWlanDeviceInfo;
struct NetWlanAntennaParameter;
struct NetWlanRateParameter;
struct NetWlanDeviceConfiguration;
struct NetWlanRssiInfo;
struct NetDhcpInfo;
struct NetAutoIpInfo;
struct NetAddress6Info;

struct NetDhcpOptionInfo {
    std::uint32_t option;
    std::uint32_t capacity;
    void* data;
    std::uint32_t size;
    std::uint32_t reserved;
};

struct NetSockInfo6 {
    char name[32];
    std::int32_t pid;
    std::int32_t s;
    std::int8_t socket_type;
    std::int8_t policy;
    std::int8_t priority;
    std::int8_t reserved8;
    std::int32_t recv_queue_length;
    std::int32_t send_queue_length;
    std::uint32_t local_adr;
    std::uint32_t remote_adr;
    std::uint16_t local_port;
    std::uint16_t remote_port;
    std::uint16_t local_vport;
    std::uint16_t remote_vport;
    std::int32_t state;
    std::int32_t flags;
    std::int32_t tx_bps;
    std::int32_t rx_bps;
    std::int32_t max_tx_bps;
    std::int32_t max_rx_bps;
    std::int32_t tx_vbps;
    std::int32_t rx_vbps;
    std::int32_t recv_buffer_size;
    std::int32_t send_buffer_size;
    std::uint8_t local_adr6[16];
    std::uint8_t remote_adr6[16];
    std::int32_t tx_drops;
    std::int32_t rx_drops;
    std::int32_t tx_wait;
    std::int32_t reserved[2];
};
struct NetStatisticsInfo;
struct NetInterfaceStats;
struct NetRouteInfo;
struct NetDebugParameter;
struct NetArpInfo;
struct NetAppMemoryInfo;
struct NetSystemMemoryInfo;
struct NetBandwidthParameter;
struct NetDataTraffic;
struct NetEmulationParameter;
struct NetResolverConnection;
struct NetThreadParameter {
    void (APS5_VABI *entry)(void*);
    void* arg;
    std::uint64_t reserved;
    std::uint32_t flags;
};
struct NetIpv4InterfaceAddress;
struct NetStatisticsInfoInternal;
struct NetInitializeParameter;
struct NetDhcpStartParameter;
struct NetAddress6StartParameter;

struct NetDhcpdStartParameter {
    std::uint32_t addresses[5];
    std::uint32_t maxClients;
};

struct NetPppoeStartParameter {
    std::uint32_t dnsAddresses[2];
    char userName[128];
    char password[128];
    std::int32_t mtu;
    std::uint32_t flags;
};

struct NetRoutingAddressParameter {
    std::uint32_t address;
    std::uint32_t mask;
};

struct NetRoutingServerParameter {
    std::int32_t count;
    std::uint32_t localAddresses[2];
    std::uint32_t remoteAddresses[2];
};

struct NetRoutingRuleParameter {
    std::int32_t count;
    const NetRoutingRule* rules;
};

struct NetInfoDumpParameter {
    std::uint32_t flags;
    std::uint32_t intervalSeconds;
    char path[64];
};

struct NetInterfaceInfo;
struct NetInterfaceStatistics;

struct NetResolverConnectOption {
    std::int32_t connectionTimeout;
    std::int32_t resolverTimeout;
    std::int32_t resolverRetryCount;
    std::uint32_t reserved;
};

struct NetSyncObject {
    void* mutex;
    void* condition;
    std::uint32_t signals;
    std::uint32_t reserved;
};

struct NetIpv6Address {
    std::uint8_t bytes[16];
};

struct NetDnsInfo {
    std::uint32_t primaryAddress;
    std::uint32_t secondaryAddress;
};

struct NetDns6Info {
    NetIpv6Address primaryAddress;
    NetIpv6Address secondaryAddress;
};

struct NetIpv6InterfaceAddress {
    NetIpv6Address address;
    std::uint8_t prefixLength;
    std::uint8_t reserved[3];
};

struct NetMemoryPoolStats {
    std::size_t pool_size;
    std::size_t max_inuse_size;
    std::size_t current_inuse_size;
};

struct NetMsghdr {
    void* name;
    std::uint32_t name_length;
    NetIovec* iov;
    int iov_length;
    void* control;
    std::uint32_t control_length;
    int flags;
};

struct NetResolverInfo {
    NetResolverRecord records[10];
    std::int32_t count;
    std::int32_t count4;
    std::int32_t reserved[14];
};

using NetAddressCallback = void (APS5_VABI *)(void* userArg, std::int32_t event, std::int32_t error);

using NetEventCallback = std::uint64_t (APS5_VABI*)(std::uint32_t interfaceId, std::uint64_t events, void* userData, void* waitData);

extern "C" {

int* APS5_VABI sceNetErrnoLoc(void);
int APS5_VABI sceNetShowNetstat(void);
int APS5_VABI sceNetInit_nid_postfix(void);
int APS5_VABI sceNetTerm(void);
int APS5_VABI sceNetPoolCreate(const char* name, int size, int flags);
int APS5_VABI sceNetPoolDestroy(int memid);
int APS5_VABI sceNetSocket(const char* name, int family, int type, int protocol);
int APS5_VABI sceNetSocketClose(int s);
int APS5_VABI sceNetSocketAbort(int s, int flags);
int APS5_VABI sceNetBind_nid_postfix(int s, const void* addr, uint32_t addrlen);
int APS5_VABI sceNetListen(int s, int backlog);
int APS5_VABI sceNetAccept(int s, void* addr, uint32_t* addrlen);
int APS5_VABI sceNetConnect(int s, const void* addr, uint32_t addrlen);
int64_t APS5_VABI sceNetRecv(int s, void* buf, size_t len, int flags);
int64_t APS5_VABI sceNetRecvfrom(int s, void* buf, size_t len, int flags, void* from, uint32_t* fromlen);
int64_t APS5_VABI sceNetSend(int s, const void* buf, size_t len, int flags);
int64_t APS5_VABI sceNetSendto(int s, const void* buf, size_t len, int flags, const void* to, uint32_t tolen);
int64_t APS5_VABI sceNetSendmsg(int s, const NetMsghdr* msg, int flags);
int64_t APS5_VABI sceNetRecvmsg(int s, NetMsghdr* msg, int flags);
int APS5_VABI sceNetShutdown(int s, int how);
int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen);
int APS5_VABI sceNetGetsockopt(int s, int level, int optname, void* optval, uint32_t* optlen);
int APS5_VABI sceNetGetsockname(int s, void* addr, uint32_t* addrlen);
int APS5_VABI sceNetGetpeername(int s, void* addr, uint32_t* addrlen);
int APS5_VABI sceNetGetSockInfo(int s, void* info, int n, int flags);
int APS5_VABI sceNetEpollCreate(const char* name, int flags);
int APS5_VABI sceNetShowNetstatWithMemory(int memid);
int APS5_VABI sceNetGetMemoryPoolStats(int memid, NetMemoryPoolStats* stats);
int APS5_VABI sceNetEpollDestroy(int eid);
int APS5_VABI sceNetEpollAbort(int eid, int flags);
int APS5_VABI sceNetEpollControl(int eid, int op, int id, const NetEpollEvent* event);
int APS5_VABI sceNetEpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout);
uint32_t APS5_VABI sceNetHtonl_nid_postfix(uint32_t host32);
uint16_t APS5_VABI sceNetHtons_nid_postfix(uint16_t host16);
uint64_t APS5_VABI sceNetHtonll(std::uint64_t host64);
uint64_t APS5_VABI sceNetNtohll(std::uint64_t net64);
uint32_t APS5_VABI sceNetNtohl_nid_postfix(uint32_t net32);
uint16_t APS5_VABI sceNetNtohs_nid_postfix(uint16_t net16);
int APS5_VABI sceNetInetPton(int af, const char* src, void* dst);
const char* APS5_VABI sceNetInetNtop(int af, const void* src, char* dst, uint32_t size);
int APS5_VABI sceNetEtherNtostr(const NetEtherAddr* n, char* str, size_t len);
int APS5_VABI sceNetEtherStrton(const char* text, NetEtherAddr* address);
int APS5_VABI sceNetGetMacAddress(NetEtherAddr* addr, int flags);
int APS5_VABI sceNetResolverCreate(const char* name, int memid, int flags);
int APS5_VABI sceNetResolverDestroy(int rid);
int APS5_VABI sceNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags);
int APS5_VABI sceNetResolverStartNtoa6(int rid, const char* hostname, void* addr, int timeout, int retry, int flags);
int APS5_VABI sceNetResolverStartNtoaMultipleRecords(int rid, const char* hostname, NetResolverInfo* info, int timeout, int retry, int flags);
int APS5_VABI sceNetResolverStartNtoaMultipleRecordsEx(int rid, const char* hostname, NetResolverInfo* info, int timeout, int retry, int flags);
int APS5_VABI sceNetResolverStartAton(int rid, const void* addr, char* hostname, int len, int timeout, int retry, int flags);
int APS5_VABI sceNetResolverStartAton6(int rid, const void* addr, char* hostname, int len, int timeout, int retry, int flags);
int APS5_VABI sceNetResolverGetError(int rid, int* status);
int APS5_VABI sceNetResolverAbort(int rid, int flags);
int APS5_VABI sceNetShowIfconfig();
int APS5_VABI sceNetClearDnsCache(int flags);
int APS5_VABI sceNetGetStatisticsInfo(NetStatisticsInfo* info, std::uint32_t flags);
int APS5_VABI sceNetGetInterfaceStats(NetInterfaceStats* stats, std::uint32_t flags);
int APS5_VABI sceNetSetDnsInfo(const NetDnsInfo* info, std::uint32_t flags);
int APS5_VABI sceNetShowIfconfigWithMemory(std::int32_t poolId);
int APS5_VABI sceNetShowPolicy(void);
int APS5_VABI sceNetShowPolicyWithMemory(std::int32_t poolId);
int APS5_VABI sceNetShowRoute(void);
int APS5_VABI sceNetShowRouteWithMemory(std::int32_t poolId);
int APS5_VABI sceNetResolverConnectAbort(NetResolverConnection* connection, std::uint32_t flags);
int APS5_VABI sceNetResolverConnectCreate(const char* name, std::int32_t socketType, NetResolverConnection* connection, std::uint32_t flags);
int APS5_VABI sceNetResolverConnect(NetResolverConnection* connection, const char* hostname, std::uint16_t port, const NetResolverConnectOption* option);
int APS5_VABI sceNetResolverConnectDestroy(NetResolverConnection* connection);
int APS5_VABI sceNetGetSockInfo6(std::int32_t socket, NetSockInfo6* info, std::uint32_t capacity, std::uint32_t flags);
int APS5_VABI sceNetSetDns6Info(const NetDns6Info* info, std::uint32_t flags);
int APS5_VABI sceNetShowRoute6(void);
int APS5_VABI sceNetShowRoute6WithMemory(std::int32_t poolId);
NetRouteInfo* APS5_VABI sceNetAllocateAllRouteInfo(std::int32_t interfaceId, std::int32_t* count);
NetRouteInfo* APS5_VABI sceNetAllocateAllRouteInfo6(std::int32_t interfaceId, std::int32_t* count);
int APS5_VABI sceNetControl(std::uint32_t interfaceId, std::int32_t interfaceIndex, std::uint32_t command, void* data, std::uint32_t dataSize);
int APS5_VABI sceNetDbgInit(NetDebugParameter* parameter, std::uint32_t flags);
int APS5_VABI sceNetEventCallbackCreate(std::uint32_t interfaceId, NetEventCallback callback, void* userData, std::uint32_t flags);
int APS5_VABI sceNetEventCallbackDestroy(std::uint32_t interfaceId, std::uint32_t flags);
int APS5_VABI sceNetEventCallbackGetError(std::uint32_t interfaceId, std::int32_t* error, std::uint32_t flags);
int APS5_VABI sceNetEventCallbackWaitCB(std::int32_t timeout, void* waitData, std::uint32_t flags);
int APS5_VABI sceNetFreeAllRouteInfo(void* routes);
int APS5_VABI sceNetGetArpInfo(NetArpInfo* info, std::uint32_t flags);
int APS5_VABI sceNetGetAppMemInfo(NetAppMemoryInfo* info, std::uint32_t flags);
int APS5_VABI sceNetGetDnsInfo(NetDnsInfo* info, std::uint32_t flags);
int APS5_VABI sceNetGetIfList(std::int32_t interfaceId, NetInterfaceInfo* interfaces, std::uint32_t capacity);
int APS5_VABI sceNetGetIfListEx(std::int32_t interfaceId, NetInterfaceInfo* interfaces, std::uint32_t capacity, std::uint32_t flags);
int APS5_VABI sceNetGetIfListOnce(std::int32_t interfaceId, NetInterfaceStatistics* statistics, std::uint32_t count);
const char* APS5_VABI sceNetGetIfName(std::uint32_t index);
int APS5_VABI sceNetGetIfnameNumList(std::uint32_t* interfaceIds, std::int32_t capacity, std::uint32_t flags);
int APS5_VABI sceNetGetNameToIndex(std::uint32_t interfaceId);
int APS5_VABI sceNetGetRandom(std::uint32_t* output);
int APS5_VABI sceNetGetRouteInfo(std::int32_t family, NetRouteInfo* routes, std::int32_t capacity, std::uint8_t flags);
int APS5_VABI sceNetGetsockoptEx(int socketId, int level, int option, void* value, std::uint32_t* valueSize);
void APS5_VABI sceNetGetSystemTime(std::uint64_t* output);
int APS5_VABI sceNetGetSystemMemInfo(NetSystemMemoryInfo* info, std::uint32_t flags);
int APS5_VABI sceNetInitParam(const NetInitializeParameter* parameter);
int APS5_VABI sceNetInetPtonEx(int af, const char* src, void* dst, int);
int APS5_VABI sceNetIoctl(int socketId, std::uint32_t command, void* argument);
void* APS5_VABI sceNetMemoryAllocate(std::int32_t size, int flags);
void APS5_VABI sceNetMemoryFree(void* memory);
int APS5_VABI sceNetSetDnsInfoToKernel(const NetDnsInfo* info, std::uint32_t flags);
int APS5_VABI sceNetSetsockoptEx(int socketId, int level, int option, const void* value, std::uint32_t valueSize);
int APS5_VABI sceNetSocketInternal(const char* name, int domain, int type, int protocol);
int APS5_VABI sceNetSyncCreate(NetSyncObject* sync, std::uint32_t signals, const char* name);
int APS5_VABI sceNetSyncDestroy(NetSyncObject* sync);
NetSyncObject* APS5_VABI sceNetSyncGet(std::uint32_t index);
int APS5_VABI sceNetSyncSignal(NetSyncObject* sync, std::uint32_t signals);
int APS5_VABI sceNetSyncWait(NetSyncObject* sync, std::uint32_t* timeout);
int APS5_VABI sceNetSysctl(const int* name, std::uint32_t nameLength, void* oldValue, std::size_t* oldSize, const void* newValue, std::size_t newSize);
int APS5_VABI sceNetThreadCreate(Pthread* thread, const NetThreadParameter* parameter, const char* name);
void APS5_VABI sceNetThreadExit();
int APS5_VABI sceNetThreadJoin(void* thread);
int APS5_VABI sceNetUsleep(int microseconds);
int APS5_VABI sceNetShowIfconfigForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId);
int APS5_VABI sceNetShowRouteForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId);
int APS5_VABI sceNetShowNetstatForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId);
int APS5_VABI sceNetShowNetstatEx(std::uint32_t flags);
int APS5_VABI sceNetShowNetstatExForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId, std::uint32_t flags);
const char* APS5_VABI sceNetInetNtopWithScopeId(std::int32_t family, const void* address, char* text, std::uint32_t size, std::uint32_t scopeId);
int APS5_VABI sceNetInetPtonWithScopeId(std::int32_t family, const char* text, void* address, std::uint32_t* scopeId);
int APS5_VABI sceNetGetDns6Info(NetDns6Info* info, std::uint32_t flags);
int APS5_VABI sceNetSetDns6InfoToKernel(const NetDns6Info* info, std::uint32_t flags);
int APS5_VABI sceNetShowRoute6ForBuffer(char* buffer, std::int32_t bufferSize, std::int32_t poolId);
int APS5_VABI sceNetConfigGetIfFlags(std::uint32_t interfaceId, std::uint32_t* flags);
int APS5_VABI sceNetConfigSetIfFlags(std::uint32_t interfaceId, std::uint32_t flags);
int APS5_VABI sceNetConfigUnsetIfFlags(std::uint32_t interfaceId, std::uint32_t flags);
int APS5_VABI sceNetConfigDelIfaddr(int interfaceId);
int APS5_VABI sceNetConfigDownInterface(int interfaceId);
int APS5_VABI sceNetConfigSetDefaultRoute(const std::uint32_t* gateway);
int APS5_VABI sceNetConfigSetDefaultRouteDev(const std::uint32_t* gateway);
int APS5_VABI sceNetConfigSetIfaddr(int interfaceId, const std::uint32_t* address, const std::uint32_t* broadcastAddress, const std::uint32_t* netmask);
int APS5_VABI sceNetConfigSetIfLinkLocalAddr6(std::uint32_t interfaceId);
int APS5_VABI sceNetConfigUpInterface(int interfaceId);
int APS5_VABI sceNetConfigUpInterfaceWithFlags(std::uint32_t interfaceId, std::uint32_t flags);
int APS5_VABI sceNetConfigCleanUpAllInterfaces(void);
int APS5_VABI sceNetConfigFlushRoute(std::uint32_t interfaceId);
int APS5_VABI sceNetConfigFlushRoute6(std::uint32_t interfaceId);
int APS5_VABI sceNetConfigAddIfaddr(int interfaceId, const std::uint32_t* address, const std::uint32_t* broadcastAddress, const std::uint32_t* netmask);
int APS5_VABI sceNetConfigGetIfaddr(std::int32_t interfaceId, NetIpv4InterfaceAddress* addresses, std::int32_t capacity);
int APS5_VABI sceNetConfigSetIfmtu(std::uint32_t interfaceId, int mtu);
int APS5_VABI sceNetConfigAddArp(const NetEtherAddr* etherAddress, const std::uint32_t* address);
int APS5_VABI sceNetConfigAddArpWithInterface(int interfaceId, const NetEtherAddr* etherAddress, const std::uint32_t* address);
int APS5_VABI sceNetConfigAddMRoute(const std::uint32_t* address);
int APS5_VABI sceNetConfigAddRoute(const std::uint32_t* destination, const std::uint32_t* gateway, const std::uint32_t* netmask, int hostRoute);
int APS5_VABI sceNetConfigAddRouteWithInterface(int interfaceId, const std::uint32_t* destination, const std::uint32_t* gateway, const std::uint32_t* netmask, int hostRoute);
int APS5_VABI sceNetConfigDelArp(const NetEtherAddr* etherAddress, const std::uint32_t* address);
int APS5_VABI sceNetConfigDelArpWithInterface(int interfaceId, const NetEtherAddr* etherAddress, const std::uint32_t* address);
int APS5_VABI sceNetConfigDelDefaultRoute(void);
int APS5_VABI sceNetConfigDelDefaultRouteDev(void);
int APS5_VABI sceNetConfigDelMRoute(const std::uint32_t* address);
int APS5_VABI sceNetConfigDelRoute(const std::uint32_t* destination, const std::uint32_t* gateway, const std::uint32_t* netmask);
int APS5_VABI sceNetConfigGetDefaultRoute(std::uint32_t* gateway);
int APS5_VABI sceNetDhcpGetAutoipInfo(std::int32_t interfaceId, NetAutoIpInfo* parameter);
int APS5_VABI sceNetDhcpGetInfo(std::int32_t interfaceId, NetDhcpInfo* parameter);
int APS5_VABI sceNetDhcpGetInfoEx(std::int32_t interfaceId, std::int32_t type, NetDhcpOptionInfo* info, std::uint32_t size);
int APS5_VABI sceNetDhcpStart(std::uint32_t interfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2, NetDhcpStartParameter* parameter);
int APS5_VABI sceNetDhcpStop(int interfaceId);
int APS5_VABI sceNetConfigAddIfaddr6(int interfaceId, const NetIpv6Address* address, std::uint8_t prefixLength);
int APS5_VABI sceNetConfigDelIfaddr6(int interfaceId, const NetIpv6Address* address);
int APS5_VABI sceNetConfigDelDefaultRoute6(void);
int APS5_VABI sceNetConfigSetIfaddr6(int interfaceId, const NetIpv6Address* address, std::uint8_t prefixLength);
int APS5_VABI sceNetConfigSetIfaddr6WithFlags(int interfaceId, const NetIpv6Address* address, std::uint8_t prefixLength, std::uint32_t flags);
int APS5_VABI sceNetConfigSetDefaultRoute6(int interfaceId, const NetIpv6Address* gateway);
int APS5_VABI sceNetConfigGetIfaddr6(int interfaceId, NetIpv6InterfaceAddress* addresses, std::uint32_t count);
int APS5_VABI sceNetConfigGetDefaultRoute6(NetIpv6Address* gateway);
int APS5_VABI sceNetConfigDelRoute6(const NetIpv6Address* destination, std::uint8_t prefixLength);
int APS5_VABI sceNetConfigAddRoute6(const NetIpv6Address* destination, const NetIpv6Address* gateway, std::uint8_t prefixLength);
int APS5_VABI sceNetAddrConfig6Start(std::uint32_t interfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2, const NetAddress6StartParameter* parameter);
int APS5_VABI sceNetAddrConfig6Stop(int interfaceId);
int APS5_VABI sceNetAddrConfig6GetInfo(std::int32_t interfaceId, NetAddress6Info* parameter);
int APS5_VABI sceNetInfoDumpStart(std::int32_t threadParameter1, std::int32_t threadParameter2, const NetInfoDumpParameter* parameter);
int APS5_VABI sceNetInfoDumpStop(void);
int APS5_VABI sceNetDhcpdStart(std::uint32_t interfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2, const NetDhcpdStartParameter* parameter);
int APS5_VABI sceNetDhcpdStop(int interfaceId);
int APS5_VABI sceNetDuplicateIpStart(std::int32_t interfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2);
int APS5_VABI sceNetDuplicateIpStop(int interfaceId);
int APS5_VABI sceNetConfigWlanAdhocClearWakeOnWlan(std::uint32_t interfaceId);
int APS5_VABI sceNetConfigWlanAdhocCreate(std::int32_t interfaceId, NetWlanAdhocParameter* parameter);
int APS5_VABI sceNetConfigWlanAdhocGetWakeOnWlanInfo(std::int32_t interfaceId, NetWlanWakeInfo* parameter);
int APS5_VABI sceNetConfigWlanAdhocJoin(std::int32_t interfaceId, NetWlanAdhocParameter* parameter);
int APS5_VABI sceNetConfigWlanAdhocLeave(int interfaceId);
int APS5_VABI sceNetConfigWlanAdhocPspEmuClearWakeOnWlan(std::uint32_t interfaceId);
int APS5_VABI sceNetConfigWlanAdhocPspEmuGetWakeOnWlanInfo(std::int32_t interfaceId, NetWlanPspWakeInfo* parameter);
int APS5_VABI sceNetConfigWlanAdhocPspEmuSetWakeOnWlan(std::int32_t interfaceId, NetWlanPspWakeParameter* parameter);
int APS5_VABI sceNetConfigWlanAdhocScanJoin(std::int32_t interfaceId, const NetWlanAdhocScanParameter* parameter, void* buffer, std::size_t bufferSize, NetWlanJoinResult** result);
int APS5_VABI sceNetConfigWlanAdhocSetExtInfoElement(std::int32_t interfaceId, NetWlanInformationElement* parameter, std::int32_t size);
int APS5_VABI sceNetConfigWlanAdhocSetWakeOnWlan(std::int32_t interfaceId, NetWlanWakeParameter* parameter, std::size_t size);
int APS5_VABI sceNetConfigWlanApStart(std::int32_t interfaceId, NetWlanApParameter* parameter);
int APS5_VABI sceNetConfigWlanApStop(int interfaceId);
int APS5_VABI sceNetConfigWlanBackgroundScanQuery(std::int32_t interfaceId, void* buffer, std::int32_t size, NetWlanScanResult** results, std::size_t* count);
int APS5_VABI sceNetConfigWlanBackgroundScanStart(std::int32_t interfaceId, NetWlanScanParameter* parameter, std::uint32_t size);
int APS5_VABI sceNetConfigWlanBackgroundScanStop(int interfaceId);
int APS5_VABI sceNetConfigWlanDiagGetDeviceInfo(std::int32_t interfaceId, NetWlanDeviceInfo* parameter);
int APS5_VABI sceNetConfigWlanDiagSetAntenna(std::int32_t interfaceId, NetWlanAntennaParameter* parameter);
int APS5_VABI sceNetConfigWlanDiagSetTxFixedRate(std::int32_t interfaceId, NetWlanRateParameter* parameter);
int APS5_VABI sceNetConfigWlanGetDeviceConfig(std::int32_t interfaceId, NetWlanDeviceConfiguration* parameter);
int APS5_VABI sceNetConfigWlanInfraGetRssiInfo(std::int32_t interfaceId, NetWlanRssiInfo* parameter);
int APS5_VABI sceNetConfigWlanInfraLeave(int interfaceId);
int APS5_VABI sceNetConfigWlanInfraScanJoin(std::int32_t interfaceId, const NetWlanInfraScanParameter* parameter, void* buffer, std::size_t bufferSize, NetWlanJoinResult** result);
int APS5_VABI sceNetConfigWlanScan(std::int32_t interfaceId, const NetWlanScanParameter* parameter, std::size_t parameterSize, void* buffer, std::size_t bufferSize, NetWlanScanResult** results, std::size_t* count);
int APS5_VABI sceNetConfigWlanSetDeviceConfig(std::int32_t interfaceId, NetWlanDeviceConfiguration* parameter);
int APS5_VABI sceNetConfigEtherGetLinkMode(std::uint32_t interfaceId, int* mode);
int APS5_VABI sceNetConfigEtherPostPlugInOutEvent(std::uint32_t interfaceId);
int APS5_VABI sceNetConfigEtherSetLinkMode(std::uint32_t interfaceId, int mode);
int APS5_VABI sceNetPppoeStart(std::uint32_t interfaceId, std::int32_t pppoeInterfaceId, NetAddressCallback callback, void* userArg, std::int32_t threadParameter1, std::int32_t threadParameter2, const NetPppoeStartParameter* parameter);
int APS5_VABI sceNetPppoeStop(void);
int APS5_VABI sceNetBandwidthControlSetDefaultParam(const NetBandwidthParameter* parameter, std::uint32_t flags);
int APS5_VABI sceNetBandwidthControlGetDefaultParam(NetBandwidthParameter* parameter, std::uint32_t flags);
int APS5_VABI sceNetBandwidthControlGetDataTraffic(std::int32_t interfaceId, NetDataTraffic* traffic, std::uint32_t flags);
int APS5_VABI sceNetConfigSetDefaultScope(std::uint32_t interfaceId, const char* interfaceName, std::int32_t scopeId);
int APS5_VABI sceNetConfigRoutingStart(std::uint32_t firstInterfaceId, std::uint32_t secondInterfaceId, const NetRoutingAddressParameter* addressParameter, NetRoutingServerParameter* serverParameter, const NetRoutingRuleParameter* ruleParameter);
int APS5_VABI sceNetConfigRoutingStop(void);
int APS5_VABI sceNetConfigRoutingShowRoutingConfig(void);
int APS5_VABI sceNetConfigRoutingShowtCtlVar(const char* name);
int APS5_VABI sceNetGetStatisticsInfoInternal(NetStatisticsInfoInternal* info);
int APS5_VABI sceNetDumpAbort(std::int32_t dumpId, std::uint32_t flags);
int APS5_VABI sceNetDumpCreate(const char* name, std::int32_t bufferSize, std::uint32_t flags);
int APS5_VABI sceNetDumpSetFilterParam(std::int32_t socket, const NetDumpFilterParameter* parameter);
int APS5_VABI sceNetDumpDestroy(std::int32_t dumpId);
int APS5_VABI sceNetDumpRead(std::int32_t dumpId, void* buffer, std::int32_t bufferSize, std::uint32_t* flags);
int APS5_VABI sceNetEmulationDebugSettingsSet(const NetEmulationParameter* parameter, std::uint32_t flags);
int APS5_VABI sceNetEmulationDebugSettingsGet(NetEmulationParameter* parameter, std::uint32_t flags);
int APS5_VABI sceNetEmulationSet(const NetEmulationParameter* parameter, std::uint32_t flags);
int APS5_VABI sceNetEmulationGet(NetEmulationParameter* parameter, std::uint32_t flags);

}

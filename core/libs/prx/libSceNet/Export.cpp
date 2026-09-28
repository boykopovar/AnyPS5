#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>

// No network is emulated: pools, resolvers and epoll instances exist, but sockets cannot be
// created, so every socket operation fails with the guest's EBADF.
static constexpr int GUEST_EINTR = 4;
static constexpr int GUEST_EBADF = 9;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ENOSPC = 28;
static constexpr int GUEST_EAFNOSUPPORT = 47;
static constexpr int GUEST_ENETDOWN = 50;
static constexpr int GUEST_EHOSTUNREACH = 65;
static constexpr int GUEST_AF_INET = 2;

static thread_local int g_netErrno = 0;
static std::atomic<int> g_nextId{1};

struct Epoll {
    std::mutex lock;
    std::condition_variable changed;
    bool aborted = false;
};

static std::mutex g_epollLock;
static std::map<int, std::shared_ptr<Epoll>> g_epolls;

static int Fail(int error) {
    g_netErrno = error;
    return static_cast<int>(0x80410100u | static_cast<unsigned>(error));
}

static std::shared_ptr<Epoll> FindEpoll(int eid) {
    std::lock_guard lock(g_epollLock);
    const auto found = g_epolls.find(eid);
    return found == g_epolls.end() ? nullptr : found->second;
}

static void AbortEpoll(Epoll& epoll) {
    {
        std::lock_guard lock(epoll.lock);
        epoll.aborted = true;
    }
    epoll.changed.notify_all();
}

extern "C" {

int APS5_VABI sceNetAccept(int s, void* addr, uint32_t* addrlen) {
    (void)s;
    (void)addr;
    (void)addrlen;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetBind_nid_postfix(int s, const void* addr, uint32_t addrlen) {
    (void)s;
    (void)addr;
    (void)addrlen;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetEpollControl(int eid, int op, int id, const NetEpollEvent* event) {
    (void)op;
    (void)id;
    (void)event;
    if (!FindEpoll(eid)) return Fail(GUEST_EBADF);
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetEpollCreate(const char* name, int flags) {
    (void)name;
    (void)flags;
    const int id = g_nextId.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(g_epollLock);
    g_epolls[id] = std::make_shared<Epoll>();
    return id;
}

int APS5_VABI sceNetEpollDestroy(int eid) {
    std::lock_guard lock(g_epollLock);
    const auto found = g_epolls.find(eid);
    if (found == g_epolls.end()) return Fail(GUEST_EBADF);
    AbortEpoll(*found->second);
    g_epolls.erase(found);
    return 0;
}

int APS5_VABI sceNetEpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout) {
    (void)events;
    (void)maxevents;
    auto epoll = FindEpoll(eid);
    if (!epoll) return Fail(GUEST_EBADF);
    std::unique_lock lock(epoll->lock);
    const auto aborted = [&] { return epoll->aborted; };
    if (timeout < 0) epoll->changed.wait(lock, aborted);
    else epoll->changed.wait_for(lock, std::chrono::microseconds(timeout), aborted);
    if (epoll->aborted) {
        epoll->aborted = false;
        return Fail(GUEST_EINTR);
    }
    return 0;
}

int APS5_VABI sceNetEtherNtostr(const NetEtherAddr* n, char* str, size_t len) {
 (void)n;
 (void)str;
 (void)len;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNetGetMacAddress(NetEtherAddr* addr, int flags) {
 (void)addr;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceNetGetSockInfo(int s, void* info, int n, int flags) {
    (void)s;
    (void)info;
    (void)n;
    (void)flags;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetGetsockname(int s, void* addr, uint32_t* addrlen) {
    (void)s;
    (void)addr;
    (void)addrlen;
    return Fail(GUEST_EBADF);
}

uint32_t APS5_VABI sceNetHtonl_nid_postfix(uint32_t host32) {
    return __builtin_bswap32(host32);
}

uint16_t APS5_VABI sceNetHtons_nid_postfix(uint16_t host16) {
    return __builtin_bswap16(host16);
}

const char* APS5_VABI sceNetInetNtop(int af, const void* src, char* dst, uint32_t size) {
    if (af != GUEST_AF_INET || !src || !dst) {
        Fail(GUEST_EAFNOSUPPORT);
        return nullptr;
    }
    const auto* bytes = static_cast<const uint8_t*>(src);
    if (std::snprintf(dst, size, "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]) >= static_cast<int>(size)) {
        Fail(GUEST_ENOSPC);
        return nullptr;
    }
    return dst;
}

int APS5_VABI sceNetInetPton(int af, const char* src, void* dst) {
    if (af != GUEST_AF_INET) return Fail(GUEST_EAFNOSUPPORT);
    if (!src || !dst) return Fail(GUEST_EINVAL);
    unsigned parts[4];
    char trailing;
    if (std::sscanf(src, "%u.%u.%u.%u%c", &parts[0], &parts[1], &parts[2], &parts[3], &trailing) != 4) return 0;
    auto* bytes = static_cast<uint8_t*>(dst);
    for (int index = 0; index < 4; ++index) {
        if (parts[index] > 255) return 0;
        bytes[index] = static_cast<uint8_t>(parts[index]);
    }
    return 1;
}

int APS5_VABI sceNetInit_nid_postfix(void) {
    return 0;
}

int APS5_VABI sceNetListen(int s, int backlog) {
    (void)s;
    (void)backlog;
    return Fail(GUEST_EBADF);
}

uint32_t APS5_VABI sceNetNtohl_nid_postfix(uint32_t net32) {
    return __builtin_bswap32(net32);
}

uint16_t APS5_VABI sceNetNtohs_nid_postfix(uint16_t net16) {
    return __builtin_bswap16(net16);
}

int APS5_VABI sceNetPoolCreate(const char* name, int size, int flags) {
    (void)name;
    (void)flags;
    if (size <= 0) return Fail(GUEST_EINVAL);
    return g_nextId.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNetPoolDestroy(int memid) {
    (void)memid;
    return 0;
}

int APS5_VABI sceNetResolverCreate(const char* name, int memid, int flags) {
    (void)name;
    (void)memid;
    (void)flags;
    return g_nextId.fetch_add(1, std::memory_order_relaxed);
}

int APS5_VABI sceNetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout, int retry, int flags) {
    (void)rid;
    (void)hostname;
    (void)addr;
    (void)timeout;
    (void)retry;
    (void)flags;
    return Fail(GUEST_EHOSTUNREACH);
}

int APS5_VABI sceNetSetsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) {
    (void)s;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetShutdown(int s, int how) {
    (void)s;
    (void)how;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetSocket(const char* name, int family, int type, int protocol) {
    (void)name;
    (void)family;
    (void)type;
    (void)protocol;
    return Fail(GUEST_ENETDOWN);
}

int APS5_VABI sceNetSocketClose(int s) {
    (void)s;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetConnect(int s, const void* addr, uint32_t addrlen) {
    (void)s;
    (void)addr;
    (void)addrlen;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetEpollAbort(int eid, int flags) {
    (void)flags;
    auto epoll = FindEpoll(eid);
    if (!epoll) return Fail(GUEST_EBADF);
    AbortEpoll(*epoll);
    return 0;
}

int* APS5_VABI sceNetErrnoLoc(void) {
    return &g_netErrno;
}

int APS5_VABI sceNetGetsockopt(int s, int level, int optname, void* optval, uint32_t* optlen) {
    (void)s;
    (void)level;
    (void)optname;
    (void)optval;
    (void)optlen;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetRecv(int s, void* buf, size_t len, int flags) {
    (void)s;
    (void)buf;
    (void)len;
    (void)flags;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetRecvfrom(int s, void* buf, size_t len, int flags, void* from, uint32_t* fromlen) {
    (void)s;
    (void)buf;
    (void)len;
    (void)flags;
    (void)from;
    (void)fromlen;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetResolverDestroy(int rid) {
    (void)rid;
    return 0;
}

int APS5_VABI sceNetResolverStartAton(int rid, const void* addr, char* hostname, int len, int timeout, int retry, int flags) {
    (void)rid;
    (void)addr;
    (void)hostname;
    (void)len;
    (void)timeout;
    (void)retry;
    (void)flags;
    return Fail(GUEST_EHOSTUNREACH);
}

int APS5_VABI sceNetSend(int s, const void* msg, size_t len, int flags) {
    (void)s;
    (void)msg;
    (void)len;
    (void)flags;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetSendto(int s, const void* msg, size_t len, int flags, const void* to, uint32_t tolen) {
    (void)s;
    (void)msg;
    (void)len;
    (void)flags;
    (void)to;
    (void)tolen;
    return Fail(GUEST_EBADF);
}

int APS5_VABI sceNetSocketAbort(int s, int flags) {
    (void)s;
    (void)flags;
    return Fail(GUEST_EBADF);
}

}

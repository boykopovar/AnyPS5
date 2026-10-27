#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libSceNetCtl/include/NetCtl.hpp"
#include "prx/libSceNet/include/Net.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <cerrno>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/Socket/include/SocketSelectDeadline.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libSceNet/src/ResolverTestHooks.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <future>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

extern "C" {
extern const std::uint8_t in6addr_any_nid_postfix[16];
extern const std::uint8_t in6addr_loopback_nid_postfix[16];

extern const std::uint32_t sce_net_in6addr_any[4];
extern const std::uint8_t sce_net_in6addr_linklocal_allnodes[16];

std::uint64_t NetCtlReadByteCounter_nid_no_patch(const char*);

bool GuestSocketIsOpen_nid_no_patch(int);

}

static void Require(bool condition) {
    if (!condition) std::abort();
}

static bool FailResolverLookup(const char* hostname, int family, std::vector<std::array<std::uint8_t, 16>>&) {
    Require(hostname != nullptr && (std::strcmp(hostname, "guest-sce-net.invalid.") == 0 ||
        std::strcmp(hostname, "guest-sce-net.invalid") == 0));
    Require(family == AF_INET || family == AF_INET6);
    return false;
}

class ScopedResolverLookupOverride {
public:
    explicit ScopedResolverLookupOverride(GuestSceNetTest::ResolverLookup lookup) {
        GuestSceNetTest::SetResolverLookupForTests_nid_no_patch(lookup);
    }
    ~ScopedResolverLookupOverride() {
        GuestSceNetTest::SetResolverLookupForTests_nid_no_patch(nullptr);
    }
};

static void APS5_VABI IgnoreNetCtlEvent(int, void*) {}

struct NetCtlEventRecord {
    int calls = 0;
    int event = 0;
    void* arg = nullptr;
};

static void APS5_VABI RecordNetCtlEvent(int event, void* arg) {
    auto* record = static_cast<NetCtlEventRecord*>(arg);
    ++record->calls;
    record->event = event;
    record->arg = arg;
}

static bool Failed(std::int64_t result, int error) {
    return result == static_cast<int>(0x80410100u | static_cast<unsigned>(error)) && *sceNetErrnoLoc() == error;
}

template<typename TBody>
static std::string CaptureStdout(TBody body) {
    std::fflush(stdout);
    std::FILE* capture = std::tmpfile();
    Require(capture != nullptr);
#ifdef _WIN32
    const int saved = _dup(_fileno(stdout));
    Require(saved >= 0 && _dup2(_fileno(capture), _fileno(stdout)) == 0);
#else
    const int saved = dup(fileno(stdout));
    Require(saved >= 0 && dup2(fileno(capture), fileno(stdout)) >= 0);
#endif
    body();
    std::fflush(stdout);
#ifdef _WIN32
    Require(_dup2(saved, _fileno(stdout)) == 0);
    _close(saved);
#else
    Require(dup2(saved, fileno(stdout)) >= 0);
    close(saved);
#endif
    std::string text;
    std::rewind(capture);
    for (int character = std::fgetc(capture); character != EOF; character = std::fgetc(capture)) text.push_back(static_cast<char>(character));
    std::fclose(capture);
    return text;
}

static void CheckSocketHostErrors() {
    Require(Failed(sceNetSocket(nullptr, 2, 1, 17), 43));
}

static void CheckStreamSender(int family) {
    const auto addressSize = family == 2 ? 16u : 28u;
    const int listener = sceNetSocket(nullptr, family, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 28> destination{};
    destination[0] = static_cast<std::uint8_t>(addressSize);
    destination[1] = static_cast<std::uint8_t>(family);
    if (family == 2) { destination[4] = 127; destination[7] = 1; }
    else destination[23] = 1;
    Require(sceNetBind_nid_postfix(listener, destination.data(), addressSize) == 0 && sceNetListen(listener, 1) == 0);
    std::uint32_t size = destination.size();
    Require(sceNetGetsockname(listener, destination.data(), &size) == 0 && size == addressSize);
    const int client = sceNetSocket(nullptr, family, 1, 6);
    Require(client >= 0 && sceNetConnect(client, destination.data(), addressSize) == 0);
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0);
    const char payload[] = "stream";
    for (const auto endpoints : {std::array{accepted, client}, std::array{client, accepted}}) {
        const auto receiver = endpoints[0], sender = endpoints[1];
        for (const std::uint32_t capacity : {0u, 1u, 16u, 28u, UINT32_MAX}) {
            Require(sceNetSend(sender, payload, sizeof(payload), 0) == sizeof(payload));
            std::array<std::uint8_t, 28> address;
            address.fill(0xa5);
            char received[sizeof(payload)]{};
            size = capacity;
            *sceNetErrnoLoc() = 123;
            Require(sceNetRecvfrom(receiver, received, 0, 2, address.data(), &size) == 0);
            Require(size == 0 && received[0] == 0 && *sceNetErrnoLoc() == 123);
            Require(std::all_of(address.begin(), address.end(), [](auto byte) { return byte == 0xa5; }));
            for (const int flags : {2, 0}) {
                size = capacity;
                *sceNetErrnoLoc() = 123;
                Require(sceNetRecvfrom(receiver, received, sizeof(received), flags, address.data(), &size) == sizeof(payload));
                Require(size == 0 && std::memcmp(received, payload, sizeof(payload)) == 0 && *sceNetErrnoLoc() == 123);
                Require(std::all_of(address.begin(), address.end(), [](auto byte) { return byte == 0xa5; }));
            }
        }
        Require(sceNetSend(sender, payload, sizeof(payload), 0) == sizeof(payload));
        std::array<std::uint8_t, 28> address;
        address.fill(0xa5);
        char received[sizeof(payload)]{};
        Require(Failed(sceNetRecvfrom(receiver, received, sizeof(received), 0, address.data(), nullptr), 22));
        size = 123;
        Require(sceNetRecvfrom(receiver, received, sizeof(received), 0, nullptr, &size) == sizeof(payload));
        Require(size == 123 && std::memcmp(received, payload, sizeof(payload)) == 0);
        Require(sceNetShutdown(sender, 1) == 0);
        for (const int flags : {2, 0}) {
            size = address.size();
            *sceNetErrnoLoc() = 123;
            Require(sceNetRecvfrom(receiver, received, sizeof(received), flags, address.data(), &size) == 0);
            Require(size == 0 && *sceNetErrnoLoc() == 123);
            Require(std::all_of(address.begin(), address.end(), [](auto byte) { return byte == 0xa5; }));
        }
    }
    for (const int descriptor : {client, accepted, listener}) Require(sceNetSocketClose(descriptor) == 0);
}

static std::string CaptureNetstat(int pool) {
    auto* file = std::tmpfile();
    Require(file != nullptr);
    Require(std::fflush(stdout) == 0);
#ifdef _WIN32
    const int output = _fileno(stdout);
    const int saved = _dup(output);
    Require(saved >= 0 && _dup2(_fileno(file), output) == 0);
#else
    const int output = fileno(stdout);
    const int saved = dup(output);
    Require(saved >= 0 && dup2(fileno(file), output) == output);
#endif
    const int result = sceNetShowNetstatWithMemory(pool);
    Require(std::fflush(stdout) == 0);
#ifdef _WIN32
    Require(_dup2(saved, output) == 0 && _close(saved) == 0);
#else
    Require(dup2(saved, output) == output && close(saved) == 0);
#endif
    Require(result == 0 && std::fseek(file, 0, SEEK_SET) == 0);
    std::string text;
    std::array<char, 256> bytes{};
    for (std::size_t count; (count = std::fread(bytes.data(), 1, bytes.size(), file)) != 0;) text.append(bytes.data(), count);
    Require(std::ferror(file) == 0 && std::fclose(file) == 0);
    return text;
}

static void CheckNetstat() {
    for (const auto diagnostic : {sceNetShowNetstat, sceNetShowIfconfig}) {
        bool unsupported = false;
        try {
            diagnostic();
        } catch (const std::runtime_error&) {
            unsupported = true;
        }
        Require(unsupported);
    }
    Require(sceNetShowPolicy() == 0);
    Require(sceNetShowNetstatEx(0) == 0);
    Require(sceNetShowRoute() == 0);
    Require(sceNetShowRoute6() == 0);
    Require(Failed(sceNetShowNetstatWithMemory(-1), 9));
    Require(Failed(sceNetShowRouteWithMemory(-1), 9));
    const int pool = sceNetPoolCreate("netstat", 4096, 0);
    Require(pool > 0);
    Require(sceNetShowRouteWithMemory(pool) == 0);
    const auto empty = CaptureNetstat(pool);
    Require(empty.find("bytes=4096 sockets=0") != std::string::npos);
    const int socket = sceNetSocket("netstat", 2, 1, 0);
    Require(socket >= 0);
    const int nonblocking = 1;
    Require(sceNetSetsockopt(socket, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(socket, address.data(), address.size()) == 0);
    Require(sceNetListen(socket, 1) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(socket, address.data(), &size) == 0 && size == address.size());
    const auto before = address;
    const auto snapshot = CaptureNetstat(pool);
    Require(snapshot.find("bytes=4096 sockets=1") != std::string::npos);
    Require(snapshot.find("socket=" + std::to_string(socket) + " family=2 type=1 nonblock=1 bound=1 listening=1") != std::string::npos);
    Require(sceNetGetsockname(socket, address.data(), &size) == 0 && address == before);
    Require(sceNetSocketClose(socket) == 0);
    Require(CaptureNetstat(pool).find("sockets=0") != std::string::npos);
    Require(sceNetPoolDestroy(pool) == 0);
    Require(Failed(sceNetShowNetstatWithMemory(pool), 9));
    Require(Failed(sceNetShowRouteWithMemory(pool), 9));
}

static void CheckPoolStats() {
    const int pool = sceNetPoolCreate("stats", 0x4000, 0);
    Require(pool > 0);
    NetMemoryPoolStats stats{1, 1, 1};
    Require(sceNetGetMemoryPoolStats(pool, &stats) == 0);
    Require(stats.pool_size == 0x4000 && stats.max_inuse_size == 0 && stats.current_inuse_size == 0);
    Require(Failed(sceNetGetMemoryPoolStats(pool, nullptr), 22));
    Require(sceNetPoolDestroy(pool) == 0);
    Require(Failed(sceNetGetMemoryPoolStats(pool, &stats), 9));
}

static void CheckIfName() {
    constexpr std::array<const char*, 10> names = {
        "lo0", "eth0", "eth1", "dbg0", "wlan0", "wlan1", "gbe0", "bt0", "phone0", "pppoe0"};
    for (std::uint32_t index = 0; index < names.size(); ++index) {
        Require(std::strcmp(sceNetGetIfName(index), names[index]) == 0);
    }
    *sceNetErrnoLoc() = 0;
    const char* missing = sceNetGetIfName(10);
    Require(missing != nullptr && missing[0] == '\0' && *sceNetErrnoLoc() == 22);
    *sceNetErrnoLoc() = 0;
    missing = sceNetGetIfName(0xffffffffu);
    Require(missing != nullptr && missing[0] == '\0' && *sceNetErrnoLoc() == 22);
}

static void CheckRandom() {
    Require(Failed(sceNetGetRandom(nullptr), 22));
    std::array<std::uint32_t, 8> values{};
    for (auto& value : values) Require(sceNetGetRandom(&value) == 0);
    std::array<std::uint32_t, 2> guarded{0x5a5a5a5au, 0x5a5a5a5au};
    Require(sceNetGetRandom(guarded.data()) == 0 && guarded[1] == 0x5a5a5a5au && *sceNetErrnoLoc() == 22);
    Require(std::any_of(values.begin() + 1, values.end(), [&](std::uint32_t value) { return value != values[0]; }));
}

static void CheckByteOrderConversion() {
    *sceNetErrnoLoc() = 123;
    Require(sceNetHtons_nid_postfix(0x1234) == 0x3412);
    Require(sceNetNtohs_nid_postfix(0x3412) == 0x1234);
    Require(sceNetHtons_nid_postfix(0x1234) == static_cast<std::uint16_t>(htons(0x1234)));
    Require(sceNetHtonl_nid_postfix(0x12345678u) == 0x78563412u);
    Require(sceNetNtohl_nid_postfix(0x78563412u) == 0x12345678u);
    Require(sceNetHtonl_nid_postfix(0x12345678u) == static_cast<std::uint32_t>(htonl(0x12345678u)));
    Require(sceNetHtonll(0x0123456789abcdefULL) == 0xefcdab8967452301ULL);
    Require(sceNetNtohll(0xefcdab8967452301ULL) == 0x0123456789abcdefULL);
    Require(sceNetHtons_nid_postfix(0x1212) == 0x1212);
    Require(sceNetHtonl_nid_postfix(0x12343412u) == 0x12343412u);
    Require(sceNetHtonll(0x0123456767452301ULL) == 0x0123456767452301ULL);
    for (const std::uint16_t value : {std::uint16_t{0x0000}, std::uint16_t{0x0001}, std::uint16_t{0x00ff}, std::uint16_t{0x8000},
                                      std::uint16_t{0xff00}, std::uint16_t{0xffff}}) {
        Require(sceNetNtohs_nid_postfix(sceNetHtons_nid_postfix(value)) == value);
    }
    for (const std::uint32_t value : {0x00000000u, 0x00000001u, 0x0000ffffu, 0x80000000u, 0xff000000u, 0xffffffffu}) {
        Require(sceNetNtohl_nid_postfix(sceNetHtonl_nid_postfix(value)) == value);
    }
    for (const std::uint64_t value : {std::uint64_t{0}, std::uint64_t{1}, std::uint64_t{0x00000000ffffffff}, std::uint64_t{0xffffffff00000000},
                                      std::uint64_t{0x8000000000000000}, UINT64_MAX}) {
        Require(sceNetNtohll(sceNetHtonll(value)) == value);
    }
    Require(*sceNetErrnoLoc() == 123);
}

static void CheckInitParam() {
    alignas(8) std::array<std::uint8_t, 64> parameter{};
    const auto* initialize = reinterpret_cast<const NetInitializeParameter*>(parameter.data());
    std::uint32_t randomOutput = 0;
    Require(sceNetTerm() == 0);
    Require(Failed(sceNetGetRandom(&randomOutput), 200));
    Require(Failed(sceNetInitParam(nullptr), 22));
    Require(Failed(sceNetGetRandom(&randomOutput), 200));
    Require(sceNetInitParam(initialize) == 0);
    Require(sceNetGetRandom(&randomOutput) == 0);
    Require(Failed(sceNetInitParam(initialize), 16));
    Require(Failed(sceNetInitParam(nullptr), 16));
    Require(sceNetTerm() == 0);
    Require(Failed(sceNetInitParam(initialize), 16));
    Require(Failed(sceNetGetRandom(&randomOutput), 200));
    Require(sceNetInit_nid_postfix() == 0);
}

static void CheckIfFlags() {
    std::uint32_t flags = 0x5a5a5a5a;
    Require(sceNetConfigGetIfFlags(0, &flags) == 0);
    Require(flags == 0x8049);
    for (const std::uint32_t interfaceId : {10u, 0xffffffffu}) {
        flags = 0x5a5a5a5a;
        Require(sceNetConfigGetIfFlags(interfaceId, &flags) == -1);
        Require(flags == 0x5a5a5a5a);
    }
    Require(sceNetConfigGetIfFlags(0, nullptr) == static_cast<int>(0x8041130Eu));
    Require(sceNetConfigGetIfFlags(10, nullptr) == static_cast<int>(0x8041130Eu));
}

static void CheckNetMemory() {
    *sceNetErrnoLoc() = 33;
    for (const std::int32_t size : {0, -1, INT32_MIN}) {
        Require(sceNetMemoryAllocate(size, 0) == nullptr);
        Require(sceNetMemoryAllocate(size, 2) == nullptr);
        Require(sceNetMemoryAllocate(size, 4) == nullptr);
        Require(sceNetMemoryAllocate(size, -1) == nullptr);
    }
    Require(*sceNetErrnoLoc() == 33);
    auto* zeroed = static_cast<std::uint8_t*>(sceNetMemoryAllocate(4096, 2));
    Require(zeroed != nullptr);
    Require(std::all_of(zeroed, zeroed + 4096, [](std::uint8_t byte) { return byte == 0; }));
    std::memset(zeroed, 0xa5, 4096);
    auto* plain = static_cast<std::uint8_t*>(sceNetMemoryAllocate(64, 0));
    Require(plain != nullptr && plain != zeroed);
    std::memset(plain, 0x5a, 64);
    Require(zeroed[4095] == 0xa5);
    sceNetMemoryFree(plain);
    sceNetMemoryFree(zeroed);
    for (const int flags : {1, 3, 4, 6, 8, 0x10, 0x100, 0x40000001, -5, INT32_MIN}) {
        auto* block = static_cast<std::uint8_t*>(sceNetMemoryAllocate(16, flags));
        Require(block != nullptr && reinterpret_cast<std::uintptr_t>(block) % 64 == 0);
        if ((flags & 2) != 0) Require(std::all_of(block, block + 16, [](std::uint8_t byte) { return byte == 0; }));
        std::memset(block, 0x5a, 16);
        sceNetMemoryFree(block);
    }
    for (const std::int32_t size : {1, 7, 256, 65536}) {
        void* block = sceNetMemoryAllocate(size, 0);
        Require(block != nullptr && reinterpret_cast<std::uintptr_t>(block) % 64 == 0);
        sceNetMemoryFree(block);
    }
    sceNetMemoryFree(nullptr);
    Require(*sceNetErrnoLoc() == 33);
}

static void CheckUnboundSocketName() {
    for (const int family : {2, 28}) {
        for (const int type : {1, 2}) {
            const int socket = sceNetSocket("unbound", family, type, type == 1 ? 6 : 17);
            if (socket < 0 && family == 28 && *sceNetErrnoLoc() == 47) continue;
            Require(socket >= 0);
            const std::uint32_t addressSize = family == 2 ? 16 : 28;
            std::array<std::uint8_t, 28> address;
            address.fill(0xa5);
            std::uint32_t size = address.size();
            Require(sceNetGetsockname(socket, address.data(), &size) == 0 && size == addressSize);
            Require(address[0] == addressSize && address[1] == family);
            Require(std::all_of(address.begin() + 2, address.begin() + addressSize, [](auto byte) { return byte == 0; }));
            Require(sceNetSocketClose(socket) == 0);
        }
    }
}

static void CheckUnboundDatagramReceive() {
    const int waiting = sceNetSocket("unbound-receive", 2, 2, 17);
    const int peer = sceNetSocket("unbound-peer", 2, 2, 17);
    Require(waiting >= 0 && peer >= 0);
    const int timeout = 50000;
    Require(sceNetSetsockopt(waiting, 0xffff, 0x1106, &timeout, sizeof(timeout)) == 0);
    char buffer[16]{};
    NetIovec in[] = {{buffer, sizeof(buffer)}};
    NetMsghdr receive{nullptr, 0, in, 1, nullptr, 0, 0};
    for (int call = 0; call < 3; ++call) {
        const auto start = std::chrono::steady_clock::now();
        const std::int64_t result = call == 0 ? sceNetRecv(waiting, buffer, sizeof(buffer), 0)
            : call == 1 ? sceNetRecvfrom(waiting, buffer, sizeof(buffer), 0, nullptr, nullptr)
                        : sceNetRecvmsg(waiting, &receive, 0);
        Require(Failed(result, 35));
        Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    }
    const int on = 1;
    const int off = 0;
    Require(sceNetSetsockopt(waiting, 0xffff, 0x1200, &on, sizeof(on)) == 0);
    Require(Failed(sceNetRecvfrom(waiting, buffer, sizeof(buffer), 0, nullptr, nullptr), 35));
    Require(sceNetSetsockopt(waiting, 0xffff, 0x1200, &off, sizeof(off)) == 0);
    const int longTimeout = 2000000;
    Require(sceNetSetsockopt(waiting, 0xffff, 0x1106, &longTimeout, sizeof(longTimeout)) == 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(peer, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(peer, address.data(), &size) == 0);
    auto reply = std::async(std::launch::async, [waiting] {
        char answer[16]{};
        return sceNetRecvfrom(waiting, answer, sizeof(answer), 0, nullptr, nullptr) == 5 && std::strcmp(answer, "pong") == 0;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(reply.wait_for(std::chrono::seconds(0)) == std::future_status::timeout);
    Require(sceNetSendto(waiting, "ping", 5, 0, address.data(), address.size()) == 5);
    std::array<std::uint8_t, 16> source{};
    size = source.size();
    Require(sceNetRecvfrom(peer, buffer, sizeof(buffer), 0, source.data(), &size) == 5 && std::strcmp(buffer, "ping") == 0);
    Require(sceNetSendto(peer, "pong", 5, 0, source.data(), size) == 5);
    Require(reply.wait_for(std::chrono::seconds(1)) == std::future_status::ready && reply.get());
    Require(Failed(sceNetBind_nid_postfix(waiting, address.data(), address.size()), 22));
    for (const int descriptor : {waiting, peer}) Require(sceNetSocketClose(descriptor) == 0);
#ifdef _WIN32
    for (const bool abort : {true, false}) {
        const int socket = sceNetSocket("unbound-abort", 2, 2, 17);
        Require(socket >= 0);
        auto waiter = std::async(std::launch::async, [socket] {
            char ignored[4];
            return sceNetRecvfrom(socket, ignored, sizeof(ignored), 0, nullptr, nullptr);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(waiter.wait_for(std::chrono::seconds(0)) == std::future_status::timeout);
        Require((abort ? sceNetSocketAbort(socket, 0) : sceNetSocketClose(socket)) == 0);
        Require(waiter.wait_for(std::chrono::seconds(1)) == std::future_status::ready && waiter.get() < 0);
        if (abort) Require(sceNetSocketClose(socket) == 0);
    }
#endif
}

static void CheckUnboundDatagramShutdown() {
    const int socket = sceNetSocket("unbound-shutdown", 2, 2, 17);
    Require(socket >= 0);
    auto waiter = std::async(std::launch::async, [socket] {
        char ignored[4];
        return sceNetRecvfrom(socket, ignored, sizeof(ignored), 0, nullptr, nullptr);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(waiter.wait_for(std::chrono::seconds(0)) == std::future_status::timeout);
    sceNetShutdown(socket, 0);
    Require(waiter.wait_for(std::chrono::seconds(1)) == std::future_status::ready && waiter.get() == 0);
    char buffer[4]{};
    Require(sceNetRecv(socket, buffer, sizeof(buffer), 0) == 0);
    std::array<std::uint8_t, 16> source{};
    NetIovec in[] = {{buffer, sizeof(buffer)}};
    NetMsghdr receive{source.data(), static_cast<std::uint32_t>(source.size()), in, 1, nullptr, 0, 0};
    Require(sceNetRecvmsg(socket, &receive, 0) == 0 && receive.name_length == 0);
#ifdef _WIN32
    std::uint32_t size = source.size();
    Require(sceNetRecvfrom(socket, buffer, sizeof(buffer), 0, source.data(), &size) == 0 && size == 0);
#endif
    Require(sceNetSocketClose(socket) == 0);
#ifdef _WIN32
    const int empty = sceNetSocket("unbound-empty", 2, 2, 17);
    Require(empty >= 0);
    Require(sceNetRecv(empty, buffer, 0, 0) == 0);
    Require(sceNetRecvfrom(empty, buffer, 0, 0, nullptr, nullptr) == 0);
    Require(sceNetSocketClose(empty) == 0);
#endif
}

static void CheckUnboundDatagramLaterAddress() {
    for (const bool connect : {false, true}) {
        const int waiting = sceNetSocket("unbound-later", 2, 2, 17);
        const int peer = sceNetSocket("unbound-later-peer", 2, 2, 17);
        Require(waiting >= 0 && peer >= 0);
        std::array<std::uint8_t, 16> peerAddress{16, 2, 0, 0, 127, 0, 0, 1};
        Require(sceNetBind_nid_postfix(peer, peerAddress.data(), peerAddress.size()) == 0);
        std::uint32_t size = peerAddress.size();
        Require(sceNetGetsockname(peer, peerAddress.data(), &size) == 0);
        auto reply = std::async(std::launch::async, [waiting] {
            char answer[16]{};
            return sceNetRecv(waiting, answer, sizeof(answer), 0) == 5 && std::strcmp(answer, "pong") == 0;
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require(reply.wait_for(std::chrono::seconds(0)) == std::future_status::timeout);
        std::array<std::uint8_t, 16> waitingAddress{16, 2, 0, 0, 127, 0, 0, 1};
        Require((connect ? sceNetConnect(waiting, peerAddress.data(), peerAddress.size())
                         : sceNetBind_nid_postfix(waiting, waitingAddress.data(), waitingAddress.size())) == 0);
        size = waitingAddress.size();
        Require(sceNetGetsockname(waiting, waitingAddress.data(), &size) == 0);
        Require(sceNetSendto(peer, "pong", 5, 0, waitingAddress.data(), size) == 5);
        Require(reply.wait_for(std::chrono::seconds(1)) == std::future_status::ready && reply.get());
        for (const int descriptor : {waiting, peer}) Require(sceNetSocketClose(descriptor) == 0);
    }
}

static void CheckUnboundDatagramTimeoutAfterAddress() {
    const int waiting = sceNetSocket("unbound-timeout", 2, 2, 17);
    Require(waiting >= 0);
    const int timeout = 500000;
    Require(sceNetSetsockopt(waiting, 0xffff, 0x1106, &timeout, sizeof(timeout)) == 0);
    auto bound = std::async(std::launch::async, [waiting] {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
        return sceNetBind_nid_postfix(waiting, address.data(), address.size()) == 0;
    });
    char buffer[4];
    const auto start = std::chrono::steady_clock::now();
    Require(Failed(sceNetRecv(waiting, buffer, sizeof(buffer), 0), 35));
    const auto elapsed = std::chrono::steady_clock::now() - start;
    Require(bound.get());
    Require(elapsed >= std::chrono::milliseconds(450) && elapsed < std::chrono::milliseconds(750));
    Require(sceNetSocketClose(waiting) == 0);
}

static void CheckEmptyEpoll() {
    const int epoll = sceNetEpollCreate("empty-epoll", 0);
    Require(epoll >= 0);
    NetEpollEvent event{};
    Require(sceNetEpollWait(epoll, &event, 1, 0) == 0);
    std::array<std::chrono::steady_clock::duration, 9> elapsed{};
    for (auto& sample : elapsed) {
        const auto start = std::chrono::steady_clock::now();
        Require(sceNetEpollWait(epoll, &event, 1, 1000) == 0);
        sample = std::chrono::steady_clock::now() - start;
        Require(sample >= std::chrono::microseconds(1000));
    }
#ifdef _WIN32
    if (std::getenv("APS5_NO_TIMER_RESOLUTION") == nullptr) {
        std::sort(elapsed.begin(), elapsed.end());
        Require(elapsed[elapsed.size() / 2] < std::chrono::milliseconds(8));
    }
#endif
    Require(sceNetEpollAbort(epoll, 0) == 0);
    Require(Failed(sceNetEpollWait(epoll, &event, 1, 1000), 53));
    Require(sceNetEpollWait(epoll, &event, 1, 0) == 0);
    Require(sceNetEpollDestroy(epoll) == 0);

    for (const bool abort : {true, false}) {
        const int waiting_epoll = sceNetEpollCreate("waiting-epoll", 0);
        Require(waiting_epoll >= 0);
        auto waiter = std::async(std::launch::async, [waiting_epoll, abort] {
            NetEpollEvent ready{};
            Require(Failed(sceNetEpollWait(waiting_epoll, &ready, 1, -1), abort ? 53 : 9));
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        Require((abort ? sceNetEpollAbort(waiting_epoll, 0) : sceNetEpollDestroy(waiting_epoll)) == 0);
        Require(waiter.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
        waiter.get();
        if (abort) Require(sceNetEpollDestroy(waiting_epoll) == 0);
    }
}

static void CheckRejectedArguments() {
    Require(Failed(sceNetEpollCreate(nullptr, 0), 22));
    for (const int flags : {1, 0x10000, -1}) Require(Failed(sceNetEpollCreate("rejected-epoll", flags), 22));

    std::array<std::uint8_t, 6> untouched{};
    untouched.fill(0xA5);
    NetEtherAddr mac{};
    std::memcpy(mac.data, untouched.data(), untouched.size());
    for (const int flags : {1, 0x10000, -1}) {
        Require(Failed(sceNetGetMacAddress(&mac, flags), 22));
        Require(std::memcmp(mac.data, untouched.data(), untouched.size()) == 0);
    }
    Require(Failed(sceNetGetMacAddress(nullptr, 1), 22));

    for (const int flags : {1, 0x10000, -1}) Require(Failed(sceNetResolverCreate("rejected-resolver", 0, flags), 22));

    std::array<std::uint8_t, 4> ipv4{};
    const int resolver = sceNetResolverCreate("rejected-ntoa", 0, 0);
    Require(resolver >= 0);
    Require(sceNetResolverAbort(resolver, 1) == 0);
    Require(Failed(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), -1, 1, 0), 22));
    Require(Failed(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, -1, 0), 22));
    for (const int flags : {2, 0x20000, 0x10003, static_cast<int>(0x80000000u), -1})
        Require(Failed(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, flags), 22));
    Require(Failed(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0), 4));
    Require(sceNetResolverDestroy(resolver) == 0);

    Require(Failed(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), -1, 0, 0), 22));
    Require(Failed(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 0, -1, 0), 22));
    Require(Failed(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 0, 0, 2), 22));
    for (const int flags : {0, 1, 0x10000, 0x10001})
        Require(Failed(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 0, 0, flags), 9));
}

static void CheckDontWait() {
    constexpr int DontWait = 0x80;
    constexpr int Peek = 0x2;
    const int receiver = sceNetSocket("dontwait-receiver", 2, 2, 17);
    const int sender = sceNetSocket("dontwait-sender", 2, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(receiver, address.data(), &size) == 0);
    const int timeout = 2000000;
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1106, &timeout, sizeof(timeout)) == 0);
    char buffer[16]{};
    NetIovec in[] = {{buffer, sizeof(buffer)}};
    NetMsghdr receive{nullptr, 0, in, 1, nullptr, 0, 0};
    const auto start = std::chrono::steady_clock::now();
    const int unbound = sceNetSocket("dontwait-unbound", 2, 2, 17);
    Require(unbound >= 0);
    const auto unboundStart = std::chrono::steady_clock::now();
    Require(Failed(sceNetRecv(unbound, buffer, sizeof(buffer), DontWait), 35));
    Require(Failed(sceNetRecvfrom(unbound, buffer, sizeof(buffer), DontWait, nullptr, nullptr), 35));
    Require(Failed(sceNetRecvmsg(unbound, &receive, DontWait), 35));
    Require(std::chrono::steady_clock::now() - unboundStart < std::chrono::seconds(1));
    Require(sceNetSocketClose(unbound) == 0);
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait), 35));
    Require(Failed(sceNetRecvfrom(receiver, buffer, sizeof(buffer), DontWait, nullptr, nullptr), 35));
    Require(Failed(sceNetRecvmsg(receiver, &receive, DontWait), 35));
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait | Peek), 35));
    Require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
    const char datagram[] = "nowait";
    Require(sceNetSendto(sender, datagram, sizeof(datagram), DontWait, address.data(), address.size()) == sizeof(datagram));
    Require(sceNetRecvfrom(receiver, buffer, sizeof(buffer), Peek, nullptr, nullptr) == sizeof(datagram));
    std::memset(buffer, 0, sizeof(buffer));
    Require(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait | Peek) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    std::memset(buffer, 0, sizeof(buffer));
    Require(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    Require(sceNetConnect(sender, address.data(), address.size()) == 0);
    Require(sceNetSend(sender, datagram, sizeof(datagram), DontWait) == sizeof(datagram));
    Require(sceNetRecvmsg(receiver, &receive, 0) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), DontWait), 35));
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), 0x1), 45));
    Require(Failed(sceNetSend(sender, "x", 1, Peek), 45));
    Require(sceNetSocketClose(receiver) == 0 && sceNetSocketClose(sender) == 0);
}

static void CheckNoSignal() {
    constexpr int NoSignal = 0x20000;
    constexpr int DontWait = 0x80;
    const int listener = sceNetSocket("nosignal-listener", 2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(sceNetListen(listener, 1) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(listener, address.data(), &size) == 0);
    const int client = sceNetSocket("nosignal-client", 2, 1, 6);
    Require(client >= 0);
    Require(sceNetConnect(client, address.data(), address.size()) == 0);
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0);
    char reply[] = "nosignal reply";
    Require(sceNetSend(accepted, reply, 4, NoSignal) == 4);
    Require(sceNetSendto(accepted, reply + 4, 4, NoSignal | DontWait, nullptr, 0) == 4);
    NetIovec out[] = {{reply + 8, sizeof(reply) - 8}};
    NetMsghdr message{nullptr, 0, out, 1, nullptr, 0, 0};
    Require(sceNetSendmsg(accepted, &message, NoSignal) == static_cast<std::int64_t>(sizeof(reply) - 8));
    char received[sizeof(reply)]{};
    std::size_t total = 0;
    while (total < sizeof(reply)) {
        const auto count = sceNetRecv(client, received + total, sizeof(received) - total, 0);
        Require(count > 0);
        total += static_cast<std::size_t>(count);
    }
    Require(std::memcmp(received, reply, sizeof(reply)) == 0);
    Require(Failed(sceNetSend(accepted, reply, 1, NoSignal | 0x2), 45));
    Require(sceNetShutdown(accepted, 1) == 0);
    Require(Failed(sceNetSend(accepted, reply, 1, NoSignal), 32));
    Require(Failed(sceNetSendto(accepted, reply, 1, NoSignal, nullptr, 0), 32));
    Require(sceNetSocketClose(client) == 0 && sceNetSocketClose(accepted) == 0 && sceNetSocketClose(listener) == 0);
}

static void CheckReceiveTimeout(int receiver, int sender, const std::array<std::uint8_t, 16>& address) {
    const KernelTimeval timeout{0, 50000};
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1006, &timeout, sizeof(timeout)) == 0);
    int microseconds = -1;
    std::uint32_t length = sizeof(microseconds);
    Require(sceNetGetsockopt(receiver, 0xffff, 0x1106, &microseconds, &length) == 0 && microseconds == 50000);
    char buffer[8]{};
    std::array<std::uint8_t, 16> source{};
    std::uint32_t size = source.size();
    NetIovec in[] = {{buffer, sizeof(buffer)}};
    NetMsghdr receive{source.data(), static_cast<std::uint32_t>(source.size()), in, 1, nullptr, 0, 0};
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), 0), 35));
    Require(Failed(sceNetRecvfrom(receiver, buffer, sizeof(buffer), 0, source.data(), &size), 35));
    Require(Failed(sceNetRecvmsg(receiver, &receive, 0), 35));
    const char datagram[] = "late";
    Require(sceNetSendto(sender, datagram, sizeof(datagram), 0, address.data(), address.size()) == sizeof(datagram));
    Require(sceNetRecv(receiver, buffer, sizeof(buffer), 0) == sizeof(datagram) && std::strcmp(buffer, datagram) == 0);
    const KernelTimeval disabled{0, 0};
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1006, &disabled, sizeof(disabled)) == 0);
}

static void CheckStreamReceiveTimeout() {
    const int listener = sceNetSocket("stream-timeout", 2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0 && sceNetListen(listener, 1) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(listener, address.data(), &size) == 0);
    const int client = sceNetSocket("stream-timeout-client", 2, 1, 6);
    Require(client >= 0 && sceNetConnect(client, address.data(), address.size()) == 0);
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0);
    const KernelTimeval timeout{0, 50000};
    Require(sceNetSetsockopt(client, 0xffff, 0x1006, &timeout, sizeof(timeout)) == 0);
    char buffer[8]{};
    NetIovec in[] = {{buffer, sizeof(buffer)}};
    NetMsghdr receive{nullptr, 0, in, 1, nullptr, 0, 0};
    const auto start = std::chrono::steady_clock::now();
    Require(Failed(sceNetRecv(client, buffer, sizeof(buffer), 0), 35));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
    Require(Failed(sceNetRecvfrom(client, buffer, sizeof(buffer), 0, nullptr, nullptr), 35));
    Require(Failed(sceNetRecvmsg(client, &receive, 0), 35));
    const char payload[] = "late";
    Require(sceNetSend(accepted, payload, sizeof(payload), 0) == sizeof(payload));
    Require(sceNetRecv(client, buffer, sizeof(buffer), 0) == sizeof(payload) && std::strcmp(buffer, payload) == 0);
    Require(sceNetSend(client, payload, sizeof(payload), 0) == sizeof(payload));
    std::memset(buffer, 0, sizeof(buffer));
    Require(sceNetRecv(accepted, buffer, sizeof(buffer), 0) == sizeof(payload) && std::strcmp(buffer, payload) == 0);
    for (const int descriptor : {client, accepted, listener}) Require(sceNetSocketClose(descriptor) == 0);
}

static void CheckMicrosecondTimeouts(int receiver) {
    const int receiveTimeout = 50000;
    const int sendTimeout = 70000;
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1106, &receiveTimeout, sizeof(receiveTimeout)) == 0);
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1105, &sendTimeout, sizeof(sendTimeout)) == 0);
    for (const auto [option, expected] : {std::array{0x1106, receiveTimeout}, std::array{0x1105, sendTimeout}}) {
        int value = -1;
        std::uint32_t size = sizeof(value);
        Require(sceNetGetsockopt(receiver, 0xffff, option, &value, &size) == 0 && size == sizeof(value) && value == expected);
    }
    char buffer[8]{};
    Require(Failed(sceNetRecv(receiver, buffer, sizeof(buffer), 0), 35));
    const int disabled = 0;
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1106, &disabled, sizeof(disabled)) == 0);
    Require(sceNetSetsockopt(receiver, 0xffff, 0x1105, &disabled, sizeof(disabled)) == 0);
}

static void CheckMessages(int receiver, int sender, const std::array<std::uint8_t, 16>& address) {
    char head[] = "scatter ";
    char tail[] = "gather";
    NetIovec out[] = {{head, 8}, {nullptr, 0}, {tail, 6}};
    std::array<std::uint8_t, 16> destination = address;
    NetMsghdr send{destination.data(), 16, out, 3, nullptr, 0, 0};
    Require(sceNetSendmsg(sender, &send, 0) == 14);

    char first[5]{};
    char second[20]{};
    NetIovec in[] = {{first, sizeof(first)}, {second, sizeof(second)}};
    std::array<std::uint8_t, 28> source{};
    char control[16]{};
    NetMsghdr receive{source.data(), static_cast<std::uint32_t>(source.size()), in, 2, control, sizeof(control), -1};
    Require(sceNetRecvmsg(receiver, &receive, 2) == 14);
    Require(std::memcmp(first, "scatt", 5) == 0 && std::memcmp(second, "er gather", 9) == 0);
    Require(receive.flags == 0 && receive.control_length == 0 && receive.name_length == 16);
    Require(source[0] == 16 && source[1] == 2 && source[4] == 127 && source[7] == 1);
    std::memset(second, 0, sizeof(second));
    receive.iov_length = 1;
    receive.flags = -1;
    Require(sceNetRecvmsg(receiver, &receive, 0) == 5);
    Require(receive.flags == 0x10 && std::memcmp(first, "scatt", 5) == 0 && second[0] == 0);

    NetMsghdr bad = send;
    Require(Failed(sceNetSendmsg(sender, nullptr, 0), 14));
    Require(Failed(sceNetRecvmsg(receiver, nullptr, 0), 14));
    bad.iov_length = -1;
    Require(Failed(sceNetSendmsg(sender, &bad, 0), 40));
    bad.iov_length = 1025;
    Require(Failed(sceNetRecvmsg(receiver, &bad, 0), 40));
    bad.iov_length = 1;
    bad.iov = nullptr;
    Require(Failed(sceNetSendmsg(sender, &bad, 0), 14));
    NetIovec hole[] = {{nullptr, 1}};
    bad.iov = hole;
    Require(Failed(sceNetRecvmsg(receiver, &bad, 0), 14));
    NetIovec huge[] = {{head, 0x7fffffff}, {tail, 1}};
    bad.iov = huge;
    bad.iov_length = 2;
    Require(Failed(sceNetSendmsg(sender, &bad, 0), 22));
    Require(Failed(sceNetRecvmsg(receiver, &receive, 1), 45));
    Require(Failed(sceNetSendmsg(sender, &send, 1), 45));
    Require(Failed(sceNetRecvmsg(12345, &receive, 0), 9));
    Require(Failed(sceNetSendmsg(12345, &send, 0), 9));
    NetMsghdr with_control = send;
    with_control.control = control;
    with_control.control_length = sizeof(control);
    bool threw = false;
    try {
        sceNetSendmsg(sender, &with_control, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

static void CheckOverlappingReceiveVectors(int receiver, int sender, const std::array<std::uint8_t, 16>& address) {
    constexpr std::size_t page = 0x4000;
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, page, 0, 0, &physical) == 0);
    void* blocked = nullptr;
    Require(sceKernelMapDirectMemory(&blocked, page, 3, 0, physical, 0) == 0);
    Require(sceKernelMprotect(blocked, page, 0) == 0);
    char tail[4]{};
    NetIovec vectors[2]{};
    vectors[1] = {tail, sizeof(tail)};
    vectors[0] = {&vectors[1], sizeof(NetIovec)};
    const NetIovec redirect{blocked, sizeof(tail)};
    char payload[sizeof(NetIovec) + sizeof(tail)];
    std::memcpy(payload, &redirect, sizeof(redirect));
    std::memcpy(payload + sizeof(redirect), "tail", sizeof(tail));
    std::array<std::uint8_t, 16> destination = address;
    Require(sceNetSendto(sender, payload, sizeof(payload), 0, destination.data(), destination.size()) == sizeof(payload));
    std::array<std::uint8_t, 16> source{};
    NetMsghdr message{source.data(), static_cast<std::uint32_t>(source.size()), vectors, 2, nullptr, 0, -1};
    Require(sceNetRecvmsg(receiver, &message, 0) == sizeof(payload));
    Require(std::memcmp(tail, "tail", sizeof(tail)) == 0);
    Require(std::memcmp(&vectors[1], &redirect, sizeof(redirect)) == 0);
    Require(message.iov == vectors && message.iov_length == 2 && message.flags == 0 && message.name_length == 16);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(blocked), page) == 0);
    Require(sceKernelReleaseDirectMemory(physical, page) == 0);
}

static void CheckEmptyReceiveVectors(int receiver, int sender, const std::array<std::uint8_t, 16>& address) {
    std::array<std::uint8_t, 16> destination = address;
    Require(sceNetSendto(sender, "x", 1, 0, destination.data(), destination.size()) == 1);
    std::array<std::uint8_t, 16> source{};
    NetMsghdr message{source.data(), static_cast<std::uint32_t>(source.size()), nullptr, 0, nullptr, 0, -1};
    Require(sceNetRecvmsg(receiver, &message, 0) == 0);
    Require(message.iov == nullptr && message.iov_length == 0 && message.flags == 0x10 && message.name_length == 16);
    Require(source[0] == 16 && source[1] == 2 && source[4] == 127 && source[7] == 1);
}

static void CheckEpollWritable() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int receiver = sceNetSocket("epoll-receiver", 2, 2, 17);
    const int sender = sceNetSocket("epoll-sender", 2, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(receiver, address.data(), &size) == 0 && size == address.size());
    const int epoll = sceNetEpollCreate("epoll-writable", 0);
    Require(epoll >= 0);
    NetEpollEvent registration{};
    registration.events = 2;
    registration.ident = static_cast<std::uint64_t>(sender);
    registration.data.u32 = 41;
    Require(sceNetEpollControl(epoll, 1, sender, &registration) == 0);
    NetEpollEvent ready{};
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(ready.events == 2 && ready.ident == static_cast<std::uint64_t>(sender) && ready.data.u32 == 41);
    Require(sceNetEpollControl(epoll, 3, sender, nullptr) == 0);
    const char payload[] = "epoll";
    char peeked[sizeof(payload)]{};
    Require(sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
    Require(sceNetRecv(receiver, peeked, sizeof(peeked), 2) == sizeof(payload));
    registration.events = 3;
    registration.ident = static_cast<std::uint64_t>(receiver);
    Require(sceNetEpollControl(epoll, 1, receiver, &registration) == 0);
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(ready.events == 3 && ready.ident == static_cast<std::uint64_t>(receiver) && ready.data.u32 == 41);
    Require(sceNetEpollDestroy(epoll) == 0);
    Require(sceNetSocketClose(sender) == 0 && sceNetSocketClose(receiver) == 0);
}

static void RequireTimeout(int socket, int option, int expected) {
    KernelTimeval timeout{-1, -1};
    std::uint32_t size = sizeof(timeout);
    Require(sceNetGetsockopt(socket, 0xffff, option, &timeout, &size) == 0 && size == sizeof(timeout));
    int microseconds = -1;
    std::uint32_t length = sizeof(microseconds);
    Require(sceNetGetsockopt(socket, 0xffff, option == 0x1006 ? 0x1106 : 0x1105, &microseconds, &length) == 0 && length == sizeof(microseconds));
    if (timeout.tv_sec * 1000000 + timeout.tv_usec != expected || microseconds != expected) {
        std::fprintf(stderr, "option 0x%x on a libSceNet socket reads back {%lld, %lld} and %d microseconds, expected %d\n",
            option, static_cast<long long>(timeout.tv_sec), static_cast<long long>(timeout.tv_usec), microseconds, expected);
        std::abort();
    }
}

static void CheckKernelTimeouts() {
    const int descriptor = sceNetSocket("kernel-timeouts", 2, 2, 17);
    Require(descriptor >= 0);
    const KernelTimeval receive{1, 250000};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1006, &receive, sizeof(receive)) == 0);
    RequireTimeout(descriptor, 0x1006, 1250000);
    const KernelTimeval send{0, 500000};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1005, &send, sizeof(send)) == 0);
    RequireTimeout(descriptor, 0x1005, 500000);
    const KernelTimeval invalid{0, 1000000};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1006, &invalid, sizeof(invalid)) == -1 && *__error_nid_postfix() == 33);
    const KernelTimeval negative{-1, 0};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1005, &negative, sizeof(negative)) == -1 && *__error_nid_postfix() == 33);
    const int microseconds = 1000;
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1006, &microseconds, sizeof(microseconds)) == -1 && *__error_nid_postfix() == 22);
    RequireTimeout(descriptor, 0x1006, 1250000);
    RequireTimeout(descriptor, 0x1005, 500000);
    Require(Failed(sceNetSetsockopt(descriptor, 0xffff, 0x1006, &microseconds, sizeof(microseconds)), 22));
    Require(Failed(sceNetSetsockopt(descriptor, 0xffff, 0x1006, &invalid, sizeof(invalid)), 33));
    Require(Failed(sceNetSetsockopt(descriptor, 0xffff, 0x1005, &negative, sizeof(negative)), 33));
    RequireTimeout(descriptor, 0x1006, 1250000);
    RequireTimeout(descriptor, 0x1005, 500000);
    int seconds = -1;
    std::uint32_t truncated = sizeof(seconds);
    Require(sceNetGetsockopt(descriptor, 0xffff, 0x1006, &seconds, &truncated) == 0 && truncated == sizeof(seconds) && seconds == 1);
    const KernelTimeval quarter{0, 250000};
    Require(sceNetSetsockopt(descriptor, 0xffff, 0x1006, &quarter, sizeof(quarter)) == 0);
    RequireTimeout(descriptor, 0x1006, 250000);
    Require(sceNetSetsockopt(descriptor, 0xffff, 0x1106, &microseconds, sizeof(microseconds)) == 0);
    RequireTimeout(descriptor, 0x1006, microseconds);
#ifndef _WIN32
    std::array<std::uint8_t, 16> loopback{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(descriptor, loopback.data(), loopback.size()) == 0);
    const KernelTimeval brief{0, 50000};
    Require(setsockopt_nid_postfix(descriptor, 0xffff, 0x1006, &brief, sizeof(brief)) == 0);
    char buffer[8]{};
    const auto start = std::chrono::steady_clock::now();
    Require(Failed(sceNetRecv(descriptor, buffer, sizeof(buffer), 0), 35));
    Require(std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(40));
#endif
    Require(sceNetSocketClose(descriptor) == 0);
}

static void CheckIoctl() {
    const int socket = sceNetSocket("ioctl", 2, 2, 17);
    Require(socket >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(socket, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(socket, address.data(), &size) == 0 && size == address.size());
    for (const int enabled : {1, 0, 7}) {
        int value = enabled;
        Require(sceNetIoctl(socket, 0x8004667e, &value) == 0);
        int nonblocking = -1;
        std::uint32_t length = sizeof(nonblocking);
        Require(sceNetGetsockopt(socket, 0xffff, 0x1200, &nonblocking, &length) == 0 && nonblocking == (enabled != 0 ? 1 : 0));
        Require(fcntl_nid_postfix(socket, 3) == (enabled != 0 ? 2 | 4 : 2));
    }
    int disabled = 0;
    int enabled = 1;
    Require(sceNetIoctl(socket, 0x8004667e, &enabled) == 0);
    char buffer[8]{};
    Require(Failed(sceNetRecv(socket, buffer, sizeof(buffer), 0), 35));
    int readable = -1;
    Require(sceNetIoctl(socket, 0x4004667f, &readable) == 0 && readable == 0);
    const char payload[5] = {'i', 'o', 'c', 't', 'l'};
    Require(sceNetSendto(socket, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
    GuestPollDescriptor entry{socket, 1, 0};
    Require(poll_nid_postfix(&entry, 1, 1000) == 1 && entry.revents == 1);
    Require(sceNetIoctl(socket, 0x4004667f, &readable) == 0 && readable == sizeof(payload));
    Require(sceNetRecv(socket, buffer, sizeof(buffer), 0) == sizeof(payload));
    Require(sceNetIoctl(socket, 0x4004667f, &readable) == 0 && readable == 0);
    Require(Failed(sceNetIoctl(socket, 0x8004667e, nullptr), 14));
    Require(Failed(sceNetIoctl(socket, 0x4004667f, nullptr), 14));
    Require(Failed(sceNetIoctl(socket, 123, &readable), 25));
    Require(Failed(sceNetIoctl(socket, 123, nullptr), 25));
    Require(sceNetSocketClose(socket) == 0);
    Require(Failed(sceNetIoctl(socket, 0x8004667e, &enabled), 9));
    Require(Failed(sceNetIoctl(-1, 0x4004667f, &readable), 9));
    const int random = open_nid_postfix("/dev/urandom", 0, 0);
    Require(random >= 0);
    Require(Failed(sceNetIoctl(random, 0x4004667f, &readable), 9));
    Require(close_nid_postfix(random) == 0);

    const int kernel = socket_nid_postfix(2, 2, 0);
    Require(kernel >= 0);
    std::array<std::uint8_t, 16> kernelAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(kernel, kernelAddress.data(), kernelAddress.size()) == 0);
    Require(sceNetIoctl(kernel, 0x8004667e, &enabled) == 0 && fcntl_nid_postfix(kernel, 3) == (2 | 4));
    Require(sceNetIoctl(kernel, 0x8004667e, &disabled) == 0 && fcntl_nid_postfix(kernel, 3) == 2);
    Require(sceNetIoctl(kernel, 0x4004667f, &readable) == 0 && readable == 0);
    Require(close_nid_postfix(kernel) == 0);
}

static void CheckKernelStatusFlags() {
    const int descriptor = sceNetSocket("kernel-status", 2, 1, 6);
    Require(descriptor >= 0);
    Require(fcntl_nid_postfix(descriptor, 3) == 2);
    Require(fcntl_nid_postfix(descriptor, 4, 2 | 4) == 0);
    Require(fcntl_nid_postfix(descriptor, 3) == (2 | 4));
    int enabled = 0;
    std::uint32_t length = sizeof(enabled);
    Require(sceNetGetsockopt(descriptor, 0xffff, 0x1200, &enabled, &length) == 0 && enabled == 1);
    Require(fcntl_nid_postfix(descriptor, 4, 2) == 0);
    Require(fcntl_nid_postfix(descriptor, 3) == 2);
    length = sizeof(enabled);
    Require(sceNetGetsockopt(descriptor, 0xffff, 0x1200, &enabled, &length) == 0 && enabled == 0);
    *__error_nid_postfix() = 0;
    Require(fcntl_nid_postfix(descriptor, 4, 0x100) == -1 && *__error_nid_postfix() == 45);
    *__error_nid_postfix() = 0;
    Require(fcntl_nid_postfix(descriptor, 1) == -1 && *__error_nid_postfix() == 22);
    Require(close_nid_postfix(descriptor) == 0);
}

static void CheckAddressText(int family, const char* text) {
    std::array<std::uint8_t, 16> address{};
    Require(sceNetInetPton(family, text, address.data()) == 1);
    std::array<char, 64> output{};
    const auto length = static_cast<std::uint32_t>(std::strlen(text));
    for (std::uint32_t size = 0; size <= length; ++size) {
        *sceNetErrnoLoc() = 123;
        Require(sceNetInetNtop(family, address.data(), output.data(), size) == nullptr);
        Require(*sceNetErrnoLoc() == 28);
    }
    output.fill('x');
    *sceNetErrnoLoc() = 123;
    Require(sceNetInetNtop(family, address.data(), output.data(), length + 1) == output.data());
    Require(std::strcmp(output.data(), text) == 0 && output[length + 1] == 'x');
    Require(*sceNetErrnoLoc() == 123);
    Require(sceNetInetNtop(99, address.data(), output.data(), output.size()) == nullptr && *sceNetErrnoLoc() == 47);
    Require(sceNetInetNtop(family, nullptr, output.data(), output.size()) == nullptr && *sceNetErrnoLoc() == 22);
    Require(sceNetInetNtop(family, address.data(), nullptr, output.size()) == nullptr && *sceNetErrnoLoc() == 22);
}

static void CheckInetPtonRejects(int family, const char* text) {
    std::array<std::uint8_t, 16> address{};
    address.fill(0xaa);
    *sceNetErrnoLoc() = 123;
    Require(sceNetInetPton(family, text, address.data()) == 0 && *sceNetErrnoLoc() == 123);
    Require(std::all_of(address.begin(), address.end(), [](std::uint8_t byte) { return byte == 0xaa; }));
}

static void CheckInetPtonWhitespace() {
    for (const char* text : {"1.2.3.4 ", "1.2.3.4\t", "1.2.3.4\n", "1.2.3.4\v", "1.2.3.4\f", "1.2.3.4\r", "1.2.3.4 5", "1.2.3.4 x"}) {
        std::array<std::uint8_t, 16> address{};
        address.fill(0xaa);
        *sceNetErrnoLoc() = 123;
        Require(sceNetInetPton(2, text, address.data()) == 1 && *sceNetErrnoLoc() == 123);
        Require(address[0] == 1 && address[1] == 2 && address[2] == 3 && address[3] == 4 && address[4] == 0xaa);
    }
    for (const char* text : {" 1.2.3.4", "1.2.3.4x", "1.2.3.4.", "1.2.3.4\x01", "1.2.3.4\xa0", "1.2. 3.4", "1.2.3", "1.2.3 ", "01.2.3.4", "256.0.0.1"}) {
        CheckInetPtonRejects(2, text);
    }
    for (const char* text : {"::1 ", "::1\t", " ::1", "::1x", "::ffff:1.2.3.4x", "::ffff:1.2.3.4\x01", "::ffff:01.2.3.4", "::ffff:1.2.3",
                             "::ffff:0x1.2.3.4", "::ffff ", "::ffff:1.2 .3.4"}) {
        CheckInetPtonRejects(28, text);
    }
    for (const char* text : {"::ffff:1.2.3.4 x", "::ffff:1.2.3.4\t", "::ffff:1.2.3.4\nzz", "::ffff:1.2.3.4 "}) {
        std::array<std::uint8_t, 16> address{};
        *sceNetErrnoLoc() = 123;
        Require(sceNetInetPton(28, text, address.data()) == 1 && *sceNetErrnoLoc() == 123);
        Require(address[10] == 0xff && address[11] == 0xff && address[12] == 1 && address[15] == 4);
    }
    std::array<std::uint8_t, 16> full{};
    Require(sceNetInetPton(28, "1:2:3:4:5:6:1.2.3.4 z", full.data()) == 1 && full[1] == 1 && full[11] == 6 && full[12] == 1 && full[15] == 4);
    *sceNetErrnoLoc() = 123;
    Require(sceNetInetPton(2, "1.2.3.4", nullptr) == 1 && *sceNetErrnoLoc() == 123);
    Require(sceNetInetPton(2, "abc", nullptr) == 0 && *sceNetErrnoLoc() == 123);
    Require(sceNetInetPton(28, "::1", nullptr) < 0 && *sceNetErrnoLoc() == 22);
    *sceNetErrnoLoc() = 123;
    Require(sceNetInetPton(28, nullptr, full.data()) < 0 && *sceNetErrnoLoc() == 22);
}

struct AtonCase {
    const char* text;
    std::uint32_t address;
};

static void CheckInetPtonExParts() {
    for (const AtonCase& c : {AtonCase{"0", 0}, AtonCase{"1", 1}, AtonCase{"4294967295", 0xffffffff}, AtonCase{"4294967296", 0},
                              AtonCase{"0xffffffff", 0xffffffff}, AtonCase{"0x100000000", 0}, AtonCase{"1.16777215", 0x01ffffff},
                              AtonCase{"1.2.65535", 0x0102ffff}, AtonCase{"1.2.3.255", 0x010203ff}, AtonCase{"0x7F.1", 0x7f000001},
                              AtonCase{"0X7f.1", 0x7f000001}, AtonCase{"0x", 0}, AtonCase{"0x.1.2.3", 0x00010203}, AtonCase{"0x8.1", 0x08000001}, AtonCase{"0x9", 9}, AtonCase{"1.2.3", 0x01020003},
                              AtonCase{"127.1", 0x7f000001}, AtonCase{"010.0.0.1", 0x08000001}, AtonCase{"0377.1", 0xff000001},
                              AtonCase{"00000000001.2.3.4", 0x01020304}, AtonCase{"1.2.3.4 x", 0x01020304}, AtonCase{"1.2.3.4\tx", 0x01020304},
                              AtonCase{"1.2.3.4\v", 0x01020304}, AtonCase{"1 .2.3.4", 1}, AtonCase{"0.8", 8}, AtonCase{"00.1", 1},
                              AtonCase{"09", 9}, AtonCase{"09.1", 0x09000001}, AtonCase{"1.09", 0x01000009}, AtonCase{"0.09", 9},
                              AtonCase{"09.09", 0x09000009}, AtonCase{"0919", 919}, AtonCase{"0989", 989}, AtonCase{"0998", 998},
                              AtonCase{"1.4294967296", 0x01000000}, AtonCase{"4294967297.1", 0x01000001}, AtonCase{"1.2.3.4294967297", 0x01020301},
                              AtonCase{"18446744073709551617", 1}, AtonCase{"0x1ffffffff", 0xffffffff}, AtonCase{"0x100000001.1", 0x01000001},
                              AtonCase{"1.0xffffff", 0x01ffffff}, AtonCase{"1.2.0xffff", 0x0102ffff}}) {
        std::array<std::uint8_t, 16> address{};
        address.fill(0xaa);
        *sceNetErrnoLoc() = 123;
        Require(sceNetInetPtonEx(2, c.text, address.data(), 0) == 1 && *sceNetErrnoLoc() == 123);
        Require(address[0] == (c.address >> 24) && address[1] == ((c.address >> 16) & 0xff) && address[2] == ((c.address >> 8) & 0xff) &&
                address[3] == (c.address & 0xff) && address[4] == 0xaa);
    }
    for (const char* text : {"", "1.16777216", "1.2.65536", "1.2.3.256", "256.1", "0400.1", "0xg.1", "08", "018", "019", "0199", "00919",
                             "080", "08.1.2.3", "1.08", "0.08", "0919.1", "09x", "1.0x1000000", "1.2.0x10000", "1.2.3.4.5", "1..2.3",
                             ".1.2.3", "1.2.3.", "1.2.3.4x", " 1.2.3.4", "1.2.3.4\x01", "1.2.3.4\xa0", "+1.2.3.4", "-1.2.3.4"}) {
        std::array<std::uint8_t, 16> address{};
        address.fill(0xaa);
        *sceNetErrnoLoc() = 123;
        Require(sceNetInetPtonEx(2, text, address.data(), 0) == 0 && *sceNetErrnoLoc() == 123);
        Require(std::all_of(address.begin(), address.end(), [](std::uint8_t byte) { return byte == 0xaa; }));
    }
    for (const char* text : {"::1 ", "::1x", " ::1", "1.2.3.4", "::1.2.3", "1:2:3:4:5:6:7:8:9"}) {
        std::array<std::uint8_t, 16> address{};
        address.fill(0xaa);
        Require(sceNetInetPtonEx(28, text, address.data(), 0) == 0);
        Require(std::all_of(address.begin(), address.end(), [](std::uint8_t byte) { return byte == 0xaa; }));
    }
}

static void CheckInetPtonEx() {
    std::array<std::uint8_t, 16> address{};
    Require(sceNetInetPtonEx(2, "192.168.1.20", address.data(), 0) == 1);
    Require(address[0] == 192 && address[1] == 168 && address[2] == 1 && address[3] == 20);
    std::array<std::uint8_t, 16> expected{};
    Require(sceNetInetPton(28, "fe80::1:2", expected.data()) == 1);
    address.fill(0);
    Require(sceNetInetPtonEx(28, "fe80::1:2", address.data(), 0) == 1 && address == expected);
    Require(sceNetInetPtonEx(2, "not an address", address.data(), 0) == 0);
    Require(Failed(sceNetInetPtonEx(99, "127.0.0.1", address.data(), 0), 47));
    Require(Failed(sceNetInetPtonEx(2, nullptr, address.data(), 0), 22));
    *sceNetErrnoLoc() = 123;
    Require(sceNetInetPtonEx(2, "127.0.0.1", nullptr, 0) == 1 && *sceNetErrnoLoc() == 123);
    Require(sceNetInetPtonEx(2, "abc", nullptr, 0) == 0 && *sceNetErrnoLoc() == 123);
    for (const int flags : {1, 0x100, -1}) {
        address.fill(0);
        Require(sceNetInetPtonEx(2, "1.2.3", address.data(), flags) == 1);
        Require(address[0] == 1 && address[1] == 2 && address[2] == 0 && address[3] == 3);
    }
}

static void CheckEtherAddress() {
    struct GuardedAddress {
        std::uint8_t before;
        NetEtherAddr address;
        std::uint8_t after;
    };
    static_assert(sizeof(GuardedAddress) == 8);
    const GuardedAddress original{0x12, {{0xa5, 0xa5, 0xa5, 0xa5, 0xa5, 0xa5}}, 0x34};
    for (const char* text : {"00:01:02:0A:0b:0F", "00-01-02-0A-0b-0F", "00.01 02+0A/0b_0F", "00:01:02:0A:0b:0F trailing"}) {
        auto output = original;
        const std::array<std::uint8_t, 6> expected{0, 1, 2, 10, 11, 15};
        *sceNetErrnoLoc() = 33;
        Require(sceNetEtherStrton(text, &output.address) == 0);
        Require(std::memcmp(output.address.data, expected.data(), expected.size()) == 0);
        Require(output.before == original.before && output.after == original.after && *sceNetErrnoLoc() == 33);
    }
    for (unsigned value = 0; value < 256; ++value) {
        NetEtherAddr input{};
        for (unsigned index = 0; index < 6; ++index) input.data[index] = static_cast<std::uint8_t>(value + index * 43);
        char text[18]{};
        Require(sceNetEtherNtostr(&input, text, sizeof(text)) == 0);
        auto output = original;
        Require(sceNetEtherStrton(text, &output.address) == 0);
        Require(std::memcmp(input.data, output.address.data, sizeof(input.data)) == 0);
        Require(output.before == original.before && output.after == original.after);
    }
    const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ:-. +";
    for (std::size_t index = 0; index + 1 < sizeof(digits); ++index) {
        const unsigned digit = index < 36 ? index : index < 62 ? index - 26 : 0x7f;
        for (std::size_t position = 0; position < 2; ++position) {
            std::string text = "01:23:45:67:89:ab";
            text[position] = digits[index];
            const std::array<std::uint8_t, 6> expected{static_cast<std::uint8_t>(position == 0 ? digit * 16 + 1 : digit), 0x23, 0x45, 0x67, 0x89, 0xab};
            auto output = original;
            *sceNetErrnoLoc() = 33;
            Require(sceNetEtherStrton(text.c_str(), &output.address) == 0);
            Require(std::memcmp(output.address.data, expected.data(), expected.size()) == 0);
            Require(output.before == original.before && output.after == original.after && *sceNetErrnoLoc() == 33);
        }
    }
    for (const char* text : {"", "0", "0:1:2:a:B:f", "0-1-2-a-B-f", "0:1-2:a-B:f", "0:1:2:3:4", "0:1:2:3:4:",
             "0:1:2:3:4:5:6", "000:1:2:3:4:5", "0:1:2:3:4:555", ":0:1:2:3:4:5", "0::1:2:3:4:5", "0.1.2.3.4.5",
             "0x0:1:2:3:4:5", "0:1:2:3:4:g", "+0:1:2:3:4:5", " 0:1:2:3:4:5", "0:1:2:3:4:5 ", "0:1:2:3:4:5\n", "001a2bcdeff0"}) {
        auto output = original;
        Require(Failed(sceNetEtherStrton(text, &output.address), 22));
        Require(std::memcmp(&output, &original, sizeof(output)) == 0);
    }
    const std::string valid = "00:11:22:33:44:55";
    std::vector<char> fixed(valid.begin(), valid.end());
    auto fixedOutput = original;
    Require(sceNetEtherStrton(fixed.data(), &fixedOutput.address) == 0);
    const std::array<std::uint8_t, 6> fixedExpected{0, 0x11, 0x22, 0x33, 0x44, 0x55};
    Require(std::memcmp(fixedOutput.address.data, fixedExpected.data(), fixedExpected.size()) == 0);
    Require(fixedOutput.before == original.before && fixedOutput.after == original.after);
    for (std::size_t length = 0; length < valid.size(); ++length) {
        std::vector<char> text(length + 1, '\0');
        std::memcpy(text.data(), valid.data(), length);
        auto output = original;
        Require(Failed(sceNetEtherStrton(text.data(), &output.address), 22));
        Require(std::memcmp(&output, &original, sizeof(output)) == 0);
    }
    for (std::size_t position = 0; position < valid.size(); ++position) {
        auto text = valid;
        text[position] = '\0';
        auto output = original;
        Require(Failed(sceNetEtherStrton(text.c_str(), &output.address), 22));
        Require(std::memcmp(&output, &original, sizeof(output)) == 0);
    }
    auto output = original;
    Require(Failed(sceNetEtherStrton(nullptr, &output.address), 22));
    Require(std::memcmp(&output, &original, sizeof(output)) == 0);
    Require(Failed(sceNetEtherStrton(valid.c_str(), nullptr), 22));
    Require(Failed(sceNetEtherStrton(nullptr, nullptr), 22));
    for (std::size_t position = 0; position < valid.size(); ++position) {
        auto text = valid;
        text[position] = static_cast<char>(0xff);
        output = original;
        bool threw = false;
        try {
            Require(sceNetEtherStrton(text.c_str(), &output.address) == 0);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw == (position % 3 != 2));
        if (threw) Require(std::memcmp(&output, &original, sizeof(output)) == 0);
        else Require(std::memcmp(output.address.data, fixedExpected.data(), fixedExpected.size()) == 0);
        Require(output.before == original.before && output.after == original.after);
    }
}

static void CheckSystemTime() {
    *sceNetErrnoLoc() = 22;
    sceNetGetSystemTime(nullptr);
    Require(*sceNetErrnoLoc() == 22);
    std::array<std::uint64_t, 3> output{0x123456789abcdef0ULL, UINT64_MAX, 0xfedcba9876543210ULL};
    std::uint64_t previous = 0;
    for (int i = 0; i < 64; ++i) {
        output[1] = UINT64_MAX;
        const auto before = sceKernelGetProcessTime();
        sceNetGetSystemTime(&output[1]);
        const auto after = sceKernelGetProcessTime();
        Require(before <= output[1] && output[1] <= after && previous <= output[1]);
        Require(output[0] == 0x123456789abcdef0ULL && output[2] == 0xfedcba9876543210ULL);
        Require(*sceNetErrnoLoc() == 22);
        previous = output[1];
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    sceNetGetSystemTime(&output[1]);
    Require(output[1] > previous);
}

static void CheckUsleep() {
    Require(CaptureStdout([] { Require(sceNetUsleep(0) == 0); }).empty());
    const auto start = std::chrono::steady_clock::now();
    Require(sceNetUsleep(2000) == 0);
    Require(std::chrono::steady_clock::now() - start >= std::chrono::microseconds(2000));
    const std::string printed = CaptureStdout([] {
        *sceNetErrnoLoc() = 0;
        Require(Failed(sceNetUsleep(-1), 22));
        *sceNetErrnoLoc() = 0;
        Require(Failed(sceNetUsleep(INT32_MIN), 22));
    });
    Require(printed == "[LIBNET] usleep -1\n[LIBNET] usleep -2147483648\n");
}

static void CheckEpollIdent() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int receiver = sceNetSocket("epoll-ident", 2, 2, 17);
    const int sender = sceNetSocket("epoll-ident-sender", 2, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(receiver, address.data(), &size) == 0 && size == address.size());
    const int epoll = sceNetEpollCreate("epoll-ident", 0);
    Require(epoll >= 0);
    NetEpollEvent registration{};
    registration.events = 1;
    registration.reserved = 0x5a5a5a5a;
    registration.ident = 0x123456789abcdef0;
    registration.data.u64 = 0x1122334455667788;
    Require(sceNetEpollControl(epoll, 1, receiver, &registration) == 0);
    const char payload[] = "ident";
    Require(sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
    NetEpollEvent ready{};
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(ready.events == 1 && ready.reserved == 0 && ready.ident == static_cast<std::uint64_t>(receiver));
    Require(ready.data.u64 == 0x1122334455667788);
    Require(sceNetEpollDestroy(epoll) == 0);
    Require(sceNetSocketClose(sender) == 0 && sceNetSocketClose(receiver) == 0);
}

static void CheckEpollClosedStream() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int listener = sceNetSocket("closed-stream-listener", 2, 1, 6);
    Require(listener >= 0);
    Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0 && sceNetListen(listener, 1) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(listener, address.data(), &size) == 0);
    const int client = sceNetSocket("closed-stream-client", 2, 1, 6);
    Require(client >= 0 && sceNetConnect(client, address.data(), address.size()) == 0);
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0);
    const int epoll = sceNetEpollCreate("epoll-closed-stream", 0);
    Require(epoll >= 0);
    NetEpollEvent registration{};
    registration.events = 1;
    registration.ident = static_cast<std::uint64_t>(accepted);
    Require(sceNetEpollControl(epoll, 1, accepted, &registration) == 0);
    Require(sceNetSocketClose(client) == 0);
    NetEpollEvent ready{};
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1 && ready.events == 1);
    char byte = 0;
    Require(sceNetRecv(accepted, &byte, 1, 0) == 0);
    registration.events = 3;
    Require(sceNetEpollControl(epoll, 2, accepted, &registration) == 0);
    Require(sceNetEpollWait(epoll, &ready, 1, 0) == 1 && ready.events == 3);
    Require(sceNetShutdown(accepted, 1) == 0);
    registration.events = 1;
    Require(sceNetEpollControl(epoll, 2, accepted, &registration) == 0);
    Require(sceNetEpollWait(epoll, &ready, 1, 0) == 1 && ready.events == (1 | 16));
    Require(sceNetEpollDestroy(epoll) == 0);
    Require(sceNetSocketClose(accepted) == 0 && sceNetSocketClose(listener) == 0);
}

static void CheckTcpNoDelay() {
    const int stream = sceNetSocket("tcp-nodelay", 2, 1, 6);
    const int datagram = sceNetSocket("tcp-nodelay-udp", 2, 2, 17);
    Require(stream >= 0 && datagram >= 0);
    int value = -1;
    std::uint32_t size = sizeof(value);
    Require(sceNetGetsockopt(stream, 6, 1, &value, &size) == 0 && size == sizeof(value) && value == 0);
    const int on = 1;
    const int off = 0;
    Require(sceNetSetsockopt(stream, 6, 1, &on, sizeof(on)) == 0);
    Require(sceNetGetsockopt(stream, 6, 1, &value, &size) == 0 && value == 4);
    Require(setsockopt_nid_postfix(stream, 6, 1, &off, sizeof(off)) == 0);
    Require(sceNetGetsockopt(stream, 6, 1, &value, &size) == 0 && value == 0);
    Require(Failed(sceNetSetsockopt(stream, 6, 2, &on, sizeof(on)), 42));
    Require(Failed(sceNetGetsockopt(stream, 6, 2, &value, &size), 42));
    Require(Failed(sceNetSetsockopt(datagram, 6, 1, &on, sizeof(on)), 22));
    Require(Failed(sceNetGetsockopt(datagram, 6, 1, &value, &size), 22));
    Require(Failed(sceNetSetsockopt(datagram, 6, 2, &on, sizeof(on)), 22));
    Require(sceNetSocketClose(stream) == 0 && sceNetSocketClose(datagram) == 0);
}

static bool ipv6Unavailable = false;

static bool HostIpv6LoopbackUnavailable() {
    const auto hostSocket = ::socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (hostSocket == INVALID_SOCKET) {
        const int error = WSAGetLastError();
        return error == WSAEAFNOSUPPORT || error == WSAEPROTONOSUPPORT;
    }
#else
    if (hostSocket < 0) return errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT;
#endif

    sockaddr_in6 loopback{};
    loopback.sin6_family = AF_INET6;
    loopback.sin6_port = htons(9);
    loopback.sin6_addr = in6addr_loopback;
    const char payload = 0;
    const auto sent = ::sendto(hostSocket, &payload, sizeof(payload), 0,
        reinterpret_cast<const sockaddr*>(&loopback), sizeof(loopback));
#ifdef _WIN32
    const int error = sent < 0 ? WSAGetLastError() : 0;
    closesocket(hostSocket);
    return sent < 0 && (error == WSAEADDRNOTAVAIL || error == WSAENETUNREACH || error == WSAEHOSTUNREACH);
#else
    const int error = sent < 0 ? errno : 0;
    close(hostSocket);
    return sent < 0 && (error == EADDRNOTAVAIL || error == ENETUNREACH || error == EHOSTUNREACH);
#endif
}

static void CheckRejectedAddressText(const char* text) {
    std::array<std::uint8_t, 16> address;
    address.fill(0xa5);
    Require(sceNetInetPton(28, text, address.data()) == 0);
    Require(address[0] == 0xa5 && address[15] == 0xa5);
}

static void CheckLinkLocalAllNodesIpv6() {
    Require(reinterpret_cast<std::uintptr_t>(sce_net_in6addr_linklocal_allnodes) % alignof(std::uint32_t) == 0);
    std::array<std::uint8_t, 16> address{};
    Require(sceNetInetPton(28, "ff02::1", address.data()) == 1);
    Require(std::memcmp(sce_net_in6addr_linklocal_allnodes, address.data(), address.size()) == 0);
    std::array<char, 9> text;
    text.fill('x');
    Require(sceNetInetNtop(28, sce_net_in6addr_linklocal_allnodes, text.data(), 7) == nullptr && *sceNetErrnoLoc() == 28);
    Require(sceNetInetNtop(28, sce_net_in6addr_linklocal_allnodes, text.data(), 8) == text.data());
    Require(std::strcmp(text.data(), "ff02::1") == 0 && text[8] == 'x');
    address.fill(0xa5);
    Require(sceNetInetPton(28, text.data(), address.data()) == 1);
    Require(std::memcmp(sce_net_in6addr_linklocal_allnodes, address.data(), address.size()) == 0);
}

template <std::size_t TSize>
static bool Untouched(const std::array<std::uint8_t, TSize>& bytes, std::uint32_t from) {
    return std::all_of(bytes.begin() + from, bytes.end(), [](auto byte) { return byte == 0xa5; });
}

static void CheckDatagramAddressLengths(int family) {
    if (family == 28 && ipv6Unavailable) return;
    const std::uint32_t addressSize = family == 2 ? 16 : 28;
    std::array<std::uint8_t, 28> destination{};
    destination[0] = static_cast<std::uint8_t>(addressSize);
    destination[1] = static_cast<std::uint8_t>(family);
    if (family == 2) { destination[4] = 127; destination[7] = 1; }
    else destination[23] = 1;
    const int receiver = sceNetSocket("length-receiver", family, 2, 17);
    const int sender = sceNetSocket("length-sender", family, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    Require(sceNetBind_nid_postfix(receiver, destination.data(), addressSize) == 0);
    std::uint32_t size = destination.size();
    Require(sceNetGetsockname(receiver, destination.data(), &size) == 0 && size == addressSize);
    Require(sceNetConnect(sender, destination.data(), addressSize) == 0);
    std::array<std::uint8_t, 28> source{};
    size = source.size();
    Require(sceNetGetsockname(sender, source.data(), &size) == 0 && size == addressSize);
    const char payload[] = "address length";
    for (const std::uint32_t capacity : {0u, 1u, 2u, addressSize - 1, addressSize, 32u, UINT32_MAX}) {
        const auto copied = std::min(capacity, addressSize);
        std::array<std::uint8_t, 32> address;
        address.fill(0xa5);
        size = capacity;
        Require(sceNetGetsockname(receiver, address.data(), &size) == 0 && size == copied);
        Require(std::memcmp(address.data(), destination.data(), copied) == 0 && Untouched(address, copied));
        char received[sizeof(payload)]{};
        Require(sceNetSend(sender, payload, sizeof(payload), 0) == sizeof(payload));
        address.fill(0xa5);
        size = capacity;
        Require(sceNetRecvfrom(receiver, received, sizeof(received), 2, address.data(), &size) == sizeof(payload));
        Require(size == copied && std::memcmp(address.data(), source.data(), copied) == 0 && Untouched(address, copied));
        address.fill(0xa5);
        NetIovec in[] = {{received, sizeof(received)}};
        NetMsghdr message{address.data(), capacity, in, 1, nullptr, 0, -1};
        Require(sceNetRecvmsg(receiver, &message, 0) == sizeof(payload) && std::memcmp(received, payload, sizeof(payload)) == 0);
        Require(message.name_length == copied && std::memcmp(address.data(), source.data(), copied) == 0 && Untouched(address, copied));
    }
    Require(sceNetSocketClose(sender) == 0 && sceNetSocketClose(receiver) == 0);
}

static void CheckStreamAddressLengths() {
    const int listener = sceNetSocket("length-listener", 2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> listening{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(listener, listening.data(), listening.size()) == 0 && sceNetListen(listener, 1) == 0);
    std::uint32_t size = listening.size();
    Require(sceNetGetsockname(listener, listening.data(), &size) == 0 && size == 16);
    const int client = sceNetSocket("length-client", 2, 1, 6);
    Require(client >= 0 && sceNetConnect(client, listening.data(), listening.size()) == 0);
    std::array<std::uint8_t, 16> clientAddress{};
    size = clientAddress.size();
    Require(sceNetGetsockname(client, clientAddress.data(), &size) == 0 && size == 16);
    std::array<std::uint8_t, 28> peer;
    peer.fill(0xa5);
    size = 1;
    const int accepted = sceNetAccept(listener, peer.data(), &size);
    Require(accepted >= 0 && size == 1 && peer[0] == 16 && Untouched(peer, 1));
    for (const std::uint32_t capacity : {0u, 1u, 15u, 16u, 28u, UINT32_MAX}) {
        const auto copied = std::min(capacity, std::uint32_t{16});
        peer.fill(0xa5);
        size = capacity;
        Require(sceNetGetpeername(accepted, peer.data(), &size) == 0 && size == copied);
        Require(std::memcmp(peer.data(), clientAddress.data(), copied) == 0 && Untouched(peer, copied));
    }
    for (const int descriptor : {accepted, client, listener}) Require(sceNetSocketClose(descriptor) == 0);
}

static void CheckKernelSocketOption() {
    const int kernel = socket_nid_postfix(2, 1, 0);
    Require(kernel >= 0);
    const int size = 65536;
    Require(sceNetSetsockopt(kernel, 0xffff, 0x1001, &size, sizeof(size)) == 0);
    const int one = 1;
    Require(sceNetSetsockopt(kernel, 6, 1, &one, sizeof(one)) == 0);
    Require(close_nid_postfix(kernel) == 0);
    Require(Failed(sceNetSetsockopt(kernel, 0xffff, 0x1001, &size, sizeof(size)), 9));
}

static void CheckEpollKernelSocket() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int receiver = socket_nid_postfix(2, 2, 0);
    const int sender = socket_nid_postfix(2, 2, 0);
    const int unpolled = socket_nid_postfix(2, 2, 0);
    Require(receiver >= 0 && sender >= 0 && unpolled >= 0);
    Require(GuestSocketIsOpen_nid_no_patch(receiver) && GuestSocketIsOpen_nid_no_patch(sender));
    Require(bind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    Require(getsockname_nid_postfix(receiver, address.data(), &size) == 0 && size == address.size());
    const int epoll = sceNetEpollCreate("epoll-kernel-socket", 0);
    Require(epoll >= 0);
    NetEpollEvent registration{};
    registration.events = 1;
    registration.data.u32 = 77;
    Require(sceNetEpollControl(epoll, 1, receiver, &registration) == 0);
    Require(Failed(sceNetEpollControl(epoll, 1, receiver, &registration), 48));
    NetEpollEvent ready{};
    Require(sceNetEpollWait(epoll, &ready, 1, 0) == 0);
    const char payload[] = "kernel";
    Require(sendto_nid_postfix(sender, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(ready.events == 1 && ready.ident == static_cast<std::uint64_t>(receiver) && ready.data.u32 == 77);
    char received[sizeof(payload)]{};
    Require(recv_nid_postfix(receiver, received, sizeof(received), 0) == sizeof(payload));
    Require(sceNetEpollWait(epoll, &ready, 1, 0) == 0);
    registration.events = 2;
    registration.data.u32 = 78;
    Require(sceNetEpollControl(epoll, 2, receiver, &registration) == 0);
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(ready.events == 2 && ready.ident == static_cast<std::uint64_t>(receiver) && ready.data.u32 == 78);
    Require(sceNetEpollControl(epoll, 3, receiver, nullptr) == 0);
    Require(Failed(sceNetEpollControl(epoll, 3, receiver, nullptr), 2));
    Require(Failed(sceNetEpollControl(epoll, 2, receiver, &registration), 2));
    Require(sceNetEpollWait(epoll, &ready, 1, 0) == 0);

    Require(sceNetEpollControl(epoll, 1, sender, &registration) == 0);
    Require(sceNetEpollControl(epoll, 1, unpolled, &registration) == 0);
    Require(close_nid_postfix(sender) == 0);
    Require(sceKernelClose(unpolled) == 0);
    Require(sceNetEpollWait(epoll, &ready, 1, 1000) == 0);
    Require(sceNetEpollWait(epoll, &ready, 1, 0) == 0);
    Require(Failed(sceNetEpollControl(epoll, 3, sender, nullptr), 9));
    Require(Failed(sceNetEpollControl(epoll, 1, unpolled, &registration), 9));
    Require(Failed(sceNetEpollControl(epoll, 1, 0x7ffffff0, &registration), 9));
    const int stale = socket_nid_postfix(2, 2, 0);
    Require(stale >= 0);
    registration.events = 1;
    registration.data.u32 = 79;
    Require(sceNetEpollControl(epoll, 1, stale, &registration) == 0);
    Require(sceKernelClose(stale) == 0);
    std::vector<int> spare;
    int reused = -1;
    for (int attempt = 0; attempt < 64 && reused != stale; ++attempt) {
        reused = socket_nid_postfix(2, 2, 0);
        Require(reused >= 0);
        if (reused != stale) spare.push_back(reused);
    }
    Require(reused == stale);
    std::array<std::uint8_t, 16> reusedAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(reused, reusedAddress.data(), reusedAddress.size()) == 0);
    size = reusedAddress.size();
    Require(getsockname_nid_postfix(reused, reusedAddress.data(), &size) == 0 && size == reusedAddress.size());
    const int reusedSender = socket_nid_postfix(2, 2, 0);
    Require(reusedSender >= 0);
    Require(sendto_nid_postfix(reusedSender, payload, sizeof(payload), 0, reusedAddress.data(), reusedAddress.size()) == sizeof(payload));
    Require(sceNetEpollWait(epoll, &ready, 1, 100000) == 0);
    registration.data.u32 = 80;
    Require(sceNetEpollControl(epoll, 1, reused, &registration) == 0);
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(ready.events == 1 && ready.ident == static_cast<std::uint64_t>(reused) && ready.data.u32 == 80);
    Require(close_nid_postfix(reusedSender) == 0);
    Require(close_nid_postfix(reused) == 0);
    for (const int descriptor : spare) Require(close_nid_postfix(descriptor) == 0);
    Require(sceNetEpollDestroy(epoll) == 0);
    Require(close_nid_postfix(receiver) == 0);
}

static void CheckBufferSizes() {
    const int socket = sceNetSocket("buffer-sizes", 2, 2, 17);
    Require(socket >= 0);
    for (const int option : {0x1001, 0x1002}) {
        int value = -1;
        std::uint32_t size = sizeof(value);
        Require(sceNetGetsockopt(socket, 0xffff, option, &value, &size) == 0 && size == sizeof(value) && value > 0);
    }
    for (const auto [option, requested] : {std::array{0x1001, 32768}, std::array{0x1002, 65536}}) {
        Require(sceNetSetsockopt(socket, 0xffff, option, &requested, sizeof(requested)) == 0);
        int value = -1;
        std::uint32_t size = sizeof(value);
        Require(sceNetGetsockopt(socket, 0xffff, option, &value, &size) == 0 && size == sizeof(value) && value == requested);
    }
    Require(sceNetSocketClose(socket) == 0);
}

static void CheckOnesBroadcast() {
    const auto read = [](int socket) {
        int value = -1;
        std::uint32_t size = sizeof(value);
        Require(sceNetGetsockopt(socket, 0xffff, 0x10000, &value, &size) == 0 && size == sizeof(value));
        return value;
    };
    const int socket = sceNetSocket("ones-broadcast", 2, 2, 17);
    const int other = sceNetSocket("ones-broadcast-other", 2, 2, 17);
    Require(socket >= 0 && other >= 0);
    Require(read(socket) == 0);
    for (const int requested : {1, 0, 7}) {
        Require(sceNetSetsockopt(socket, 0xffff, 0x10000, &requested, sizeof(requested)) == 0);
        Require(read(socket) == requested);
    }
    Require(read(other) == 0);
    const std::uint8_t truncated = 1;
    Require(Failed(sceNetSetsockopt(socket, 0xffff, 0x10000, &truncated, sizeof(truncated)), 22));
    Require(read(socket) == 7);
    for (const int descriptor : {socket, other}) Require(sceNetSocketClose(descriptor) == 0);
}

static void CheckUnspecifiedIpv6() {
    std::array<std::uint8_t, 16> unspecified{};
    Require(sceNetInetPton(28, "::", unspecified.data()) == 1);
    Require(std::memcmp(sce_net_in6addr_any, unspecified.data(), unspecified.size()) == 0);
    char text[4] = {'x', 'x', 'x', 'x'};
    Require(sceNetInetNtop(28, sce_net_in6addr_any, text, 3) == text);
    Require(std::strcmp(text, "::") == 0 && text[3] == 'x');

    const int receiver = sceNetSocket("ipv6-any", 28, 2, 17);
    if (receiver < 0 && *sceNetErrnoLoc() == 47) {
        ipv6Unavailable = true;
        return;
    }
    const int sender = sceNetSocket("ipv6-loopback", 28, 2, 17);
    Require(receiver >= 0 && sender >= 0);
    std::array<std::uint8_t, 28> address{28, 28};
    std::memcpy(address.data() + 8, sce_net_in6addr_any, 16);
    Require(sceNetBind_nid_postfix(receiver, address.data(), address.size()) == 0);
    std::uint32_t size = address.size();
    address.fill(0xa5);
    Require(sceNetGetsockname(receiver, address.data(), &size) == 0 && size == address.size());
    Require(address[0] == 28 && address[1] == 28 && (address[2] != 0 || address[3] != 0));
    Require(std::memcmp(address.data() + 8, unspecified.data(), unspecified.size()) == 0);
    Require(sceNetInetPton(28, "::1", address.data() + 8) == 1);

    const char payload[] = "IPv6 wildcard receive";
    const auto sent = sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size());
    if (sent < 0 && (*sceNetErrnoLoc() == 49 || *sceNetErrnoLoc() == 51 || *sceNetErrnoLoc() == 65) && HostIpv6LoopbackUnavailable()) {
        Require(sceNetSocketClose(sender) == 0);
        Require(sceNetSocketClose(receiver) == 0);
        ipv6Unavailable = true;
        return;
    }
    Require(sent == sizeof(payload));
    char received[sizeof(payload)]{};
    std::array<std::uint8_t, 28> peer{};
    size = peer.size();
    Require(sceNetRecvfrom(receiver, received, sizeof(received), 0, peer.data(), &size) == sizeof(received));
    Require(std::memcmp(received, payload, sizeof(payload)) == 0 && size == peer.size() && peer[1] == 28);
    Require(std::memcmp(peer.data() + 8, address.data() + 8, 16) == 0);
    address[0] = 0;
    Require(sceNetSendto(sender, payload, sizeof(payload), 0, address.data(), address.size()) == sizeof(payload));
    Require(sceNetRecvfrom(receiver, received, sizeof(received), 0, nullptr, nullptr) == sizeof(received));
    Require(std::memcmp(sce_net_in6addr_any, unspecified.data(), unspecified.size()) == 0);
    Require(sceNetSocketClose(sender) == 0);
    Require(sceNetSocketClose(receiver) == 0);
}

static void CheckAcceptedNonblocking(int family, bool nonblocking) {
    const auto addressSize = family == 2 ? 16u : 28u;
    std::array<std::uint8_t, 28> address{};
    address[0] = static_cast<std::uint8_t>(addressSize);
    address[1] = static_cast<std::uint8_t>(family);
    if (family == 2) { address[4] = 127; address[7] = 1; }
    else address[23] = 1;
    const int listener = sceNetSocket("accept-mode", family, 1, 6);
    Require(listener >= 0 && sceNetBind_nid_postfix(listener, address.data(), addressSize) == 0);
    Require(sceNetListen(listener, 1) == 0);
    const int mode = nonblocking ? 1 : 0;
    Require(sceNetSetsockopt(listener, 0xffff, 0x1200, &mode, sizeof(mode)) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(listener, address.data(), &size) == 0 && size == addressSize);
    const int client = sceNetSocket("accept-client", family, 1, 6);
    Require(client >= 0 && sceNetConnect(client, address.data(), addressSize) == 0);
    int accepted = -1;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    do {
        accepted = sceNetAccept(listener, nullptr, nullptr);
        if (accepted >= 0) break;
        Require(nonblocking && Failed(accepted, 35));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    Require(accepted >= 0);
    int inherited = -1;
    size = sizeof(inherited);
    Require(sceNetGetsockopt(accepted, 0xffff, 0x1200, &inherited, &size) == 0 && size == sizeof(inherited));
    const int status = fcntl_nid_postfix(accepted, 3);
    std::printf("Accepted IPv%d mode=%d SO_NBIO=%d F_GETFL=%d\n", family == 2 ? 4 : 6, mode, inherited, status);
    std::fflush(stdout);
    Require(inherited == mode && status == (2 | (nonblocking ? 4 : 0)));
    const int changed = 1 - mode;
    Require(sceNetSetsockopt(listener, 0xffff, 0x1200, &changed, sizeof(changed)) == 0);
    size = sizeof(inherited);
    Require(sceNetGetsockopt(accepted, 0xffff, 0x1200, &inherited, &size) == 0 && inherited == mode);
    struct ReceiveResult { std::int64_t bytes; int error; char value; };
    auto receive = std::async(std::launch::async, [accepted] {
        char byte = '?';
        const auto received = sceNetRecv(accepted, &byte, sizeof(byte), 0);
        return ReceiveResult{received, *sceNetErrnoLoc(), byte};
    });
    const bool ready = receive.wait_for(nonblocking ? std::chrono::milliseconds(2000) : std::chrono::milliseconds(50)) == std::future_status::ready;
    const char payload = 'n';
    Require(sceNetSend(client, &payload, sizeof(payload), 0) == sizeof(payload));
    const auto received = receive.get();
    Require(ready == nonblocking);
    if (nonblocking) {
        Require(received.bytes == static_cast<int>(0x80410123) && received.error == 35 && received.value == '?');
        char byte = '?';
        std::int64_t count = -1;
        const auto arrival = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        do {
            count = sceNetRecv(accepted, &byte, sizeof(byte), 0);
            if (count >= 0) break;
            Require(Failed(count, 35));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        } while (std::chrono::steady_clock::now() < arrival);
        Require(count == sizeof(byte) && byte == payload);
    } else {
        Require(received.bytes == sizeof(payload) && received.value == payload);
    }
    for (const int descriptor : {accepted, client, listener}) Require(sceNetSocketClose(descriptor) == 0);
}

static void CheckIpv6SocketsAreIpv6Only() {
    const int inet = sceNetSocket("ipv4-any", 2, 2, 17);
    Require(inet >= 0);
    std::array<std::uint8_t, 16> any{16, 2};
    Require(sceNetBind_nid_postfix(inet, any.data(), any.size()) == 0);
    std::uint32_t size = any.size();
    Require(sceNetGetsockname(inet, any.data(), &size) == 0 && size == any.size());
    const int inet6 = sceNetSocket("ipv6-any-same-port", 28, 2, 17);
    Require(inet6 >= 0);
    std::array<std::uint8_t, 28> any6{28, 28, any[2], any[3]};
    if (sceNetBind_nid_postfix(inet6, any6.data(), any6.size()) != 0) {
        std::fprintf(stderr, "sceNetBind of [::]:%d failed with errno %d while an IPv4 socket holds 0.0.0.0:%d\n",
            (any[2] << 8) | any[3], *sceNetErrnoLoc(), (any[2] << 8) | any[3]);
        std::abort();
    }
    Require(sceNetSocketClose(inet6) == 0);
    Require(sceNetSocketClose(inet) == 0);
}

static void CheckSocketError() {
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    const int listener = sceNetSocket(nullptr, 2, 1, 6);
    Require(listener >= 0 && sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0 && sceNetListen(listener, 1) == 0);
    std::uint32_t size = address.size();
    Require(sceNetGetsockname(listener, address.data(), &size) == 0 && size == address.size());
    const int connected = sceNetSocket(nullptr, 2, 1, 6);
    Require(connected >= 0 && sceNetConnect(connected, address.data(), address.size()) == 0);
    int error = -1;
    size = sizeof(error);
    Require(sceNetGetsockopt(connected, 0xffff, 0x1007, &error, &size) == 0 && size == sizeof(error) && error == 0);
    for (const int descriptor : {connected, listener}) Require(sceNetSocketClose(descriptor) == 0);

    const int refused = sceNetSocket(nullptr, 2, 1, 6);
    const int nonblocking = 1;
    Require(refused >= 0 && sceNetSetsockopt(refused, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    const int started = sceNetConnect(refused, address.data(), address.size());
    Require(started == static_cast<int>(0x80410124) && *sceNetErrnoLoc() == 36);
    error = 0;
    for (int attempt = 0; attempt < 500 && error == 0; ++attempt) {
        size = sizeof(error);
        Require(sceNetGetsockopt(refused, 0xffff, 0x1007, &error, &size) == 0 && size == sizeof(error));
        if (error == 0) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Require(error == 61);
    size = sizeof(error);
    Require(sceNetGetsockopt(refused, 0xffff, 0x1007, &error, &size) == 0 && error == 0);
    Require(sceNetSocketClose(refused) == 0);
}

static void CheckCloseKernelSocket() {
    const int kernel = socket_nid_postfix(2, 1, 0);
    Require(kernel >= 0 && GuestSocketIsOpen_nid_no_patch(kernel));
    Require(sceNetSocketClose(kernel) == 0);
    Require(!GuestSocketIsOpen_nid_no_patch(kernel));
    Require(Failed(sceNetSocketClose(kernel), 9));
}

static void CheckKernelSocketGetOption() {
    const int kernel = socket_nid_postfix(2, 1, 0);
    Require(kernel >= 0);
    const int requested = 65536;
    Require(sceNetSetsockopt(kernel, 0xffff, 0x1001, &requested, sizeof(requested)) == 0);
    int value = 0;
    std::uint32_t size = sizeof(value);
    Require(sceNetGetsockopt(kernel, 0xffff, 0x1001, &value, &size) == 0 && size == sizeof(value) && value > 0);
    int expected = 0;
    std::uint32_t expectedSize = sizeof(expected);
    Require(getsockopt_nid_postfix(kernel, 0xffff, 0x1001, &expected, &expectedSize) == 0 && value == expected);
    const int one = 1;
    Require(sceNetSetsockopt(kernel, 6, 1, &one, sizeof(one)) == 0);
    value = 0;
    size = sizeof(value);
    Require(sceNetGetsockopt(kernel, 6, 1, &value, &size) == 0 && value != 0);
    Require(Failed(sceNetGetsockopt(kernel, 1234, 1, &value, &size), 22));
    Require(close_nid_postfix(kernel) == 0);
    Require(Failed(sceNetGetsockopt(kernel, 0xffff, 0x1001, &value, &size), 9));
}

static std::array<std::uint8_t, 0xa0> RequireSockInfo(int socket, int type, int flags, const std::array<std::uint8_t, 16>& local,
    const std::array<std::uint8_t, 16>& remote) {
    std::array<std::uint8_t, 0x140> records;
    records.fill(0xa5);
    Require(sceNetGetSockInfo(socket, records.data(), 2, 0) == 1);
    std::array<std::uint8_t, 0xa0> expected{};
    std::memcpy(expected.data() + 0x24, &socket, sizeof(socket));
    expected[0x28] = static_cast<std::uint8_t>(type);
    std::memcpy(expected.data() + 0x34, local.data() + 4, 4);
    std::memcpy(expected.data() + 0x38, remote.data() + 4, 4);
    std::memcpy(expected.data() + 0x3c, local.data() + 2, 2);
    std::memcpy(expected.data() + 0x3e, remote.data() + 2, 2);
    std::memcpy(expected.data() + 0x48, &flags, sizeof(flags));
    for (const auto [offset, option] : {std::array{0x64, 0x1002}, std::array{0x68, 0x1001}}) {
        int size = 0;
        std::uint32_t length = sizeof(size);
        Require(sceNetGetsockopt(socket, 0xffff, option, &size, &length) == 0 && size > 0);
        std::memcpy(expected.data() + offset, &size, sizeof(size));
    }
    Require(std::equal(expected.begin(), expected.end(), records.begin()));
    Require(std::all_of(records.begin() + 0xa0, records.end(), [](auto byte) { return byte == 0xa5; }));
    return expected;
}

static std::vector<int> RequireSockInfoList(int flags) {
    const int count = sceNetGetSockInfo(-1, nullptr, 0, flags);
    Require(count >= 0);
    std::vector<std::uint8_t> records(static_cast<std::size_t>(count + 1) * 0xa0, 0xa5);
    Require(sceNetGetSockInfo(-1, records.data(), count + 1, flags) == count);
    Require(std::all_of(records.end() - 0xa0, records.end(), [](auto byte) { return byte == 0xa5; }));
    std::vector<int> sockets;
    for (int index = 0; index < count; ++index) {
        const auto listed = records.begin() + index * 0xa0;
        int socket = 0;
        std::memcpy(&socket, &listed[0x24], sizeof(socket));
        Require(sockets.empty() || socket > sockets.back());
        alignas(NetSockInfo6) std::array<std::uint8_t, 0xa0> single;
        Require(sceNetGetSockInfo(socket, single.data(), 1, 0) == 1 && std::equal(single.begin(), single.end(), listed));
        sockets.push_back(socket);
    }
    const int one = std::min(count, 1);
    std::array<std::uint8_t, 0x140> first;
    first.fill(0xa5);
    Require(sceNetGetSockInfo(-1, first.data(), 1, flags) == one && std::equal(first.begin(), first.begin() + one * 0xa0, records.begin()));
    Require(std::all_of(first.begin() + one * 0xa0, first.end(), [](auto byte) { return byte == 0xa5; }));
    return sockets;
}

static void CheckSockInfo() {
    const auto earlier = RequireSockInfoList(0);
    const std::array<std::uint8_t, 16> unnamed{};
    const int datagram = sceNetSocket("sockinfo-datagram", 2, 2, 17);
    Require(datagram >= 0);
    RequireSockInfo(datagram, 2, 1, unnamed, unnamed);
    std::array<std::uint8_t, 16> datagramAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(datagram, datagramAddress.data(), datagramAddress.size()) == 0);
    std::uint32_t size = datagramAddress.size();
    Require(sceNetGetsockname(datagram, datagramAddress.data(), &size) == 0 && size == 16);
    RequireSockInfo(datagram, 2, 1, datagramAddress, unnamed);
    const int listener = sceNetSocket("sockinfo-listener", 2, 1, 6);
    std::array<std::uint8_t, 16> listening{16, 2, 0, 0, 127, 0, 0, 1};
    Require(listener >= 0 && sceNetBind_nid_postfix(listener, listening.data(), listening.size()) == 0 && sceNetListen(listener, 1) == 0);
    size = listening.size();
    Require(sceNetGetsockname(listener, listening.data(), &size) == 0 && size == 16);
    RequireSockInfo(listener, 1, 1, listening, unnamed);
    const int client = sceNetSocket("sockinfo-client", 2, 1, 6);
    Require(client >= 0 && sceNetConnect(client, listening.data(), listening.size()) == 0);
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0);
    std::array<std::uint8_t, 16> clientAddress{};
    size = clientAddress.size();
    Require(sceNetGetsockname(client, clientAddress.data(), &size) == 0 && size == 16);
    RequireSockInfo(client, 1, 1, clientAddress, listening);
    RequireSockInfo(accepted, 1, 1, listening, clientAddress);
    const char queued[] = "queued";
    constexpr auto queuedLength = static_cast<std::int32_t>(sizeof(queued));
    Require(sceNetSend(client, queued, sizeof(queued), 0) == queuedLength);
    std::array<std::uint8_t, 0xa0> pending{};
    std::int32_t pendingLength = 0;
    for (int attempt = 0; attempt < 500 && pendingLength != queuedLength; ++attempt) {
        Require(sceNetGetSockInfo(accepted, pending.data(), 1, 0) == 1);
        std::memcpy(&pendingLength, pending.data() + 0x2c, sizeof(pendingLength));
        if (pendingLength != queuedLength) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    Require(pendingLength == queuedLength);
    char drained[sizeof(queued)] = {};
    Require(sceNetRecv(accepted, drained, sizeof(drained), 0) == queuedLength && std::memcmp(drained, queued, sizeof(queued)) == 0);
    RequireSockInfo(accepted, 1, 1, listening, clientAddress);
    Require(sceNetConnect(datagram, listening.data(), listening.size()) == 0);
    RequireSockInfo(datagram, 2, 1, datagramAddress, listening);
    const int nonblocking = 1;
    const std::array<int, 2> buffers{65536, 32768};
    Require(sceNetSetsockopt(datagram, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    Require(sceNetSetsockopt(datagram, 0xffff, 0x1002, &buffers[0], sizeof(buffers[0])) == 0);
    Require(sceNetSetsockopt(datagram, 0xffff, 0x1001, &buffers[1], sizeof(buffers[1])) == 0);
    const auto configured = RequireSockInfo(datagram, 2, 0x200001, datagramAddress, listening);
    Require(std::memcmp(configured.data() + 0x64, buffers.data(), sizeof(buffers)) == 0);
    alignas(NetSockInfo6) std::array<std::uint8_t, 0xa0> record;
    record.fill(0xa5);
    Require(Failed(sceNetGetSockInfo(datagram, nullptr, 1, 0), 22));
    Require(Failed(sceNetGetSockInfo(datagram, record.data(), 0, 0), 22));
    Require(Failed(sceNetGetSockInfo(-1, record.data(), 0, 0), 22));
    for (const int flags : {1, 0x41, 0x1000}) {
        Require(Failed(sceNetGetSockInfo(datagram, record.data(), 1, flags), 22));
        Require(Failed(sceNetGetSockInfo(-1, nullptr, 0, flags), 22));
    }
    const int inet6 = sceNetSocket("sockinfo-inet6", 28, 2, 17);
    Require(inet6 >= 0 ? Failed(sceNetGetSockInfo(inet6, record.data(), 1, 0), 47) : *sceNetErrnoLoc() == 47);
    Require(std::all_of(record.begin(), record.end(), [](auto byte) { return byte == 0xa5; }));
    const int kernel = socket_nid_postfix(2, 2, 0);
    std::array<std::uint8_t, 16> kernelAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(kernel >= 0 && bind_nid_postfix(kernel, kernelAddress.data(), kernelAddress.size()) == 0);
    size = kernelAddress.size();
    Require(getsockname_nid_postfix(kernel, kernelAddress.data(), &size) == 0 && size == 16);
    std::array<std::uint8_t, 0xa0> kernelRecord;
    kernelRecord.fill(0xa5);
    Require(sceNetGetSockInfo(kernel, kernelRecord.data(), 1, 0) == 1);
    std::array<std::uint8_t, 0xa0> kernelExpected{};
    std::memcpy(kernelExpected.data() + 0x24, &kernel, sizeof(kernel));
    kernelExpected[0x28] = 2;
    std::memcpy(kernelExpected.data() + 0x34, kernelAddress.data() + 4, 4);
    std::memcpy(kernelExpected.data() + 0x3c, kernelAddress.data() + 2, 2);
    kernelExpected[0x48] = 1;
    std::memcpy(kernelExpected.data() + 0x64, kernelRecord.data() + 0x64, 8);
    Require(std::equal(kernelExpected.begin(), kernelExpected.end(), kernelRecord.begin()));
    std::int32_t kernelBuffers[2] = {};
    std::memcpy(kernelBuffers, kernelRecord.data() + 0x64, sizeof(kernelBuffers));
    Require(kernelBuffers[0] > 0 && kernelBuffers[1] > 0);
    std::vector<int> expected = earlier;
    expected.insert(expected.end(), {datagram, listener, client, accepted, kernel});
    std::sort(expected.begin(), expected.end());
    Require(RequireSockInfoList(0) == expected && RequireSockInfoList(0x40) == expected);
    Require(sceNetGetSockInfo(datagram, record.data(), 1, 0x40) == 1 && std::equal(configured.begin(), configured.end(), record.begin()));
    if (inet6 >= 0) Require(sceNetSocketClose(inet6) == 0);
    for (const int descriptor : {accepted, client, listener, datagram}) Require(sceNetSocketClose(descriptor) == 0);
    Require(Failed(sceNetGetSockInfo(datagram, record.data(), 1, 0), 9));
    Require(close_nid_postfix(kernel) == 0);
    Require(Failed(sceNetGetSockInfo(kernel, record.data(), 1, 0), 9));
    Require(RequireSockInfoList(0) == earlier);
}

static std::array<std::uint8_t, 0xa0> RequireSockInfo6(int socket, int type, bool ipv6, bool nonblock, const std::array<std::uint8_t, 28>& local,
    const std::array<std::uint8_t, 28>& remote, int requestFlags) {
    std::array<std::uint8_t, 0x140> records;
    records.fill(0xa5);
    Require(sceNetGetSockInfo6(socket, reinterpret_cast<NetSockInfo6*>(records.data()), 2, requestFlags) == 1);
    std::array<std::uint8_t, 0xa0> expected{};
    std::memcpy(expected.data() + 0x24, &socket, sizeof(socket));
    expected[0x28] = static_cast<std::uint8_t>(type);
    if (ipv6) {
        std::memcpy(expected.data() + 0x6c, local.data() + 8, 16);
        std::memcpy(expected.data() + 0x7c, remote.data() + 8, 16);
    } else {
        std::memcpy(expected.data() + 0x34, local.data() + 4, 4);
        std::memcpy(expected.data() + 0x38, remote.data() + 4, 4);
    }
    std::memcpy(expected.data() + 0x3c, local.data() + 2, 2);
    std::memcpy(expected.data() + 0x3e, remote.data() + 2, 2);
    const int flags = 1 | (nonblock ? 0x200000 : 0) | (ipv6 ? 0x400000 : 0);
    std::memcpy(expected.data() + 0x48, &flags, sizeof(flags));
    for (const auto [offset, option] : {std::array{0x64, 0x1002}, std::array{0x68, 0x1001}}) {
        int size = 0;
        std::uint32_t length = sizeof(size);
        Require(sceNetGetsockopt(socket, 0xffff, option, &size, &length) == 0 && size > 0);
        std::memcpy(expected.data() + offset, &size, sizeof(size));
    }
    Require(std::equal(expected.begin(), expected.end(), records.begin()));
    Require(std::all_of(records.begin() + 0xa0, records.end(), [](auto byte) { return byte == 0xa5; }));
    return expected;
}

static std::vector<int> RequireSockInfo6List(int flags) {
    const int count = sceNetGetSockInfo6(-1, nullptr, 0, flags);
    Require(count >= 0);
    std::vector<std::uint8_t> records(static_cast<std::size_t>(count + 1) * 0xa0, 0xa5);
    Require(sceNetGetSockInfo6(-1, reinterpret_cast<NetSockInfo6*>(records.data()), count + 1, flags) == count);
    Require(std::all_of(records.end() - 0xa0, records.end(), [](auto byte) { return byte == 0xa5; }));
    std::vector<std::uint8_t> unbounded(records.size(), 0xa5);
    Require(sceNetGetSockInfo6(-1, reinterpret_cast<NetSockInfo6*>(unbounded.data()), INT32_MAX, flags) == count && unbounded == records);
    std::vector<int> sockets;
    for (int index = 0; index < count; ++index) {
        const auto listed = records.begin() + index * 0xa0;
        int socket = 0;
        int status = 0;
        std::memcpy(&socket, &listed[0x24], sizeof(socket));
        std::memcpy(&status, &listed[0x48], sizeof(status));
        Require(sockets.empty() || socket > sockets.back());
        Require((flags & 0x1000) != 0 || (status & 0x400000) != 0);
        alignas(NetSockInfo6) std::array<std::uint8_t, 0xa0> single;
        Require(sceNetGetSockInfo6(socket, reinterpret_cast<NetSockInfo6*>(single.data()), 1, flags) == 1 && std::equal(single.begin(), single.end(), listed));
        sockets.push_back(socket);
    }
    for (const int limit : {1, 2}) {
        const int written = std::min(count, limit);
        std::vector<std::uint8_t> limited(records.size(), 0xa5);
        Require(sceNetGetSockInfo6(-1, reinterpret_cast<NetSockInfo6*>(limited.data()), limit, flags) == written);
        Require(std::equal(limited.begin(), limited.begin() + written * 0xa0, records.begin()));
        Require(std::all_of(limited.begin() + written * 0xa0, limited.end(), [](auto byte) { return byte == 0xa5; }));
    }
    return sockets;
}

static std::vector<int> WithSockets(std::vector<int> sockets, std::initializer_list<int> added) {
    sockets.insert(sockets.end(), added);
    std::sort(sockets.begin(), sockets.end());
    return sockets;
}

static void CheckSockInfo6() {
    const auto earlier = RequireSockInfo6List(0);
    const auto earlierAll = RequireSockInfo6List(0x1000);
    Require(RequireSockInfo6List(0x40) == earlier && RequireSockInfo6List(0x1040) == earlierAll);
    Require(std::includes(earlierAll.begin(), earlierAll.end(), earlier.begin(), earlier.end()));

    const std::array<std::uint8_t, 28> unnamed{};
    const int datagram = sceNetSocket("sockinfo6-inet", 2, 2, 17);
    Require(datagram >= 0);
    std::array<std::uint8_t, 28> inetAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(datagram, inetAddress.data(), 16) == 0);
    std::uint32_t size = 16;
    Require(sceNetGetsockname(datagram, inetAddress.data(), &size) == 0 && size == 16);
    const int nonblocking = 1;
    Require(sceNetSetsockopt(datagram, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    RequireSockInfo6(datagram, 2, false, true, inetAddress, unnamed, 0x1000);
    RequireSockInfo6(datagram, 2, false, true, inetAddress, unnamed, 0x1040);

    alignas(NetSockInfo6) std::array<std::uint8_t, 0xa0> record;
    record.fill(0xa5);
    const auto untouched = [&record] { return std::all_of(record.begin(), record.end(), [](auto byte) { return byte == 0xa5; }); };
    Require(Failed(sceNetGetSockInfo6(datagram, nullptr, 1, 0x1000), 22));
    Require(Failed(sceNetGetSockInfo6(datagram, reinterpret_cast<NetSockInfo6*>(record.data()), 0, 0x1000), 22));
    Require(Failed(sceNetGetSockInfo6(datagram, reinterpret_cast<NetSockInfo6*>(record.data()), -1, 0x1000), 22));
    Require(Failed(sceNetGetSockInfo6(-1, reinterpret_cast<NetSockInfo6*>(record.data()), 0, 0), 22));
    Require(Failed(sceNetGetSockInfo6(-1, reinterpret_cast<NetSockInfo6*>(record.data()), -5, 0x1000), 22));
    for (const int flags : {1, 0x41, 0x80, 0x1001, 0x2000, 0x400000, 0x1048}) {
        Require(Failed(sceNetGetSockInfo6(datagram, reinterpret_cast<NetSockInfo6*>(record.data()), 1, flags), 22));
        Require(Failed(sceNetGetSockInfo6(-1, nullptr, 0, flags), 22));
        Require(Failed(sceNetGetSockInfo6(-1, reinterpret_cast<NetSockInfo6*>(record.data()), 1, flags), 22));
    }
    Require(Failed(sceNetGetSockInfo6(datagram, reinterpret_cast<NetSockInfo6*>(record.data()), 1, 0), 47));
    Require(Failed(sceNetGetSockInfo6(datagram, reinterpret_cast<NetSockInfo6*>(record.data()), 1, 0x40), 47));
    Require(untouched());
    const int closed = sceNetSocket("sockinfo6-closed", 2, 2, 17);
    Require(closed >= 0 && sceNetSocketClose(closed) == 0);
    for (const int flags : {0, 0x40, 0x1000, 0x1040}) {
        Require(Failed(sceNetGetSockInfo6(closed, reinterpret_cast<NetSockInfo6*>(record.data()), 1, flags), 9));
        Require(Failed(sceNetGetSockInfo6(INT32_MAX, reinterpret_cast<NetSockInfo6*>(record.data()), 1, flags), 9));
    }
    Require(untouched());
    Require(RequireSockInfo6List(0) == earlier);
    Require(RequireSockInfo6List(0x1000) == WithSockets(earlierAll, {datagram}));

    if (ipv6Unavailable) {
        Require(sceNetSocketClose(datagram) == 0);
        Require(Failed(sceNetGetSockInfo6(datagram, reinterpret_cast<NetSockInfo6*>(record.data()), 1, 0x1000), 9));
        Require(RequireSockInfo6List(0x1000) == earlierAll);
        return;
    }

    const int udp = sceNetSocket("sockinfo6-datagram", 28, 2, 17);
    Require(udp >= 0);
    RequireSockInfo6(udp, 2, true, false, unnamed, unnamed, 0);
    std::array<std::uint8_t, 28> udpAddress{28, 28};
    udpAddress[23] = 1;
    Require(sceNetBind_nid_postfix(udp, udpAddress.data(), udpAddress.size()) == 0);
    size = udpAddress.size();
    Require(sceNetGetsockname(udp, udpAddress.data(), &size) == 0 && size == udpAddress.size());
    Require(udpAddress[2] != 0 || udpAddress[3] != 0);
    RequireSockInfo6(udp, 2, true, false, udpAddress, unnamed, 0);
    RequireSockInfo6(udp, 2, true, false, udpAddress, unnamed, 0x40);
    RequireSockInfo6(udp, 2, true, false, udpAddress, unnamed, 0x1000);
    RequireSockInfo6(udp, 2, true, false, udpAddress, unnamed, 0x1040);

    const int listener = sceNetSocket("sockinfo6-listener", 28, 1, 6);
    std::array<std::uint8_t, 28> listening{28, 28};
    listening[23] = 1;
    Require(listener >= 0 && sceNetBind_nid_postfix(listener, listening.data(), listening.size()) == 0 && sceNetListen(listener, 1) == 0);
    size = listening.size();
    Require(sceNetGetsockname(listener, listening.data(), &size) == 0 && size == listening.size());
    RequireSockInfo6(listener, 1, true, false, listening, unnamed, 0);
    const int client = sceNetSocket("sockinfo6-client", 28, 1, 6);
    Require(client >= 0 && sceNetConnect(client, listening.data(), listening.size()) == 0);
    const int accepted = sceNetAccept(listener, nullptr, nullptr);
    Require(accepted >= 0);
    std::array<std::uint8_t, 28> clientAddress{};
    size = clientAddress.size();
    Require(sceNetGetsockname(client, clientAddress.data(), &size) == 0 && size == clientAddress.size());
    Require(clientAddress[2] != 0 || clientAddress[3] != 0);
    RequireSockInfo6(client, 1, true, false, clientAddress, listening, 0);
    RequireSockInfo6(accepted, 1, true, false, listening, clientAddress, 0);

    const int peer = sceNetSocket("sockinfo6-peer", 28, 2, 17);
    std::array<std::uint8_t, 28> peerAddress{28, 28};
    peerAddress[23] = 1;
    Require(peer >= 0 && sceNetBind_nid_postfix(peer, peerAddress.data(), peerAddress.size()) == 0);
    size = peerAddress.size();
    Require(sceNetGetsockname(peer, peerAddress.data(), &size) == 0 && size == peerAddress.size());
    Require(sceNetConnect(peer, udpAddress.data(), udpAddress.size()) == 0);
    RequireSockInfo6(peer, 2, true, false, peerAddress, udpAddress, 0);
    const std::array<int, 2> buffers{65536, 32768};
    Require(sceNetSetsockopt(peer, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    Require(sceNetSetsockopt(peer, 0xffff, 0x1002, &buffers[0], sizeof(buffers[0])) == 0);
    Require(sceNetSetsockopt(peer, 0xffff, 0x1001, &buffers[1], sizeof(buffers[1])) == 0);
    const auto configured = RequireSockInfo6(peer, 2, true, true, peerAddress, udpAddress, 0);
    Require(std::memcmp(configured.data() + 0x64, buffers.data(), sizeof(buffers)) == 0);
    Require(std::all_of(configured.begin() + 0x34, configured.begin() + 0x3c, [](auto byte) { return byte == 0; }));
    Require(sceNetGetSockInfo6(peer, reinterpret_cast<NetSockInfo6*>(record.data()), 1, 0x1040) == 1 && std::equal(configured.begin(), configured.end(), record.begin()));

    Require(RequireSockInfo6List(0) == WithSockets(earlier, {udp, listener, client, accepted, peer}));
    Require(RequireSockInfo6List(0x40) == WithSockets(earlier, {udp, listener, client, accepted, peer}));
    Require(RequireSockInfo6List(0x1000) == WithSockets(earlierAll, {datagram, udp, listener, client, accepted, peer}));
    Require(Failed(sceNetGetSockInfo6(datagram, reinterpret_cast<NetSockInfo6*>(record.data()), 1, 0), 47));

    for (const int descriptor : {peer, accepted, client, listener, udp}) Require(sceNetSocketClose(descriptor) == 0);
    Require(Failed(sceNetGetSockInfo6(udp, reinterpret_cast<NetSockInfo6*>(record.data()), 1, 0), 9));
    Require(RequireSockInfo6List(0) == earlier);
    Require(sceNetSocketClose(datagram) == 0);
    Require(RequireSockInfo6List(0x1000) == earlierAll);
}

static void CheckDatagramDisconnect() {
    const int peer = sceNetSocket("disconnect-peer", 2, 2, 17);
    const int other = sceNetSocket("disconnect-other", 2, 2, 17);
    const int dissolved = sceNetSocket("disconnect", 2, 2, 17);
    Require(peer >= 0 && other >= 0 && dissolved >= 0);
    std::array<std::uint8_t, 16> peerAddress{16, 2, 0, 0, 127, 0, 0, 1};
    std::array<std::uint8_t, 16> dissolvedAddress{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(peer, peerAddress.data(), peerAddress.size()) == 0);
    Require(sceNetBind_nid_postfix(dissolved, dissolvedAddress.data(), dissolvedAddress.size()) == 0);
    std::uint32_t size = peerAddress.size();
    Require(sceNetGetsockname(peer, peerAddress.data(), &size) == 0);
    size = dissolvedAddress.size();
    Require(sceNetGetsockname(dissolved, dissolvedAddress.data(), &size) == 0);
    Require(sceNetConnect(dissolved, peerAddress.data(), peerAddress.size()) == 0);
    std::array<std::uint8_t, 16> name{};
    size = name.size();
    Require(sceNetGetpeername(dissolved, name.data(), &size) == 0 && name[2] == peerAddress[2] && name[3] == peerAddress[3]);
    const std::array<std::uint8_t, 16> unspecified{16, 0};
    Require(Failed(sceNetConnect(dissolved, unspecified.data(), unspecified.size()), 47));
    size = name.size();
    Require(Failed(sceNetGetpeername(dissolved, name.data(), &size), 57));
    std::array<std::uint8_t, 16> local{};
    size = local.size();
    Require(sceNetGetsockname(dissolved, local.data(), &size) == 0 && local[2] == dissolvedAddress[2] && local[3] == dissolvedAddress[3]);
#ifndef _WIN32
    Require(local[4] == 0 && local[5] == 0 && local[6] == 0 && local[7] == 0);
#endif
    const int timeout = 2000000;
    Require(sceNetSetsockopt(dissolved, 0xffff, 0x1106, &timeout, sizeof(timeout)) == 0);
    const char unrelated[] = "unrelated sender";
    Require(sceNetSendto(other, unrelated, sizeof(unrelated), 0, dissolvedAddress.data(), dissolvedAddress.size()) == sizeof(unrelated));
    char buffer[32]{};
    Require(sceNetRecvfrom(dissolved, buffer, sizeof(buffer), 0, nullptr, nullptr) == sizeof(unrelated));
    Require(std::strcmp(buffer, unrelated) == 0);
    for (const int descriptor : {peer, other, dissolved}) Require(sceNetSocketClose(descriptor) == 0);
}

int main() {
    CheckEtherAddress();
    const auto epoch = std::chrono::steady_clock::time_point{};
    Require(KernelSocketSelect::DeadlineAfter(epoch, 2, 250000) == epoch + std::chrono::milliseconds(2250));
    Require(KernelSocketSelect::DeadlineAfter(epoch, INT64_MAX, 0) == std::chrono::steady_clock::time_point::max());
    Require(KernelSocketSelect::DeadlineAfter(epoch, 10000000000LL, 0) == std::chrono::steady_clock::time_point::max());
    const auto almostMax = std::chrono::steady_clock::time_point::max() - std::chrono::microseconds(1);
    Require(KernelSocketSelect::DeadlineAfter(almostMax, 0, 1) == std::chrono::steady_clock::time_point::max());
    Require(KernelSocketSelect::DeadlineAfter(almostMax, 0, 2) == std::chrono::steady_clock::time_point::max());
    for (int i = 0; i < 16; ++i) {
        Require(in6addr_any_nid_postfix[i] == 0);
        Require(in6addr_loopback_nid_postfix[i] == (i == 15 ? 1 : 0));
    }
    for (const int flags : {0, 1, -1}) {
        Require(sceNetClearDnsCache(flags) == static_cast<int>(0x804101C8));
        Require(*sceNetErrnoLoc() == 200);
    }
    std::uint32_t randomOutput = 0x12345678;
    Require(Failed(sceNetGetRandom(&randomOutput), 200));
    Require(randomOutput == 0x12345678);
    Require(Failed(sceNetGetRandom(nullptr), 200));
    Require(Failed(sceNetUsleep(0), 200));
    Require(Failed(sceNetUsleep(-1), 200));
    Require(Failed(sceNetShowNetstat(), 200));
    Require(Failed(sceNetShowIfconfig(), 200));
    Require(sceNetInit_nid_postfix() == 0);
    CheckInitParam();
    CheckEpollWritable();
    CheckSocketHostErrors();
    CheckPoolStats();
    CheckIfName();
    CheckRandom();
    CheckByteOrderConversion();
    CheckNetMemory();
    CheckNetstat();
    CheckEmptyEpoll();
    CheckIfFlags();
    CheckRejectedArguments();
    CheckDontWait();
    CheckNoSignal();
    CheckAddressText(2, "127.0.0.1");
    CheckAddressText(2, "255.255.255.255");
    CheckAddressText(28, "::1");
    CheckAddressText(28, "::ffff:0.0.0.1");
    CheckAddressText(28, "::ffff:0:102:304");
    CheckAddressText(28, "fe80::5efe:102:304");
    CheckRejectedAddressText("::01.2.3.4");
    CheckRejectedAddressText("::ffff:01.2.3.4");
    CheckAddressText(28, "1234:5678:9abc:def0:1234:5678:9abc:def0");
    CheckInetPtonWhitespace();
    CheckInetPtonExParts();
    CheckInetPtonEx();
    CheckUsleep();
    CheckSystemTime();
    CheckEpollIdent();
    CheckEpollClosedStream();
    CheckTcpNoDelay();
    CheckLinkLocalAllNodesIpv6();
    CheckUnboundSocketName();
    CheckUnboundDatagramReceive();
    CheckUnboundDatagramShutdown();
    CheckUnboundDatagramLaterAddress();
    CheckUnboundDatagramTimeoutAfterAddress();
    CheckUnspecifiedIpv6();
    if (!ipv6Unavailable) CheckIpv6SocketsAreIpv6Only();
    for (const int family : {2, 28}) {
        if (family == 28 && ipv6Unavailable) continue;
        CheckAcceptedNonblocking(family, false);
        CheckAcceptedNonblocking(family, true);
    }
    CheckDatagramDisconnect();
    CheckCloseKernelSocket();
    CheckKernelSocketGetOption();
    CheckSockInfo();
    CheckSockInfo6();

    const int closed = sceNetSocket("closed-by-kernel", 2, 2, 17);
    Require(closed >= 0);
    const int enabled = 1;
    Require(setsockopt_nid_postfix(closed, 0xffff, 0x20, &enabled, sizeof(enabled)) == 0);
    Require(sceNetSetsockopt(closed, 0xffff, 0x20, &enabled, sizeof(enabled)) == 0);
    Require(sceNetSetsockopt(closed, 0xffff, 0x4, &enabled, sizeof(enabled)) == 0);
    Require(close_nid_postfix(closed) == 0);
    Require(Failed(sceNetSocketClose(closed), 9));
    CheckKernelTimeouts();
    CheckSocketError();
    CheckKernelStatusFlags();
    CheckIoctl();

    const int listener = sceNetSocket(nullptr, 2, 1, 6);
    Require(listener >= 0);
    std::array<std::uint8_t, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(listener, address.data(), address.size()) == 0);
    Require(sceNetListen(listener, 4) == 0);
    std::uint32_t address_size = address.size();
    Require(sceNetGetsockname(listener, address.data(), &address_size) == 0 && address_size == 16);
    Require(address[2] != 0 || address[3] != 0);

    const int client = sceNetSocket(nullptr, 2, 1, 6);
    Require(client >= 0);
    Require(sceNetConnect(client, address.data(), address.size()) == 0);
    std::array<std::uint8_t, 16> peer{};
    address_size = peer.size();
    const int accepted = sceNetAccept(listener, peer.data(), &address_size);
    Require(accepted >= 0 && address_size == 16);
    Require(sceNetConnect(client, address.data(), address.size()) == static_cast<int>(0x80410138) && *sceNetErrnoLoc() == 56);
    const int nonblocking = 1;
    Require(sceNetSetsockopt(accepted, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    char pending = 0;
    Require(sceNetRecv(accepted, &pending, sizeof(pending), 0) == static_cast<int>(0x80410123) && *sceNetErrnoLoc() == 35);
    const int connecting = sceNetSocket(nullptr, 2, 1, 6);
    Require(connecting >= 0);
    Require(sceNetSetsockopt(connecting, 0xffff, 0x1200, &nonblocking, sizeof(nonblocking)) == 0);
    const int started = sceNetConnect(connecting, address.data(), address.size());
    Require(started == 0 || (started == static_cast<int>(0x80410124) && *sceNetErrnoLoc() == 36));
    Require(sceNetSocketClose(connecting) == 0);

    CheckDatagramAddressLengths(2);
    CheckDatagramAddressLengths(28);
    CheckStreamAddressLengths();
    CheckBufferSizes();
    CheckOnesBroadcast();
    CheckKernelSocketOption();
    CheckEpollKernelSocket();

    const int epoll = sceNetEpollCreate("guest-sce-net", 0);
    Require(epoll >= 0);
    NetEpollEvent registration{};
    registration.events = 1;
    registration.ident = static_cast<std::uint64_t>(accepted);
    registration.data.u32 = 77;
    NetEpollEvent ready{};
    auto waiter = std::async(std::launch::async, [&] { return sceNetEpollWait(epoll, &ready, 1, -1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(sceNetEpollControl(epoll, 1, accepted, &registration) == 0);
    const char request[] = "guest tcp loopback";
    char response[sizeof(request)]{};
    Require(sceNetSend(client, request, sizeof(request), 0) == sizeof(request));
    Require(waiter.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    const int ready_count = waiter.get();
    Require(ready_count == 1 && (ready.events & 1) && ready.ident == static_cast<std::uint64_t>(accepted) && ready.data.u32 == 77);
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1);
    Require(sceNetRecv(accepted, response, sizeof(response), 0) == sizeof(response));
    Require(std::strcmp(request, response) == 0);
    char reply[] = "stream reply";
    NetIovec reply_out[] = {{reply, 7}, {reply + 7, sizeof(reply) - 7}};
    NetMsghdr reply_send{nullptr, 0, reply_out, 2, nullptr, 0, 0};
    Require(sceNetSendmsg(accepted, &reply_send, 0) == sizeof(reply));
    char reply_result[sizeof(reply)]{};
    NetIovec reply_in[] = {{reply_result, sizeof(reply_result)}};
    std::array<std::uint8_t, 16> reply_name{};
    NetMsghdr reply_receive{reply_name.data(), 16, reply_in, 1, nullptr, 0, -1};
    Require(sceNetRecvmsg(client, &reply_receive, 0) == sizeof(reply));
    Require(std::strcmp(reply, reply_result) == 0 && reply_receive.flags == 0 && reply_receive.name_length == 0);
    Require(sceNetSend(client, request, sizeof(request), 0) == sizeof(request));
    ready = {};
    Require(sceNetEpollWait(epoll, &ready, 1, 1000000) == 1 && (ready.events & 1) && ready.ident == static_cast<std::uint64_t>(accepted));
    Require(sceNetShutdown(accepted, 1) == 0);
    Require(Failed(sceNetSend(accepted, request, sizeof(request), 0), 32));
    registration.data.u32 = 78;
    Require(sceNetEpollControl(epoll, 2, accepted, &registration) == 0);
    Require(Failed(sceNetEpollControl(epoll, 2, accepted, nullptr), 22));
    Require(sceNetEpollControl(epoll, 3, accepted, nullptr) == 0);
    Require(Failed(sceNetEpollControl(epoll, 3, accepted, nullptr), 2));
    Require(Failed(sceNetEpollControl(epoll, 2, accepted, &registration), 2));
    Require(sceNetEpollDestroy(epoll) == 0);
    Require(sceNetSocketClose(accepted) == 0);
    bool send_failed = false;
    for (int attempt = 0; attempt < 100 && !send_failed; ++attempt) {
        send_failed = sceNetSend(client, request, sizeof(request), 0) < 0;
        if (!send_failed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    Require(send_failed);
    Require(sceNetSocketClose(client) == 0);
    Require(sceNetSocketClose(listener) == 0);
    *__error_nid_postfix() = 0;
    Require(sceNetSocketClose(listener) == static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);
    Require(*__error_nid_postfix() == 9);

    const int udp_receiver = sceNetSocket(nullptr, 2, 2, 17);
    const int udp_sender = sceNetSocket(nullptr, 2, 2, 17);
    Require(udp_receiver >= 0 && udp_sender >= 0 && udp_receiver < 1024 && udp_sender < 1024);
    address = {16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(udp_receiver, address.data(), address.size()) == 0);
    address_size = address.size();
    Require(sceNetGetsockname(udp_receiver, address.data(), &address_size) == 0);
    const char datagram[] = "guest udp loopback";
    char datagram_result[sizeof(datagram)]{};
    Require(sceNetSendto(udp_sender, datagram, sizeof(datagram), 0, address.data(), address.size()) == sizeof(datagram));
    std::array<std::uint8_t, 16> source{};
    address_size = source.size();
    Require(sceNetRecvfrom(udp_receiver, datagram_result, sizeof(datagram_result), 0,
        source.data(), &address_size) == sizeof(datagram_result));
    Require(std::strcmp(datagram, datagram_result) == 0 && source[1] == 2);
    std::array<std::uint8_t, 16> unsized = address;
    unsized[0] = 0;
    Require(sceNetSendto(udp_sender, datagram, sizeof(datagram), 0, unsized.data(), unsized.size()) == sizeof(datagram));
    Require(sceNetRecvfrom(udp_receiver, datagram_result, sizeof(datagram_result), 0, nullptr, nullptr) == sizeof(datagram_result));
    std::uint64_t readable[16]{};
    readable[udp_receiver / 64] |= std::uint64_t{1} << (udp_receiver % 64);
    const std::int64_t poll_now[2]{0, 0};
    Require(select_nid_postfix(udp_receiver + 1, readable, nullptr, nullptr, poll_now) == 0);
    Require(readable[udp_receiver / 64] == 0);
    Require(sceNetSendto(udp_sender, datagram, sizeof(datagram), 0, address.data(), address.size()) == sizeof(datagram));
    readable[udp_receiver / 64] |= std::uint64_t{1} << (udp_receiver % 64);
    const std::int64_t wait_second[2]{1, 0};
    Require(select_nid_postfix(udp_receiver + 1, readable, nullptr, nullptr, wait_second) == 1);
    Require(((readable[udp_receiver / 64] >> (udp_receiver % 64)) & 1u) != 0);
    Require(sceNetRecvfrom(udp_receiver, datagram_result, sizeof(datagram_result), 0, nullptr, nullptr) == sizeof(datagram_result));
    const std::int64_t bad_timeout[2]{0, 1000000};
    Require(select_nid_postfix(udp_receiver + 1, readable, nullptr, nullptr, bad_timeout) == -1 && *__error_nid_postfix() == 22);
    Require(select_nid_postfix(1025, nullptr, nullptr, nullptr, poll_now) == -1 && *__error_nid_postfix() == 22);
    const int random = open_nid_postfix("/dev/urandom", 0, 0);
    Require(random >= 0 && random < 1024);
    std::uint64_t random_readable[16]{};
    std::uint64_t random_urgent[16]{};
    random_readable[random / 64] |= std::uint64_t{1} << (random % 64);
    random_readable[udp_receiver / 64] |= std::uint64_t{1} << (udp_receiver % 64);
    random_urgent[random / 64] |= std::uint64_t{1} << (random % 64);
    Require(select_nid_postfix(std::max(random, udp_receiver) + 1, random_readable, nullptr, random_urgent, nullptr) == 1);
    Require(((random_readable[random / 64] >> (random % 64)) & 1u) != 0);
    Require(((random_readable[udp_receiver / 64] >> (udp_receiver % 64)) & 1u) == 0);
    Require(random_urgent[random / 64] == 0);
    Require(close_nid_postfix(random) == 0);
    const int kernel_socket = socket_nid_postfix(2, 2, 0);
    Require(kernel_socket >= 0 && kernel_socket < 1024 && kernel_socket != udp_receiver && kernel_socket != udp_sender);
    std::array<std::uint8_t, 16> kernel_address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(bind_nid_postfix(kernel_socket, kernel_address.data(), kernel_address.size()) == 0);
    std::uint64_t mixed_writable[16]{};
    mixed_writable[kernel_socket / 64] |= std::uint64_t{1} << (kernel_socket % 64);
    mixed_writable[udp_sender / 64] |= std::uint64_t{1} << (udp_sender % 64);
    Require(select_nid_postfix(std::max(kernel_socket, udp_sender) + 1, nullptr, mixed_writable, nullptr, wait_second) == 2);
    Require(((mixed_writable[kernel_socket / 64] >> (kernel_socket % 64)) & 1u) != 0);
    Require(((mixed_writable[udp_sender / 64] >> (udp_sender % 64)) & 1u) != 0);
    GuestPollDescriptor net_read{udp_receiver, 1, 0};
    Require(poll_nid_postfix(&net_read, 1, 0) == 0 && net_read.revents == 0);
    Require(sceNetSendto(udp_sender, datagram, sizeof(datagram), 0, address.data(), address.size()) == sizeof(datagram));
    Require(poll_nid_postfix(&net_read, 1, 1000) == 1 && net_read.revents == 1);
    Require(sceNetRecvfrom(udp_receiver, datagram_result, sizeof(datagram_result), 0, nullptr, nullptr) == sizeof(datagram_result));
    GuestPollDescriptor mixed_poll[2]{{kernel_socket, 4, 0}, {udp_sender, 4, 0}};
    Require(poll_nid_postfix(mixed_poll, 2, 1000) == 2 && mixed_poll[0].revents == 4 && mixed_poll[1].revents == 4);
    Require(close_nid_postfix(kernel_socket) == 0);
    std::uint64_t stdio_readable[16]{1};
    std::uint64_t stdio_writable[16]{6};
    Require(select_nid_postfix(3, stdio_readable, stdio_writable, nullptr, poll_now) == 2);
    Require(stdio_readable[0] == 0 && stdio_writable[0] == 6);
    stdio_readable[0] = 1;
    Require(select_nid_postfix(1, stdio_readable, nullptr, nullptr, poll_now) == 0 && stdio_readable[0] == 0);
    CheckReceiveTimeout(udp_receiver, udp_sender, address);
    CheckStreamReceiveTimeout();
    CheckMicrosecondTimeouts(udp_receiver);
    CheckMessages(udp_receiver, udp_sender, address);
    CheckOverlappingReceiveVectors(udp_receiver, udp_sender, address);
    CheckEmptyReceiveVectors(udp_receiver, udp_sender, address);
    Require(sceNetSocketClose(udp_sender) == 0);
    Require(sceNetSocketClose(udp_receiver) == 0);

    const int resolver = sceNetResolverCreate("guest-sce-net", 0, 0);
    Require(resolver >= 0);
    int resolver_error = -1;
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    std::array<std::uint8_t, 4> ipv4{};
    ipv4.fill(0xA5);
    {
        ScopedResolverLookupOverride lookupOverride(FailResolverLookup);
        Require(sceNetResolverStartNtoa(resolver, "guest-sce-net.invalid.", ipv4.data(), 5000000, 1, 0) ==
            static_cast<int>(0x804101E1));
    }
    Require(std::all_of(ipv4.begin(), ipv4.end(), [](std::uint8_t byte) { return byte == 0xA5; }));
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 &&
        resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, nullptr, ipv4.data(), 5000000, 1, 0) == static_cast<int>(0x80410116));
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 &&
        resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);
    Require(ipv4[0] == 127);
    std::array<std::uint8_t, 16> resolved6{};
    resolved6.fill(0xA5);
    Require(sceNetResolverStartNtoa6(resolver, "::1", resolved6.data(), 5000000, 1, 0) == 0);
    Require(std::all_of(resolved6.begin(), resolved6.end() - 1, [](std::uint8_t byte) { return byte == 0; }) && resolved6[15] == 1);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    Require(sceNetResolverStartNtoa6(resolver, nullptr, resolved6.data(), 5000000, 1, 0) == static_cast<int>(0x80410116));
    const auto resolved6BeforeFailure = resolved6;
    Require(sceNetResolverStartNtoa6(resolver, "127.0.0.1", resolved6.data(), 5000000, 1, 0) == static_cast<int>(0x804101E1));
    Require(resolved6 == resolved6BeforeFailure);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    alignas(NetResolverInfo) std::array<std::uint8_t, 512> records{};
    records.fill(0xA5);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(records.data()), 5000000, 1, 0) == 0);
    std::int32_t record_family = 0;
    std::int32_t record_count = 0;
    std::int32_t record_count4 = 0;
    std::memcpy(&record_family, records.data() + 16, sizeof(record_family));
    std::memcpy(&record_count, records.data() + 320, sizeof(record_count));
    std::memcpy(&record_count4, records.data() + 324, sizeof(record_count4));
    Require(records[0] == 127 && record_family == 2 && record_count >= 1 && record_count <= 10 && record_count4 == record_count);
    Require(std::all_of(records.begin() + 32 * record_count, records.begin() + 320, [](std::uint8_t byte) { return byte == 0; }));
    Require(std::all_of(records.begin() + 328, records.begin() + 384, [](std::uint8_t byte) { return byte == 0; }));
    Require(std::all_of(records.begin() + 384, records.end(), [](std::uint8_t byte) { return byte == 0xA5; }));
    const auto resolvedRecords = records;
    for (int first = 0; first < record_count; ++first) {
        for (int second = first + 1; second < record_count; ++second) {
            Require(std::memcmp(records.data() + 32 * first, records.data() + 32 * second, 4) != 0);
        }
    }
    const auto resolved = records;
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, nullptr, reinterpret_cast<NetResolverInfo*>(records.data()), 5000000, 1, 0) ==
        static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22 && records == resolved);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", nullptr, 5000000, 1, 0) ==
        static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22);
    {
        ScopedResolverLookupOverride lookupOverride(FailResolverLookup);
        Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "guest-sce-net.invalid.", reinterpret_cast<NetResolverInfo*>(records.data()), 5000000, 1, 0) ==
            static_cast<int>(0x804101E1));
    }
    Require(records == resolvedRecords);

    alignas(NetResolverInfo) std::array<std::uint8_t, 512> plain{};
    plain.fill(0xA5);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(plain.data()), 5000000, 1, 0) == 0);
    Require(plain == resolved);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, nullptr, reinterpret_cast<NetResolverInfo*>(plain.data()), 5000000, 1, 0) ==
        static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22 && plain == resolved);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", nullptr, 5000000, 1, 0) ==
        static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22);
    const auto resolvedPlain = plain;
    {
        ScopedResolverLookupOverride lookupOverride(FailResolverLookup);
        Require(sceNetResolverStartNtoaMultipleRecords(resolver, "guest-sce-net.invalid", reinterpret_cast<NetResolverInfo*>(plain.data()), 5000000, 1, 0) ==
            static_cast<int>(0x804101E1));
    }
    Require(plain == resolvedPlain);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x804101E1));
    for (const int flags : {0x1, 0x10000}) {
        bool threw = false;
        try { sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(plain.data()), 5000000, 1, flags); }
        catch (const std::runtime_error&) { threw = true; }
        Require(threw && plain == resolved);
    }

    const std::array<std::uint8_t, 4> loopback{127, 0, 0, 1};
    char reverse[256]{};
    Require(sceNetResolverAbort(resolver, 0) == 0);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    Require(sceNetResolverAbort(resolver, 1) == 0);
    Require(sceNetResolverAbort(resolver, 1) == 0);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    const int reverse_result = sceNetResolverStartAton(resolver, loopback.data(), reverse, sizeof(reverse), 5000000, 1, 0);
    Require(reverse_result == 0 || reverse_result == static_cast<int>(0x804101E1));
    Require(sceNetResolverStartNtoa(resolver, nullptr, ipv4.data(), 5000000, 1, 0) == static_cast<int>(0x80410116));
    ipv4.fill(0xA5);
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == static_cast<int>(0x80410104) &&
        *sceNetErrnoLoc() == 4 && ipv4[0] == 0xA5);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x80410104));
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0 && ipv4[0] == 127);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    Require(sceNetResolverAbort(resolver, 1) == 0);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(plain.data()), 5000000, 1, 0) ==
        static_cast<int>(0x80410104) && *sceNetErrnoLoc() == 4);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(plain.data()), 5000000, 1, 0) == 0 && plain == resolved);
    Require(sceNetResolverAbort(resolver, 2) == 0);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(records.data()), 5000000, 1, 0) == 0 && records == resolved);
    std::memset(reverse, 0x5A, sizeof(reverse));
    Require(sceNetResolverStartAton(resolver, loopback.data(), reverse, sizeof(reverse), 5000000, 1, 0) ==
        static_cast<int>(0x80410104) && *sceNetErrnoLoc() == 4 && reverse[0] == 0x5A);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x80410104));
    Require(sceNetResolverStartAton(resolver, loopback.data(), reverse, sizeof(reverse), 5000000, 1, 0) == reverse_result);
    Require(sceNetResolverAbort(resolver, 3) == 0);
    Require(sceNetResolverStartAton(resolver, loopback.data(), reverse, sizeof(reverse), 5000000, 1, 0) ==
        static_cast<int>(0x80410104) && *sceNetErrnoLoc() == 4);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(records.data()), 5000000, 1, 0) ==
        static_cast<int>(0x80410104) && *sceNetErrnoLoc() == 4 && records == resolved);
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);
    for (const int flags : {0x4, -1}) {
        bool threw = false;
        try { sceNetResolverAbort(resolver, flags); } catch (const std::runtime_error&) { threw = true; }
        Require(threw);
    }
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0);

    std::array<std::uint8_t, 16> loopback6{};
    loopback6[15] = 1;
    std::memset(reverse, 0x5A, sizeof(reverse));
    const int reverse6_result = sceNetResolverStartAton6(resolver, loopback6.data(), reverse, sizeof(reverse), 5000000, 1, 0);
    Require(reverse6_result == 0 || reverse6_result == static_cast<int>(0x804101E1));
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == reverse6_result);
    char host_reverse6[256]{};
    sockaddr_in6 host_loopback6{};
    host_loopback6.sin6_family = AF_INET6;
    std::memcpy(&host_loopback6.sin6_addr, loopback6.data(), loopback6.size());
    const bool host_named6 = ::getnameinfo(reinterpret_cast<const sockaddr*>(&host_loopback6), sizeof(host_loopback6),
        host_reverse6, sizeof(host_reverse6), nullptr, 0, NI_NAMEREQD) == 0;
    Require(host_named6 == (reverse6_result == 0));
    if (reverse6_result == 0) {
        Require(std::strcmp(reverse, host_reverse6) == 0);
        const auto name_length = static_cast<int>(std::strlen(host_reverse6));
        char exact[256];
        std::memset(exact, 0x5A, sizeof(exact));
        Require(sceNetResolverStartAton6(resolver, loopback6.data(), exact, name_length + 1, 5000000, 1, 0) == 0 &&
            std::strcmp(exact, host_reverse6) == 0 && static_cast<unsigned char>(exact[name_length + 1]) == 0x5A);
        std::memset(exact, 0x5A, sizeof(exact));
        Require(sceNetResolverStartAton6(resolver, loopback6.data(), exact, name_length, 5000000, 1, 0) ==
            static_cast<int>(0x804101E1) && static_cast<unsigned char>(exact[0]) == 0x5A);
    } else {
        Require(static_cast<unsigned char>(reverse[0]) == 0x5A);
    }
    std::memset(reverse, 0x5A, sizeof(reverse));
    Require(sceNetResolverAbort(resolver, 2) == 0);
    Require(sceNetResolverStartAton6(resolver, loopback6.data(), reverse, sizeof(reverse), 5000000, 1, 0) ==
        static_cast<int>(0x80410104) && *sceNetErrnoLoc() == 4 && static_cast<unsigned char>(reverse[0]) == 0x5A);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == static_cast<int>(0x80410104));
    Require(sceNetResolverAbort(resolver, 1) == 0);
    Require(sceNetResolverStartAton6(resolver, loopback6.data(), reverse, sizeof(reverse), 5000000, 1, 0) == reverse6_result);
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == static_cast<int>(0x80410104));
    Require(sceNetResolverStartAton6(resolver, nullptr, reverse, sizeof(reverse), 5000000, 1, 0) == static_cast<int>(0x80410116) &&
        *sceNetErrnoLoc() == 22);
    Require(sceNetResolverStartAton6(resolver, loopback6.data(), nullptr, sizeof(reverse), 5000000, 1, 0) == static_cast<int>(0x80410116));
    Require(sceNetResolverStartAton6(resolver, loopback6.data(), reverse, 0, 5000000, 1, 0) == static_cast<int>(0x80410116));
    Require(sceNetResolverStartAton6(resolver + 1000, loopback6.data(), reverse, sizeof(reverse), 5000000, 1, 0) ==
        static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);
    for (const int flags : {0x1, 0x10000, -1}) {
        bool threw = false;
        try { sceNetResolverStartAton6(resolver, loopback6.data(), reverse, sizeof(reverse), 5000000, 1, flags); }
        catch (const std::runtime_error&) { threw = true; }
        Require(threw);
    }

    const int other = sceNetResolverCreate("guest-sce-net-other", 0, 0);
    Require(other >= 0 && other != resolver);
    Require(sceNetResolverAbort(other, 3) == 0);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(plain.data()), 5000000, 1, 0) == 0);
    Require(sceNetResolverDestroy(other) == 0);
    Require(sceNetResolverAbort(other, 0) == static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);
    Require(sceNetResolverAbort(-1, 1) == static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);
    Require(sceNetResolverGetError(resolver, nullptr) == static_cast<int>(0x80410116) && *sceNetErrnoLoc() == 22);
    Require(sceNetClearDnsCache(0) == 0);
    ipv4.fill(0xA5);
    Require(sceNetResolverStartNtoa(resolver, "localhost", ipv4.data(), 5000000, 1, 0) == 0 && ipv4[0] == 127);
    Require(sceNetResolverGetError(resolver, &resolver_error) == 0 && resolver_error == 0);
    for (const int flags : {1, 2, 0x10000, -1}) {
        Require(sceNetClearDnsCache(flags) == static_cast<int>(0x80410116));
        Require(*sceNetErrnoLoc() == 22);
    }
    Require(sceNetResolverDestroy(resolver) == 0);
    resolver_error = -1;
    Require(sceNetResolverGetError(resolver, &resolver_error) == static_cast<int>(0x80410109) &&
        *sceNetErrnoLoc() == 9 && resolver_error == -1);
    Require(sceNetResolverStartNtoaMultipleRecordsEx(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(records.data()), 5000000, 1, 0) ==
        static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9 && records == resolved);
    Require(sceNetResolverStartNtoaMultipleRecords(resolver, "localhost", reinterpret_cast<NetResolverInfo*>(plain.data()), 5000000, 1, 0) ==
        static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9 && plain == resolved);
    Require(sceNetResolverAbort(resolver, 1) == static_cast<int>(0x80410109) && *sceNetErrnoLoc() == 9);

    std::array<std::uint8_t, 16> ipv6{};
    Require(sceNetInetPton(28, "::1", ipv6.data()) == 1);
    char text[64]{};
    Require(sceNetInetNtop(28, ipv6.data(), text, sizeof(text)) == text);
    Require(std::strcmp(text, "::1") == 0);

    CheckStreamSender(2);
    if (!ipv6Unavailable) CheckStreamSender(28);

    int state = -1;
    Require(sceNetCtlInit() == 0);
    Require(sceNetCtlGetState(&state) == 0);
    Require(state == 0 || state == 3);
    Require(sceNetCtlGetIfStat(nullptr) == static_cast<int>(0x80412107));
    {
        const char* counterPath = "guest_sce_net_byte_counter.txt";
        std::FILE* counter = std::fopen(counterPath, "w");
        Require(counter != nullptr);
        Require(std::fputs("1099511627776", counter) >= 0);
        Require(std::fclose(counter) == 0);
        Require(NetCtlReadByteCounter_nid_no_patch(counterPath) == 1099511627776ull);
        Require(std::remove(counterPath) == 0);
        bool missing = false;
        try {
            NetCtlReadByteCounter_nid_no_patch(counterPath);
        } catch (const std::runtime_error&) {
            missing = true;
        }
        Require(missing);
    }
    {
        std::array<std::uint32_t, 3> dns{};
        dns.fill(0xA5A5A5A5u);
        const int count = sceNetGetDnsInfo(reinterpret_cast<NetDnsInfo*>(dns.data()), 0);
        Require(count >= 0 && count <= 2 && dns[2] == 0xA5A5A5A5u);
        Require(count == 0 || state == 3);
        for (int index = count; index < 2; ++index) Require(dns[index] == 0);
        for (int index = 0; index < 2; ++index) {
            NetCtlInfo info{};
            const int result = sceNetCtlGetInfo(17 + index, &info);
            if (result != 0) {
                Require(result == static_cast<int>(0x80412108) && count == 0);
                continue;
            }
            const auto* bytes = reinterpret_cast<const std::uint8_t*>(&dns[index]);
            char text[16] = {};
            std::snprintf(text, sizeof(text), "%u.%u.%u.%u", static_cast<unsigned>(bytes[0]), static_cast<unsigned>(bytes[1]), static_cast<unsigned>(bytes[2]), static_cast<unsigned>(bytes[3]));
            Require(std::strcmp(text, reinterpret_cast<const char*>(info.opaque)) == 0);
        }
        for (const int flags : {0x1, -1}) {
            const auto before = dns;
            Require(Failed(sceNetGetDnsInfo(reinterpret_cast<NetDnsInfo*>(dns.data()), flags), 22));
            Require(dns == before);
        }
        Require(Failed(sceNetGetDnsInfo(nullptr, 0), 22));
    }
    NetCtlIfStat first{};
    first.device = 7;
    first.reserved[7] = 7;
    const int statResult = sceNetCtlGetIfStat(&first);
    Require(statResult == (state == 3 ? 0 : static_cast<int>(0x80412108)));
    if (statResult == 0) {
        Require(first.device == 0 && first.reserved[7] == 0);
        NetCtlIfStat second{};
        Require(sceNetCtlGetIfStat(&second) == 0);
        Require(second.tx_bytes >= first.tx_bytes && second.rx_bytes >= first.rx_bytes);
    }
    {
        int cid = -1;
        Require(sceNetCtlRegisterCallbackV6(nullptr, nullptr, &cid) == static_cast<int>(0x80412107));
        Require(sceNetCtlRegisterCallbackV6(IgnoreNetCtlEvent, nullptr, nullptr) == static_cast<int>(0x80412107));
        int cids[8] = {};
        for (int i = 0; i < 8; ++i) {
            Require(sceNetCtlRegisterCallbackV6(IgnoreNetCtlEvent, nullptr, &cids[i]) == 0);
            Require(cids[i] == i);
        }
        Require(sceNetCtlRegisterCallbackV6(IgnoreNetCtlEvent, nullptr, &cid) == static_cast<int>(0x80412103));
        Require(sceNetCtlUnregisterCallbackV6(-1) == static_cast<int>(0x80412105));
        Require(sceNetCtlUnregisterCallbackV6(8) == static_cast<int>(0x80412105));
        Require(sceNetCtlUnregisterCallbackV6(3) == 0);
        Require(sceNetCtlRegisterCallbackV6(IgnoreNetCtlEvent, nullptr, &cid) == 0);
        Require(cid == 3);
        for (int i = 0; i < 8; ++i) {
            Require(sceNetCtlUnregisterCallbackV6(i) == 0);
        }
    }
    {
        int cid = -1;
        Require(sceNetCtlRegisterCallback(nullptr, nullptr, &cid) == static_cast<int>(0x80412107));
        Require(sceNetCtlRegisterCallback(IgnoreNetCtlEvent, nullptr, nullptr) == static_cast<int>(0x80412107));
        int cids[8] = {};
        for (int i = 0; i < 8; ++i) {
            Require(sceNetCtlRegisterCallback(IgnoreNetCtlEvent, nullptr, &cids[i]) == 0);
            Require(cids[i] == i);
        }
        Require(sceNetCtlRegisterCallback(IgnoreNetCtlEvent, nullptr, &cid) == static_cast<int>(0x80412103));
        Require(sceNetCtlRegisterCallbackV6(IgnoreNetCtlEvent, nullptr, &cid) == 0);
        Require(sceNetCtlUnregisterCallback(-1) == static_cast<int>(0x80412105));
        Require(sceNetCtlUnregisterCallback(8) == static_cast<int>(0x80412105));
        Require(sceNetCtlUnregisterCallback(3) == 0);
        Require(sceNetCtlRegisterCallback(IgnoreNetCtlEvent, nullptr, &cid) == 0);
        Require(cid == 3);
        for (int i = 0; i < 8; ++i) {
            Require(sceNetCtlUnregisterCallback(i) == 0);
        }
        Require(sceNetCtlUnregisterCallbackV6(0) == 0);
    }
    {
        int cid = -1;
        Require(sceNetCtlRegisterCallbackForNpToolkit(nullptr, nullptr, &cid) == static_cast<int>(0x80412107));
        Require(sceNetCtlRegisterCallbackForNpToolkit(IgnoreNetCtlEvent, nullptr, nullptr) == static_cast<int>(0x80412107));
        int cids[8] = {};
        for (int i = 0; i < 8; ++i) {
            Require(sceNetCtlRegisterCallbackForNpToolkit(IgnoreNetCtlEvent, nullptr, &cids[i]) == 0);
            Require(cids[i] == i);
        }
        Require(sceNetCtlRegisterCallbackForNpToolkit(IgnoreNetCtlEvent, nullptr, &cid) == static_cast<int>(0x80412103));
        Require(sceNetCtlRegisterCallback(IgnoreNetCtlEvent, nullptr, &cid) == 0 && cid == 0);
        Require(sceNetCtlUnregisterCallback(0) == 0);
        Require(sceNetCtlUnregisterCallbackForNpToolkit(-1) == static_cast<int>(0x80412105));
        Require(sceNetCtlUnregisterCallbackForNpToolkit(8) == static_cast<int>(0x80412105));
        Require(sceNetCtlUnregisterCallbackForNpToolkit(3) == 0);
        Require(sceNetCtlRegisterCallbackForNpToolkit(IgnoreNetCtlEvent, nullptr, &cid) == 0);
        Require(cid == 3);
        for (int i = 0; i < 8; ++i) {
            Require(sceNetCtlUnregisterCallbackForNpToolkit(i) == 0);
        }

        NetCtlEventRecord toolkit;
        NetCtlEventRecord plain;
        Require(sceNetCtlRegisterCallbackForNpToolkit(RecordNetCtlEvent, &toolkit, &cid) == 0 && cid == 0);
        Require(sceNetCtlCheckCallback() == 0 && toolkit.calls == 0);
        Require(sceNetCtlCheckCallbackForNpToolkit() == 0);
        Require(toolkit.calls == 1 && toolkit.arg == &toolkit && (toolkit.event == 1 || toolkit.event == 3));
        int current = -1;
        Require(sceNetCtlGetState(&current) == 0 && toolkit.event == (current == 3 ? 3 : 1));
        Require(sceNetCtlCheckCallbackForNpToolkit() == 0 && toolkit.calls == 1);
        int plainCid = -1;
        Require(sceNetCtlRegisterCallback(RecordNetCtlEvent, &plain, &plainCid) == 0);
        Require(sceNetCtlCheckCallbackForNpToolkit() == 0 && plain.calls == 0);
        Require(sceNetCtlCheckCallback() == 0 && plain.calls == 1 && toolkit.calls == 1);
        Require(sceNetCtlUnregisterCallback(plainCid) == 0);
        Require(sceNetCtlUnregisterCallbackForNpToolkit(cid) == 0);
        Require(sceNetCtlRegisterCallbackForNpToolkit(RecordNetCtlEvent, &toolkit, &cid) == 0);
        Require(sceNetCtlCheckCallbackForNpToolkit() == 0 && toolkit.calls == 2);
        Require(sceNetCtlUnregisterCallbackForNpToolkit(cid) == 0);
    }
    if (ipv6Unavailable) {
        std::puts("skipped the IPv6 socket checks, the host has no usable IPv6 loopback");
        return 77;
    }
}

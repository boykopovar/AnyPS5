extern "C" {
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <linux/sockios.h>
#endif
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#ifdef __linux__
#include <sys/uio.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif
#endif
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/PreciseWait.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libkernel/Socket/include/SocketError.hpp"
#include "prx/libkernel/Socket/include/SocketOptions.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libkernel/File/include/DescriptorControl.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/Socket/include/SocketBorrow.hpp"
#include "prx/libkernel/Socket/include/SocketConnect.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstring>
#include <new>
#include <map>
#include <set>
#include <memory>
#include <mutex>
#include <cstdarg>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef __APPLE__
#include <sys/event.h>
#include <sys/param.h>
#endif

namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr auto Invalid = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr auto Invalid = -1;
#endif
int Fail(int error) { *__error_nid_postfix() = error; return -1; }
constexpr int GuestSocketDescriptorLimit = 1024;
constexpr int GuestPeek = 0x2;
constexpr int GuestWaitAll = 0x40;
constexpr int GuestDontWait = 0x80;
constexpr int GuestNoSignal = 0x20000;
constexpr int GuestSendFlags = GuestNoSignal | GuestDontWait;
constexpr int GuestReceiveFlags = GuestPeek | GuestWaitAll | GuestDontWait;
int NativeSendFlags(int flags) {
#ifdef _WIN32
    static_cast<void>(flags);
    return 0;
#else
    return (flags & GuestNoSignal ? MSG_NOSIGNAL : 0) | (flags & GuestDontWait ? MSG_DONTWAIT : 0);
#endif
}
int NativeReceiveFlags(int flags, int type) {
    const int waitAll = type == 1 && (flags & GuestWaitAll) != 0 ? MSG_WAITALL : 0;
#ifdef _WIN32
    return (flags & GuestPeek ? MSG_PEEK : 0) | waitAll;
#else
    return (flags & GuestPeek ? MSG_PEEK : 0) | (flags & GuestDontWait ? MSG_DONTWAIT : 0) | waitAll;
#endif
}
int NativeError() {
#ifdef _WIN32
    return GuestSockets::GuestErrorFromHost(WSAGetLastError());
#else
    return GuestSockets::GuestErrorFromHost(errno);
#endif
}
int ConnectError() {
#ifdef _WIN32
    if (WSAGetLastError() == WSAEWOULDBLOCK) return 36;
#endif
    return NativeError();
}
int SendError(int type) {
#ifdef _WIN32
    const int error = WSAGetLastError();
    if (type == 2 && error == WSAENOTCONN) return 39;
    if (type == 1 && (error == WSAECONNABORTED || error == WSAECONNRESET)) return 32;
#else
    static_cast<void>(type);
#endif
    return NativeError();
}
struct Socket : KernelSocketPoll::NativeOwner {
    NativeSocket value;
    int family;
    int type;
    std::mutex modeMutex;
    GuestSockets::SocketOptionState options;
    bool receiveShut = false;
    bool sendShut = false;
    bool localAddressCleared = false;
    bool connectStarted = false;
#ifdef _WIN32
    bool connecting = false;
    int connectFailure = 0;
#endif
    std::uint32_t inode = 0;
    std::weak_ptr<Socket> peer;
    bool externalPeer = false;
    Socket(NativeSocket value, int family, int type) : value(value), family(family), type(type) {}
    ~Socket() {
        if (value == Invalid) return;
#ifdef _WIN32
        closesocket(value);
#else
        ::close(value);
#endif
    }
};
struct Waker {
    NativeSocket value = Invalid;
    Waker() {
#ifdef _WIN32
        static const int startup = [] { WSADATA data{}; return WSAStartup(MAKEWORD(2, 2), &data); }();
        if (startup) throw std::runtime_error("equeue waker: WSAStartup failed, error " + std::to_string(startup));
#endif
        value = ::socket(AF_INET, SOCK_DGRAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        socklen_t size = sizeof(address);
#ifdef _WIN32
        u_long nonblocking = 1;
        const bool configured = value != Invalid && ioctlsocket(value, FIONBIO, &nonblocking) == 0;
#else
        const bool configured = value != Invalid && ::fcntl(value, F_SETFL, ::fcntl(value, F_GETFL) | O_NONBLOCK) == 0;
#endif
        if (!configured || bind(value, reinterpret_cast<sockaddr*>(&address), size) != 0 ||
            getsockname(value, reinterpret_cast<sockaddr*>(&address), &size) != 0 ||
            connect(value, reinterpret_cast<sockaddr*>(&address), size) != 0) {
            const int error = NativeError();
            Close();
            throw std::runtime_error("equeue waker: loopback socket setup failed, guest errno " + std::to_string(error));
        }
    }
    ~Waker() { Close(); }
    void Close() {
        if (value == Invalid) return;
#ifdef _WIN32
        closesocket(value);
#else
        ::close(value);
#endif
        value = Invalid;
    }
};
Waker& ThreadWaker() {
    struct WakerTag {};
    return HostThreadLocal<Waker, WakerTag>();
}
void DrainWaker(NativeSocket waker) {
    char drained[16];
    while (recv(waker, drained, sizeof(drained), 0) > 0) {}
}
std::mutex socketsMutex;
std::map<int, std::shared_ptr<Socket>> sockets;
std::set<int> closeOnExecDescriptors;
#ifdef _WIN32
int ReserveNumber(NativeSocket) {
    const HANDLE placeholder = ::CreateFileW(L"NUL", 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (placeholder == INVALID_HANDLE_VALUE)
        throw std::runtime_error("socket: opening the NUL descriptor placeholder failed, error " + std::to_string(::GetLastError()));
    const int number = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(placeholder), _O_RDONLY);
    if (number < 0) {
        ::CloseHandle(placeholder);
        return Fail(24);
    }
    if (number >= GuestSocketDescriptorLimit) {
        ::_close(number);
        return Fail(24);
    }
    return number;
}
int CopyNumber(int number, int minimum = 0) {
    if (minimum >= GuestSocketDescriptorLimit) return Fail(24);
    std::vector<int> below;
    int copy = -1;
    int error = 0;
    try {
        while ((copy = ::_dup(number)) >= 0 && copy < minimum) below.push_back(copy);
        if (copy < 0) error = errno == EMFILE ? 24 : 9;
    } catch (const std::bad_alloc&) {
        if (copy >= 0) ::_close(copy);
        copy = -1;
        error = 12;
    }
    for (const int temporary : below) ::_close(temporary);
    if (copy < 0) return Fail(error);
    if (copy >= GuestSocketDescriptorLimit) {
        ::_close(copy);
        return Fail(24);
    }
    return copy;
}
int CopyNumberTo(int number, int target) {
    if (target >= GuestSocketDescriptorLimit) return Fail(24);
    return ::_dup2(number, target) != 0 ? Fail(errno == EMFILE ? 24 : 9) : 0;
}
void ReleaseNumber(int number) {
    ::_close(number);
}
#else
int ReserveNumber(NativeSocket native) {
    const int number = ::fcntl(native, F_DUPFD_CLOEXEC, 0);
    if (number < 0) return Fail(NativeError());
    if (number >= GuestSocketDescriptorLimit) {
        ::close(number);
        return Fail(24);
    }
    return number;
}
int CopyNumber(int number, int minimum = 0) {
    if (minimum >= GuestSocketDescriptorLimit) return Fail(24);
    const int copy = ::fcntl(number, F_DUPFD_CLOEXEC, minimum);
    if (copy < 0) return Fail(NativeError());
    if (copy >= GuestSocketDescriptorLimit) {
        ::close(copy);
        return Fail(24);
    }
    return copy;
}
int CopyNumberTo(int number, int target) {
    if (target >= GuestSocketDescriptorLimit) return Fail(24);
    return ::dup2(number, target) < 0 ? Fail(NativeError()) : 0;
}
void ReleaseNumber(int number) {
    ::close(number);
}
#endif
int Install(std::shared_ptr<Socket> socket) {
    const int number = ReserveNumber(socket->value);
    if (number < 0) return -1;
    try {
        std::lock_guard lock(socketsMutex);
        sockets.insert_or_assign(number, std::move(socket));
    } catch (const std::bad_alloc&) {
        ReleaseNumber(number);
        return Fail(12);
    }
    return number;
}
std::atomic<const KernelSocketPoll::Forwards*> socketForwards{nullptr};
struct Forwarded {
    const KernelSocketPoll::Forwards* forwards = nullptr;
    int family = 0;
};
Forwarded ForwardsFor(int descriptor) {
    const auto* forwards = GuestSockets::IsOpen(descriptor) ? nullptr : socketForwards.load(std::memory_order_acquire);
    const int family = forwards == nullptr ? 0 : forwards->family(descriptor);
    return family == 0 ? Forwarded{} : Forwarded{forwards, family};
}
std::shared_ptr<Socket> Lookup(int descriptor) {
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    if (found != sockets.end()) return found->second;
    Fail(descriptor >= 0 && DescriptorIsOpen_nid_no_patch(descriptor) ? 38 : 9);
    return {};
}
int AddressLengthError(const void* input, std::uint32_t length) {
    if (length > 255) return 63;
    if (length < 2) return 22;
    return input ? 0 : 14;
}
bool Address(int family, const void* input, std::uint32_t length, sockaddr_storage& native, socklen_t& size) {
    if (const int error = AddressLengthError(input, length)) { Fail(error); return false; }
    const auto* bytes = static_cast<const unsigned char*>(input);
    const bool wrongLength = (family == 2 && length != 16) || (family == 28 && length != 28);
    if (wrongLength) { Fail(22); return false; }
    if (family == 2 && bytes[1] != 2) { Fail(47); return false; }
    if (family == 2) {
        auto& v4 = reinterpret_cast<sockaddr_in&>(native);
        v4.sin_family = AF_INET;
        std::memcpy(&v4.sin_port, bytes + 2, 2);
        std::memcpy(&v4.sin_addr, bytes + 4, 4);
        size = sizeof(v4);
        return true;
    }
    if (family == 28 && bytes[1] == 28) {
        auto& v6 = reinterpret_cast<sockaddr_in6&>(native);
        v6.sin6_family = AF_INET6;
        std::memcpy(&v6.sin6_port, bytes + 2, 2);
        std::memcpy(&v6.sin6_flowinfo, bytes + 4, 4);
        std::memcpy(&v6.sin6_addr, bytes + 8, 16);
        std::memcpy(&v6.sin6_scope_id, bytes + 24, 4);
        size = sizeof(v6);
        return true;
    }
    Fail(47);
    return false;
}
int LongMappedDualStack(const Socket& socket, const void* address, std::uint32_t length) {
    if (socket.family != 28 || socket.type != 2 || address == nullptr || length <= 28 || length > 255) return 0;
    const auto* bytes = static_cast<const unsigned char*>(address);
    constexpr unsigned char mapped[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    if (bytes[1] != 28 || std::memcmp(bytes + 8, mapped, sizeof(mapped)) != 0) return 0;
    int ipv6Only = 0;
    socklen_t size = sizeof(ipv6Only);
    if (::getsockopt(socket.value, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<char*>(&ipv6Only), &size)) return Fail(NativeError());
    return ipv6Only ? 0 : 1;
}
std::uint16_t& PortOf(sockaddr_storage& address) {
    return address.ss_family == AF_INET ? reinterpret_cast<sockaddr_in&>(address).sin_port : reinterpret_cast<sockaddr_in6&>(address).sin6_port;
}
bool Listening(const Socket& socket) {
    bool listening = false;
    return GuestSockets::NativeListening(static_cast<std::uintptr_t>(socket.value), listening) && listening;
}
bool PeerConnected(const Socket& socket) {
    sockaddr_storage peer{};
    socklen_t size = sizeof(peer);
    return ::getpeername(socket.value, reinterpret_cast<sockaddr*>(&peer), &size) == 0;
}
bool Multicast(const sockaddr_storage& address) {
    if (address.ss_family == AF_INET) return (reinterpret_cast<const unsigned char*>(&reinterpret_cast<const sockaddr_in&>(address).sin_addr)[0] & 0xf0) == 0xe0;
    return address.ss_family == AF_INET6 && reinterpret_cast<const unsigned char*>(&reinterpret_cast<const sockaddr_in6&>(address).sin6_addr)[0] == 0xff;
}
constexpr std::int64_t NotAnswered = -2;
std::int64_t ZeroPortSend(const Socket& socket, const sockaddr_storage& destination, std::uint64_t length) {
    if (socket.type != 2 || socket.family != 2 || destination.ss_family != AF_INET) return NotAnswered;
    const auto& remote = reinterpret_cast<const sockaddr_in&>(destination);
    if (remote.sin_port != 0) return NotAnswered;
    sockaddr_storage peer{};
    socklen_t peerSize = sizeof(peer);
    if (::getpeername(socket.value, reinterpret_cast<sockaddr*>(&peer), &peerSize) == 0) return NotAnswered;
    sockaddr_in local{};
    socklen_t localSize = sizeof(local);
    const bool bound = ::getsockname(socket.value, reinterpret_cast<sockaddr*>(&local), &localSize) == 0
        && local.sin_port != 0 && local.sin_addr.s_addr != htonl(INADDR_ANY);
    if (bound && remote.sin_addr.s_addr != htonl(INADDR_ANY) && remote.sin_addr.s_addr != htonl(INADDR_BROADCAST)) return static_cast<std::int64_t>(length);
    return Fail(49);
}
int LocalAddress(Socket& socket, sockaddr_storage& address, socklen_t& size) {
    std::unique_lock lock(socket.modeMutex, std::defer_lock);
    if (socket.type == 2) lock.lock();
    if (::getsockname(socket.value, reinterpret_cast<sockaddr*>(&address), &size)) return -1;
    if (socket.type == 2 && socket.localAddressCleared) {
        if (address.ss_family == AF_INET) reinterpret_cast<sockaddr_in&>(address).sin_addr = {};
        else if (address.ss_family == AF_INET6) {
            auto& v6 = reinterpret_cast<sockaddr_in6&>(address);
            v6.sin6_addr = {};
            v6.sin6_scope_id = 0;
        }
    }
    return 0;
}
int Disconnect(Socket& socket) {
    std::lock_guard lock(socket.modeMutex);
    sockaddr_storage peer{};
    socklen_t peerSize = sizeof(peer);
    if (::getpeername(socket.value, reinterpret_cast<sockaddr*>(&peer), &peerSize) != 0) return 0;
    sockaddr_storage local{};
    socklen_t localSize = sizeof(local);
    if (::getsockname(socket.value, reinterpret_cast<sockaddr*>(&local), &localSize) != 0) return Fail(NativeError());
#ifdef _WIN32
    sockaddr_storage unspecified{};
    unspecified.ss_family = local.ss_family;
    if (::connect(socket.value, reinterpret_cast<sockaddr*>(&unspecified), localSize) != 0) return Fail(NativeError());
#elif defined(__APPLE__)
    sockaddr unspecified{};
    unspecified.sa_family = AF_UNSPEC;
    if (::connect(socket.value, &unspecified, sizeof(unspecified)) != 0) {
        const int error = NativeError();
        peerSize = sizeof(peer);
        if (::getpeername(socket.value, reinterpret_cast<sockaddr*>(&peer), &peerSize) == 0 || errno != ENOTCONN) return Fail(error);
    }
#else
    sockaddr unspecified{};
    unspecified.sa_family = AF_UNSPEC;
    if (::connect(socket.value, &unspecified, sizeof(unspecified)) != 0) {
        const int error = NativeError();
        peerSize = sizeof(peer);
        if (::getpeername(socket.value, reinterpret_cast<sockaddr*>(&peer), &peerSize) == 0 || errno != ENOTCONN) return Fail(error);
    }
#endif
    sockaddr_storage current{};
    socklen_t currentSize = sizeof(current);
    if (::getsockname(socket.value, reinterpret_cast<sockaddr*>(&current), &currentSize) != 0) return Fail(NativeError());
    if (PortOf(current) != 0) {
        socket.localAddressCleared = true;
        return 0;
    }
    sockaddr_storage wildcard{};
    wildcard.ss_family = local.ss_family;
    PortOf(wildcard) = PortOf(local);
    if (::bind(socket.value, reinterpret_cast<sockaddr*>(&wildcard), localSize) != 0) return Fail(NativeError());
    socket.localAddressCleared = true;
    return 0;
}
#ifdef _WIN32
void LoopbackForUnspecifiedInet(sockaddr_storage& native) {
    if (native.ss_family == AF_INET) {
        auto& inet = reinterpret_cast<sockaddr_in&>(native);
        if (inet.sin_addr.s_addr == htonl(INADDR_ANY)) inet.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        return;
    }
    if (native.ss_family != AF_INET6) return;
    auto& address = reinterpret_cast<sockaddr_in6&>(native).sin6_addr;
    auto* bytes = address.s6_addr;
    if (IN6_IS_ADDR_V4MAPPED(&address) && (bytes[12] | bytes[13] | bytes[14] | bytes[15]) == 0) {
        bytes[12] = 127;
        bytes[15] = 1;
    }
}
#endif
bool ZeroPortConnect(Socket& socket, const void* address, std::uint32_t length) {
    if (!address || (socket.type != 1 && socket.type != 2) || (socket.family != 2 && socket.family != 28)) return false;
    const auto* bytes = static_cast<const unsigned char*>(address);
    if (length < (socket.family == 2 ? 16u : 28u) || bytes[1] != socket.family || (bytes[2] | bytes[3]) != 0) return false;
    if (socket.type == 2) {
        if (Disconnect(socket) != 0) return true;
    } else {
        sockaddr_storage peer{};
        socklen_t peerSize = sizeof(peer);
        if (::getpeername(socket.value, reinterpret_cast<sockaddr*>(&peer), &peerSize) == 0) return false;
    }
    Fail(49);
    return true;
}
bool GuestReadable(const void* pointer, std::uint64_t length) {
    if (length == 0) return true;
    constexpr std::uint64_t page = 4096;
    const auto* bytes = static_cast<const unsigned char*>(pointer);
    unsigned char value;
    for (std::uint64_t done = 0; done < length; done += page)
        if (!GuestCopyIn_nid_no_patch(&value, bytes + done, 1)) return false;
    return GuestCopyIn_nid_no_patch(&value, bytes + (length - 1), 1);
}
#ifdef _WIN32
constexpr DWORD ReadableProtection = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
constexpr DWORD WritableProtection = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
std::uint64_t GuestPrefix(const void* pointer, std::uint64_t length, DWORD allowed) {
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    std::uint64_t covered = 0;
    while (covered < length) {
        const auto address = start + covered;
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &memory, sizeof(memory)) != sizeof(memory)) break;
        if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 || (memory.Protect & allowed) == 0) break;
        covered += reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize - address;
    }
    return std::min(covered, length);
}
bool SourceFaults(const void* pointer, std::uint64_t length) {
    if (length == 0) return false;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(pointer, static_cast<std::size_t>(length));
    return GuestPrefix(pointer, length, ReadableProtection) != length;
}
bool PendingFits(const Socket& socket, void* buffer, std::uint64_t usable, int flags, int& error) {
    error = 0;
    if (socket.type == 2) {
        sockaddr_storage ignored{};
        socklen_t ignoredSize = sizeof(ignored);
        const auto peeked = ::recvfrom(socket.value, static_cast<char*>(buffer), static_cast<int>(usable),
            NativeReceiveFlags((flags & ~GuestWaitAll) | GuestPeek, socket.type), reinterpret_cast<sockaddr*>(&ignored), &ignoredSize);
        if (peeked >= 0) return true;
        if (WSAGetLastError() == WSAEMSGSIZE) return false;
        error = NativeError();
        return true;
    }
    char probe[1];
    sockaddr_storage ignored{};
    socklen_t ignoredSize = sizeof(ignored);
    const auto first = ::recvfrom(socket.value, probe, sizeof(probe), NativeReceiveFlags((flags & ~GuestWaitAll) | GuestPeek, socket.type),
        reinterpret_cast<sockaddr*>(&ignored), &ignoredSize);
    if (first < 0) {
        error = NativeError();
        return true;
    }
    if (first == 0) return true;
    unsigned long pending = 0;
    if (ioctlsocket(socket.value, FIONREAD, &pending) != 0) {
        error = NativeError();
        return true;
    }
    return pending <= usable;
}
void DropDatagram(const Socket& socket, int flags) {
    if (socket.type != 2 || (flags & GuestPeek) != 0) return;
    char probe[1];
    sockaddr_storage ignored{};
    socklen_t ignoredSize = sizeof(ignored);
    ::recvfrom(socket.value, probe, sizeof(probe), NativeReceiveFlags(flags, socket.type), reinterpret_cast<sockaddr*>(&ignored), &ignoredSize);
}
#else
bool SourceFaults(const void* pointer, std::uint64_t length) {
    return !GuestReadable(pointer, length);
}
#endif
#ifdef __APPLE__
void DropFaultedDatagram(const Socket& socket, int flags) {
    if (socket.type != 2 || (flags & GuestPeek) != 0) return;
    char probe[1];
    ::recv(socket.value, probe, sizeof(probe), MSG_DONTWAIT);
}
#endif
bool ReportAddress(void* address, std::uint32_t* addressLength, const void* name, std::uint32_t size) {
    return (address == nullptr || GuestCopyOut_nid_no_patch(address, name, size)) && GuestCopyOut_nid_no_patch(addressLength, &size, sizeof(size));
}
void GuestAddress(const sockaddr_storage& native, void* output, std::uint32_t* length) {
    unsigned char bytes[28]{};
    bytes[0] = native.ss_family == AF_INET ? 16 : 28;
    bytes[1] = native.ss_family == AF_INET ? 2 : 28;
    if (native.ss_family == AF_INET) {
        const auto& v4 = reinterpret_cast<const sockaddr_in&>(native);
        std::memcpy(bytes + 2, &v4.sin_port, 2);
        std::memcpy(bytes + 4, &v4.sin_addr, 4);
    } else {
        const auto& v6 = reinterpret_cast<const sockaddr_in6&>(native);
        std::memcpy(bytes + 2, &v6.sin6_port, 2);
        std::memcpy(bytes + 4, &v6.sin6_flowinfo, 4);
        std::memcpy(bytes + 8, &v6.sin6_addr, 16);
        std::memcpy(bytes + 24, &v6.sin6_scope_id, 4);
    }
    const auto copied = std::min<std::uint32_t>(*length, bytes[0]);
    std::memcpy(output, bytes, copied);
    *length = copied;
}
}

int GuestSockets::Close(int descriptor) {
    std::shared_ptr<Socket> released;
    {
        std::lock_guard lock(socketsMutex);
        const auto found = sockets.find(descriptor);
        if (found == sockets.end()) return Fail(9);
        released = std::move(found->second);
        sockets.erase(found);
        closeOnExecDescriptors.erase(descriptor);
    }
    EqueueDescriptorClosed(descriptor);
    ReleaseNumber(descriptor);
    return 0;
}

void GuestSockets::Forget(int descriptor) {
    std::shared_ptr<Socket> released;
    {
        std::lock_guard lock(socketsMutex);
        const auto found = sockets.find(descriptor);
        if (found == sockets.end()) return;
        released = std::move(found->second);
        sockets.erase(found);
        closeOnExecDescriptors.erase(descriptor);
    }
    EqueueDescriptorClosed(descriptor);
}

std::uintptr_t GuestSockets::CurrentWaker() {
    return static_cast<std::uintptr_t>(ThreadWaker().value);
}

void GuestSockets::Wake(std::uintptr_t waker) {
    const char signal = 0;
    if (send(static_cast<NativeSocket>(waker), &signal, 1, 0) < 0 && NativeError() != 35)
        throw std::runtime_error("equeue waker: send failed, guest errno " + std::to_string(NativeError()));
}

bool GuestSockets::WaitAny(const std::vector<Interest>& interests, std::uint64_t deadlineNanos) {
    const NativeSocket waker = ThreadWaker().value;
    std::vector<std::shared_ptr<Socket>> held;
#ifdef _WIN32
    std::vector<WSAPOLLFD> entries;
    constexpr SHORT ReadMask = POLLRDNORM;
    constexpr SHORT WriteMask = POLLWRNORM;
    bool pipes = false;
#else
    std::vector<pollfd> entries;
    constexpr short ReadMask = POLLIN;
    constexpr short WriteMask = POLLOUT;
#endif
    {
        std::lock_guard lock(socketsMutex);
        for (const auto& interest : interests) {
            const auto found = sockets.find(interest.descriptor);
            if (found == sockets.end()) {
#ifdef _WIN32
                pipes = true;
#else
                entries.push_back({interest.descriptor, interest.write ? WriteMask : ReadMask, 0});
#endif
                continue;
            }
            held.push_back(found->second);
            entries.push_back({found->second->value, interest.write ? WriteMask : ReadMask, 0});
        }
    }
    entries.push_back({waker, ReadMask, 0});
    std::uint64_t remaining = 0;
    if (deadlineNanos != 0) {
        const std::uint64_t now = TimedWait::NowNanos();
        if (now >= deadlineNanos) return true;
        remaining = deadlineNanos - now;
    }
#ifdef _WIN32
    INT timeout = -1;
    if (deadlineNanos != 0) {
        if (remaining < 1000000) return false;
        timeout = static_cast<INT>(std::min<std::uint64_t>(remaining / 1000000, INT_MAX));
    }
    if (pipes && (timeout < 0 || timeout > 1)) timeout = 1;
    if (WSAPoll(entries.data(), static_cast<ULONG>(entries.size()), timeout) == SOCKET_ERROR)
        throw std::runtime_error("equeue wait: WSAPoll failed, guest errno " + std::to_string(NativeError()));
#elif defined(__APPLE__)
    const int timeout = deadlineNanos != 0 ? static_cast<int>(std::min<std::uint64_t>((remaining + 999999) / 1000000, INT_MAX)) : -1;
    if (::poll(entries.data(), entries.size(), timeout) < 0 && errno != EINTR)
        throw std::runtime_error("equeue wait: poll failed, guest errno " + std::to_string(NativeError()));
#else
    timespec timeout{static_cast<time_t>(remaining / 1000000000), static_cast<long>(remaining % 1000000000)};
    if (::ppoll(entries.data(), entries.size(), deadlineNanos != 0 ? &timeout : nullptr, nullptr) < 0 && errno != EINTR)
        throw std::runtime_error("equeue wait: ppoll failed, guest errno " + std::to_string(NativeError()));
#endif
    DrainWaker(waker);
    return true;
}

std::intptr_t GuestSockets::NativeDescriptor(int descriptor) {
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    return found == sockets.end() ? -1 : static_cast<std::intptr_t>(found->second->value);
}

bool GuestSockets::IsOpen(int descriptor) {
    std::lock_guard lock(socketsMutex);
    return sockets.contains(descriptor);
}
int GuestSockets::Family(int descriptor) {
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    return found == sockets.end() ? -1 : found->second->family;
}
int GuestSockets::Type(int descriptor) {
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    return found == sockets.end() ? -1 : found->second->type;
}
bool GuestSockets::Listening(int descriptor) {
    std::shared_ptr<Socket> socket;
    {
        std::lock_guard lock(socketsMutex);
        const auto found = sockets.find(descriptor);
        if (found == sockets.end()) return false;
        socket = found->second;
    }
    return Listening(*socket);
}

extern "C" bool GuestSocketIsOpen_nid_no_patch(int descriptor) {
    return GuestSockets::IsOpen(descriptor);
}

extern "C" int KernelReserveSocketNumber_nid_no_patch(std::uintptr_t native) {
    const int number = ReserveNumber(static_cast<NativeSocket>(native));
    return number < 0 ? -*__error_nid_postfix() : number;
}

extern "C" void KernelReleaseSocketNumber_nid_no_patch(int number) {
    ReleaseNumber(number);
}

extern "C" void KernelSetSocketForwards_nid_no_patch(const KernelSocketPoll::Forwards* forwards) {
    socketForwards.store(forwards, std::memory_order_release);
}

namespace {
#ifdef _WIN32
using NativePollDescriptor = WSAPOLLFD;
#else
using NativePollDescriptor = pollfd;
#endif

struct PollFlag {
    short guest;
    short native;
};
constexpr short GuestPollPriority = 0x2;
constexpr short GuestPollInvalid = 0x20;
constexpr int SocketCloseCheckMilliseconds = 10;
constexpr short GuestPollAccepted = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20 | 0x40 | 0x80 | 0x100;
constexpr short GuestPollIgnoreEof = 0x2000;
#ifdef _WIN32
constexpr short NativePollIn = POLLRDNORM;
constexpr short NativePollPriority = POLLRDBAND;
#else
constexpr short NativePollIn = POLLIN;
constexpr short NativePollPriority = POLLPRI;
#endif
constexpr PollFlag RequestFlags[] = {
    {0x1, NativePollIn}, {GuestPollPriority, NativePollPriority}, {0x4, POLLOUT}, {0x40, POLLRDNORM}, {0x80, POLLRDBAND}, {0x100, POLLWRBAND}};
constexpr PollFlag StatusFlags[] = {{0x8, POLLERR}, {0x10, POLLHUP}, {GuestPollInvalid, POLLNVAL}};
template <typename Descriptor>
bool UrgentPending(Descriptor descriptor) {
#ifdef __APPLE__
    int atmark = 0;
    return ::ioctl(static_cast<int>(descriptor), SIOCATMARK, &atmark) == 0 && atmark != 0;
#else
    static_cast<void>(descriptor);
    return false;
#endif
}
std::shared_ptr<Socket> Find(int descriptor) {
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    return found != sockets.end() ? found->second : nullptr;
}
std::vector<int> DescriptorsOf(const std::shared_ptr<Socket>& socket) {
    std::lock_guard lock(socketsMutex);
    std::vector<int> descriptors;
    for (const auto& [descriptor, entry] : sockets)
        if (entry == socket) descriptors.push_back(descriptor);
    return descriptors;
}
constexpr short GuestPollReadable = 0x1 | 0x40;
constexpr short GuestPollWritable = 0x4;
constexpr short GuestPollVnodeReady = GuestPollReadable | GuestPollWritable;
constexpr short GuestPollHangUp = 0x10;
constexpr short GuestPollError = 0x8;
#ifdef _WIN32
short ClosedStreamEvents(int descriptor, short revents, short requested) {
    if ((revents & (POLLHUP | POLLERR)) != POLLHUP) return revents;
    const auto socket = Find(descriptor);
    if (!socket || socket->type != 1) return revents;
    revents = static_cast<short>(revents | (requested & POLLRDNORM));
    std::lock_guard lock(socket->modeMutex);
    return socket->sendShut ? revents : static_cast<short>((revents & ~POLLHUP) | (requested & POLLOUT));
}
#elif defined(__APPLE__)
short ClosedStreamEvents(int descriptor, short revents, short requested) {
    if ((revents & (POLLHUP | POLLERR)) != POLLHUP) return revents;
    const auto socket = Find(descriptor);
    if (!socket || socket->type != 1) return revents;
    pollfd received{socket->value, POLLIN, 0};
    pollfd sent{socket->value, POLLOUT, 0};
    if (::poll(&received, 1, 0) < 0 || ::poll(&sent, 1, 0) < 0) return revents;
    revents = static_cast<short>(revents & ~(POLLPRI | POLLRDBAND));
    if ((received.revents & POLLHUP) && (sent.revents & POLLHUP)) return revents;
    revents = static_cast<short>(revents & ~POLLHUP);
    return (sent.revents & (POLLOUT | POLLHUP)) ? static_cast<short>(revents | (requested & POLLOUT)) : revents;
}
#endif
[[noreturn]] void ThrowReadEndWritable(int descriptor) {
    throw std::runtime_error("poll: POLLOUT on the read end of pipe descriptor " + std::to_string(descriptor) + " is not supported");
}
#ifdef _WIN32
struct PipeIoStatus {
    void* status;
    ULONG_PTR information;
};
struct PipeLocalInformation {
    ULONG type;
    ULONG configuration;
    ULONG maximumInstances;
    ULONG currentInstances;
    ULONG inboundQuota;
    ULONG readDataAvailable;
    ULONG outboundQuota;
    ULONG writeQuotaAvailable;
    ULONG state;
    ULONG end;
};
using QueryInformationFile = LONG(NTAPI*)(HANDLE, PipeIoStatus*, void*, ULONG, int);
constexpr int FileAccessInformationClass = 8;
constexpr int FilePipeLocalInformationClass = 24;
constexpr ULONG PipeClosingState = 4;
constexpr ULONG PipeAtomicBytes = 512;
bool QueryPipe(int descriptor, const char* function, ACCESS_MASK& access, PipeLocalInformation& pipe) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    if (handle == INVALID_HANDLE_VALUE || GetFileType(handle) != FILE_TYPE_PIPE) return false;
    static const auto query = reinterpret_cast<QueryInformationFile>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationFile")));
    if (!query) throw std::runtime_error(std::string(function) + ": NtQueryInformationFile is unavailable");
    PipeIoStatus status{};
    if (query(handle, &status, &access, sizeof(access), FileAccessInformationClass) < 0 ||
        query(handle, &status, &pipe, sizeof(pipe), FilePipeLocalInformationClass) < 0) {
        if (reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor)) != handle || GetFileType(handle) != FILE_TYPE_PIPE) return false;
        throw std::runtime_error(std::string(function) + ": querying pipe descriptor " + std::to_string(descriptor) + " failed");
    }
    return true;
}
bool PipeReadiness(int descriptor, short events, short& revents) {
    ACCESS_MASK access = 0;
    PipeLocalInformation pipe{};
    if (!QueryPipe(descriptor, "poll", access, pipe)) return false;
    if ((events & GuestPollWritable) && !(access & FILE_WRITE_DATA)) ThrowReadEndWritable(descriptor);
    const bool closed = pipe.state == PipeClosingState;
    revents = 0;
    if ((access & FILE_READ_DATA) && pipe.readDataAvailable > 0)
        revents = static_cast<short>(revents | (events & GuestPollReadable));
    if ((access & FILE_WRITE_DATA) && !closed && pipe.writeQuotaAvailable >= PipeAtomicBytes)
        revents = static_cast<short>(revents | (events & GuestPollWritable));
    if (closed) revents = static_cast<short>(revents | (events & GuestPollReadable) | GuestPollHangUp);
    return true;
}
#else
bool IsPipe(int descriptor, short events) {
    struct stat status{};
    if (::fstat(descriptor, &status) != 0 || !S_ISFIFO(status.st_mode)) return false;
    const int flags = ::fcntl(descriptor, F_GETFL);
    if (flags < 0 && errno == EBADF) return false;
    if (flags < 0) throw std::runtime_error("poll: querying pipe descriptor " + std::to_string(descriptor) + " failed");
    if ((events & GuestPollWritable) && (flags & O_ACCMODE) == O_RDONLY) ThrowReadEndWritable(descriptor);
    return true;
}
#endif
}

#ifdef _WIN32
bool GuestSockets::PipeReady(int descriptor, short events, short& revents) {
    return PipeReadiness(descriptor, events, revents);
}
#endif

namespace {
int DuplicateSocket_nid_no_patch(int descriptor, int minimum) {
    if (minimum < 0) return Fail(22);
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    if (found == sockets.end()) return Fail(9);
    const int duplicate = CopyNumber(descriptor, minimum);
    if (duplicate < 0) return -1;
    try {
        sockets.insert_or_assign(duplicate, found->second);
        closeOnExecDescriptors.erase(duplicate);
    } catch (const std::bad_alloc&) {
        sockets.erase(duplicate);
        ReleaseNumber(duplicate);
        return Fail(12);
    }
    return duplicate;
}
}

int GuestSockets::Duplicate(int descriptor) {
    return DuplicateSocket_nid_no_patch(descriptor, 0);
}

int GuestSockets::DuplicateTo(int descriptor, int target) {
    std::shared_ptr<Socket> replaced;
    {
        std::lock_guard lock(socketsMutex);
        const auto found = sockets.find(descriptor);
        if (found == sockets.end()) return Fail(9);
        if (target == descriptor) return target;
        if (CopyNumberTo(descriptor, target) != 0) return -1;
        const auto previous = sockets.find(target);
        if (previous != sockets.end()) {
            replaced = std::move(previous->second);
            previous->second = found->second;
        } else {
            try {
                sockets.emplace(target, found->second);
            } catch (const std::bad_alloc&) {
                ReleaseNumber(target);
                return Fail(12);
            }
        }
    }
    EqueueDescriptorClosed(target);
    {
        std::lock_guard lock(socketsMutex);
        closeOnExecDescriptors.erase(target);
    }
    return target;
}

namespace {
constexpr int GuestUnix = 1;
constexpr int GuestCloseOnExec = 0x10000000;
constexpr int GuestNonBlocking = 0x20000000;
constexpr std::uint32_t NoDevice = 0xFFFFFFFFu;
std::uint32_t lastUnixInode = 0;
bool PairPeerGone(const Socket& socket) {
    return socket.family == GuestUnix && !socket.externalPeer && socket.peer.expired();
}
bool RecordOverSendBuffer(Socket& socket, std::uint64_t length) {
    if (socket.family != GuestUnix || (socket.type != 2 && socket.type != 5) || socket.peer.expired()) return false;
    std::lock_guard lock(socket.modeMutex);
    return !socket.sendShut && length > static_cast<std::uint64_t>(GuestSockets::SendBufferSize(socket.options, socket.family, socket.type));
}
int SetNonBlocking(Socket& socket) {
#ifdef _WIN32
    unsigned long value = 1;
    if (ioctlsocket(socket.value, FIONBIO, &value)) return Fail(NativeError());
#else
    int value = 1;
    if (::ioctl(socket.value, FIONBIO, &value)) return Fail(NativeError());
#endif
    socket.options.nonblocking = true;
    return 0;
}
bool ReadableNow(const Socket& socket) {
    NativePollDescriptor entry{};
    entry.fd = socket.value;
#ifdef _WIN32
    entry.events = POLLRDNORM;
    return WSAPoll(&entry, 1, 0) > 0;
#else
    entry.events = POLLIN;
    return ::poll(&entry, 1, 0) > 0;
#endif
}
bool ReceiveShut(Socket& socket) {
    std::lock_guard lock(socket.modeMutex);
    return socket.receiveShut;
}
std::mutex readWatchersMutex;
std::multimap<int, std::uintptr_t> readWatchers;
struct ReadWatch {
    std::vector<std::multimap<int, std::uintptr_t>::iterator> entries;
    explicit ReadWatch(const std::vector<int>& descriptors) {
        if (descriptors.empty()) return;
        const auto waker = static_cast<std::uintptr_t>(ThreadWaker().value);
        entries.reserve(descriptors.size());
        std::lock_guard lock(readWatchersMutex);
        try {
            for (const int descriptor : descriptors) entries.push_back(readWatchers.emplace(descriptor, waker));
        } catch (...) {
            for (const auto entry : entries) readWatchers.erase(entry);
            throw;
        }
    }
    ~ReadWatch() {
        std::lock_guard lock(readWatchersMutex);
        for (const auto entry : entries) readWatchers.erase(entry);
    }
    ReadWatch(const ReadWatch&) = delete;
    ReadWatch& operator=(const ReadWatch&) = delete;
};
void WakeReadWatchers(int descriptor) {
    std::lock_guard lock(readWatchersMutex);
    const auto [first, last] = readWatchers.equal_range(descriptor);
    for (auto entry = first; entry != last; ++entry) GuestSockets::Wake(entry->second);
}

int ReceiveAnsweredHere(Socket& socket, std::uint64_t length) {
    if (ReceiveShut(socket)) return 0;
    if (length != 0) return -1;
    if (socket.type != 1) return ReadableNow(socket) ? -1 : 0;
    sockaddr_storage peer{};
    socklen_t size = sizeof(peer);
    return ::getpeername(socket.value, reinterpret_cast<sockaddr*>(&peer), &size) == 0 ? 0 : NativeError();
}

int UnsupportedLevelError(const Socket& socket, int level, int option, bool set) {
    if (socket.family == GuestUnix) {
        if (level != 0) return 22;
        const bool handledByFreeBsd = set ? option == 2 || option == 4 : option == 1 || option == 2 || option == 4;
        if (handledByFreeBsd) NotImplemented_nid_no_patch("setsockopt, getsockopt: LOCAL_PEERCRED, LOCAL_CREDS and LOCAL_CONNWAIT on an AF_UNIX socket");
        return 45;
    }
    const bool ownProtocol = level == 17 && socket.type == 2 && socket.family == 2;
    const bool ownFamily = socket.family == 2 && level == 0;
    return ownProtocol || ownFamily ? 42 : 22;
}
bool SendBeforeConnect(Socket& socket) {
    if (socket.type != 1) return false;
    std::lock_guard lock(socket.modeMutex);
    return !socket.connectStarted;
}
bool LocalDatagram(const Socket& socket) {
#ifdef _WIN32
    static_cast<void>(socket);
    return false;
#else
    return socket.family == GuestUnix && socket.type == 2;
#endif
}
int SendError(const Socket& socket) {
    const int error = SendError(socket.type);
    return LocalDatagram(socket) && error == 35 ? 55 : error;
}
int SocketSendFlags(Socket& socket, int flags) {
    std::lock_guard lock(socket.modeMutex);
    const int native = NativeSendFlags(socket.options.noSignal ? flags | GuestNoSignal : flags);
#ifdef _WIN32
    return native;
#else
    return LocalDatagram(socket) ? native | MSG_DONTWAIT : native;
#endif
}
#ifdef _WIN32
bool ReceiveWouldBlock(const Socket& socket) {
    if (socket.type == 1) return PeerConnected(socket);
    sockaddr_storage name{};
    socklen_t size = sizeof(name);
    return ::getsockname(socket.value, reinterpret_cast<sockaddr*>(&name), &size) == 0;
}
#endif
int WaitForTransfer(Socket& socket, bool send, int flags, int receiver = -1) {
    if (send && LocalDatagram(socket)) return 0;
    if ((flags & GuestDontWait) != 0) {
#ifdef _WIN32
        NativePollDescriptor entry{};
        entry.fd = socket.value;
        entry.events = static_cast<short>(send ? POLLWRNORM : POLLRDNORM);
        return WSAPoll(&entry, 1, 0) == 0 ? 35 : 0;
#else
        return 0;
#endif
    }
    std::int64_t timeout;
    {
        std::lock_guard lock(socket.modeMutex);
        if (socket.options.nonblocking) return 0;
        timeout = send ? socket.options.sendTimeout : socket.options.receiveTimeout;
    }
#ifdef _WIN32
    const bool watched = receiver >= 0 && ReceiveWouldBlock(socket);
#else
    static_cast<void>(receiver);
    constexpr bool watched = false;
#endif
    if (timeout == 0 && !watched) return 0;
    const ReadWatch watch(watched ? std::vector<int>{receiver} : std::vector<int>{});
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout >> 32) +
        std::chrono::nanoseconds((static_cast<std::int64_t>(static_cast<std::uint32_t>(timeout)) * 1000000000) >> 32);
    for (;;) {
        if (watched && ReceiveShut(socket)) return 0;
        int milliseconds = -1;
        if (timeout != 0) {
            const auto left = deadline - std::chrono::steady_clock::now();
            if (left <= std::chrono::steady_clock::duration::zero()) return 35;
            milliseconds = static_cast<int>(std::min<std::int64_t>(std::chrono::ceil<std::chrono::milliseconds>(left).count(), INT_MAX));
        }
        NativePollDescriptor entries[2]{};
        entries[0].fd = socket.value;
#ifdef _WIN32
        entries[0].events = static_cast<short>(send ? POLLWRNORM : POLLRDNORM);
        if (watched) {
            entries[1].fd = ThreadWaker().value;
            entries[1].events = POLLRDNORM;
        }
        const int ready = WSAPoll(entries, watched ? 2 : 1, milliseconds);
        if (ready < 0 || entries[0].revents != 0) return 0;
        if (entries[1].revents != 0) DrainWaker(entries[1].fd);
#else
        entries[0].events = static_cast<short>(send ? POLLOUT : POLLIN);
        const int ready = ::poll(entries, 1, milliseconds);
        if (ready > 0 || (ready < 0 && errno != EINTR)) return 0;
#endif
    }
}
#ifdef _WIN32
bool LoopbackName(NativeSocket socket, sockaddr_in& name) {
    int size = sizeof(name);
    return getsockname(socket, reinterpret_cast<sockaddr*>(&name), &size) == 0;
}
int NativePair(int type, NativeSocket (&pair)[2]) {
    static const int startup = [] { WSADATA data{}; return WSAStartup(MAKEWORD(2, 2), &data); }();
    if (startup) return 5;
    if (type == 5) throw std::runtime_error("socketpair: SOCK_SEQPACKET is not supported on Windows");
    sockaddr_in loopback{};
    loopback.sin_family = AF_INET;
    loopback.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    const int nativeType = type == 1 ? SOCK_STREAM : SOCK_DGRAM;
    Socket first(::socket(AF_INET, nativeType, 0), 2, type);
    if (first.value == Invalid) return NativeError();
    const int exclusive = 1;
    setsockopt(first.value, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive));
    if (bind(first.value, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback))) return NativeError();
    sockaddr_in firstName{};
    if (!LoopbackName(first.value, firstName)) return NativeError();
    Socket second(::socket(AF_INET, nativeType, 0), 2, type);
    if (second.value == Invalid) return NativeError();
    if (type == 1) {
        if (listen(first.value, 1)) return NativeError();
        if (connect(second.value, reinterpret_cast<sockaddr*>(&firstName), sizeof(firstName))) return NativeError();
        sockaddr_in peer{};
        int peerSize = sizeof(peer);
        Socket accepted(accept(first.value, reinterpret_cast<sockaddr*>(&peer), &peerSize), 2, type);
        if (accepted.value == Invalid) return NativeError();
        sockaddr_in secondName{};
        if (!LoopbackName(second.value, secondName)) return NativeError();
        if (peer.sin_port != secondName.sin_port) return 53;
        const int noDelay = 1;
        setsockopt(second.value, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
        setsockopt(accepted.value, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
        pair[0] = second.value;
        pair[1] = accepted.value;
        second.value = Invalid;
        accepted.value = Invalid;
        return 0;
    }
    if (bind(second.value, reinterpret_cast<sockaddr*>(&loopback), sizeof(loopback))) return NativeError();
    sockaddr_in secondName{};
    if (!LoopbackName(second.value, secondName)) return NativeError();
    if (connect(first.value, reinterpret_cast<sockaddr*>(&secondName), sizeof(secondName))) return NativeError();
    if (connect(second.value, reinterpret_cast<sockaddr*>(&firstName), sizeof(firstName))) return NativeError();
    pair[0] = first.value;
    pair[1] = second.value;
    first.value = Invalid;
    second.value = Invalid;
    return 0;
}
#else
int NativePair(int type, NativeSocket (&pair)[2]) {
#ifdef __APPLE__
    if (type == 5) throw std::runtime_error("socketpair: SOCK_SEQPACKET is not supported on macOS");
#endif
    const int nativeType = type == 1 ? SOCK_STREAM : type == 2 ? SOCK_DGRAM : SOCK_SEQPACKET;
    int native[2];
    if (::socketpair(AF_UNIX, nativeType, 0, native)) return NativeError();
    pair[0] = native[0];
    pair[1] = native[1];
    return 0;
}
#endif
int QueuedBytes(const Socket& socket, int& count) {
#ifdef _WIN32
    unsigned long pending = 0;
    if (ioctlsocket(socket.value, FIONREAD, &pending) != 0) return -1;
    count = static_cast<int>(pending);
    return 0;
#else
#ifdef __APPLE__
    if (socket.type == 2) {
        socklen_t size = sizeof(count);
        return ::getsockopt(socket.value, SOL_SOCKET, SO_NREAD, &count, &size);
    }
#endif
    return ::ioctl(socket.value, FIONREAD, &count);
#endif
}
#ifdef _WIN32
bool DatagramQueued(const Socket& socket) {
    fd_set readable;
    FD_ZERO(&readable);
    FD_SET(socket.value, &readable);
    timeval none{};
    return ::select(0, &readable, nullptr, nullptr, &none) > 0;
}
#endif
int UnnamedAddress(void* address, std::uint32_t* length) {
    const unsigned char bytes[16] = {16, GuestUnix};
    *length = std::min<std::uint32_t>(*length, sizeof(bytes));
    std::memcpy(address, bytes, *length);
    return 0;
}
}

int GuestSockets::Stat(int descriptor, FileStat* status) {
    std::shared_ptr<Socket> socket;
    std::uint32_t inode = 0;
    {
        std::lock_guard lock(socketsMutex);
        const auto found = sockets.find(descriptor);
        if (found == sockets.end()) return 9;
        socket = found->second;
        if (socket->family == GuestUnix && socket->inode == 0) socket->inode = ++lastUnixInode != 0 ? lastUnixInode : ++lastUnixInode;
        inode = socket->inode;
    }
    bool receiveShut = false;
    bool sendShut = false;
    {
        std::lock_guard lock(socket->modeMutex);
        receiveShut = socket->receiveShut;
        sendShut = socket->sendShut;
    }
    std::int64_t available = 0;
#ifdef _WIN32
    unsigned long count = 0;
    if (!receiveShut && ioctlsocket(socket->value, FIONREAD, &count) == 0) available = static_cast<std::int64_t>(count);
#else
    int count = 0;
    if (!receiveShut && QueuedBytes(*socket, count) == 0) available = count;
#endif
    int sendBuffer = 0;
    socklen_t length = sizeof(sendBuffer);
    if (getsockopt(socket->value, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sendBuffer), &length) != 0) sendBuffer = 0;
    *status = FileStat{};
    if (socket->family == GuestUnix) {
        status->st_dev = NoDevice;
        status->st_ino = inode;
    }
    status->st_mode = 0140000;
    if (!receiveShut || available > 0) status->st_mode |= 0444;
    if (!sendShut) status->st_mode |= 0222;
    status->st_size = available;
    status->st_blksize = sendBuffer > 0 ? static_cast<std::uint32_t>(sendBuffer) : 0;
    return 0;
}

namespace {
std::int64_t ListenQueueLength(NativeSocket socket) {
#ifdef __linux__
    tcp_info info{};
    socklen_t length = sizeof(info);
    if (getsockopt(socket, IPPROTO_TCP, TCP_INFO, &info, &length) == 0 && length >= offsetof(tcp_info, tcpi_unacked) + sizeof(info.tcpi_unacked))
        return info.tcpi_unacked;
#else
    (void)socket;
#endif
    return 1;
}
}

bool GuestSockets::Ready(int descriptor, bool write, std::int64_t* data, bool* eof) {
    std::shared_ptr<Socket> socket;
    {
        std::lock_guard lock(socketsMutex);
        const auto found = sockets.find(descriptor);
        if (found == sockets.end() && !write) return PipeReadState(descriptor, data, eof) && (*data > 0 || *eof);
        if (found == sockets.end()) {
            bool writable = false;
            return PipeWriteState(descriptor, data, eof, &writable) && (writable || *eof);
        }
        socket = found->second;
    }
    if (!write && ReceiveShut(*socket) && !Listening(*socket)) {
        *data = 0;
        *eof = true;
        return true;
    }
#ifdef _WIN32
    WSAPOLLFD entry{socket->value, static_cast<SHORT>(write ? POLLWRNORM : POLLRDNORM), 0};
    if (WSAPoll(&entry, 1, 0) <= 0) return false;
#elif defined(__APPLE__)
    pollfd entry{socket->value, static_cast<short>(write ? POLLOUT : POLLIN), 0};
    if (::poll(&entry, 1, 0) <= 0) return false;
    if (!write && socket->type == 1 && (entry.revents & POLLIN)) {
        char byte;
        if (::recv(socket->value, &byte, 1, MSG_PEEK | MSG_DONTWAIT) == 0) entry.revents |= POLLHUP;
    }
#else
    pollfd entry{socket->value, static_cast<short>(write ? POLLOUT : (POLLIN | POLLRDHUP)), 0};
    if (::poll(&entry, 1, 0) <= 0) return false;
    if (!write && (entry.revents & POLLRDHUP)) entry.revents |= POLLHUP;
#endif
    *eof = (entry.revents & (POLLHUP | POLLERR)) != 0;
    *data = 0;
    if (write) {
        int size = 0;
        socklen_t length = sizeof(size);
        if (getsockopt(socket->value, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&size), &length) == 0) *data = size;
#ifdef __linux__
        int queued = 0;
        if (*data > 0 && ::ioctl(socket->value, SIOCOUTQ, &queued) == 0) *data = std::max<std::int64_t>(0, *data - queued);
#endif
    } else if (Listening(*socket)) {
        *data = ListenQueueLength(socket->value);
    } else {
#ifdef _WIN32
        unsigned long available = 0;
        if (ioctlsocket(socket->value, FIONREAD, &available) == 0) *data = static_cast<std::int64_t>(available);
#else
        int available = 0;
        if (QueuedBytes(*socket, available) == 0) *data = available;
#endif
    }
    return true;
}

bool GuestSockets::PipeReadState(int descriptor, std::int64_t* data, bool* eof) {
#ifdef _WIN32
    ACCESS_MASK access = 0;
    PipeLocalInformation pipe{};
    if (!QueryPipe(descriptor, "kevent", access, pipe) || !(access & FILE_READ_DATA)) return false;
    *data = pipe.readDataAvailable;
    *eof = pipe.state == PipeClosingState;
#else
    if (!IsPipe(descriptor, 0) || (::fcntl(descriptor, F_GETFL) & O_ACCMODE) == O_WRONLY) return false;
    pollfd entry{descriptor, POLLIN, 0};
    int available = 0;
    if (::poll(&entry, 1, 0) < 0 || ::ioctl(descriptor, FIONREAD, &available) != 0)
        throw std::runtime_error("kevent: querying pipe descriptor " + std::to_string(descriptor) + " failed");
    *data = available;
    *eof = (entry.revents & POLLHUP) != 0;
#endif
    return true;
}

bool GuestSockets::PipeWriteState(int descriptor, std::int64_t* data, bool* eof, bool* writable) {
#ifdef _WIN32
    ACCESS_MASK access = 0;
    PipeLocalInformation pipe{};
    if (!QueryPipe(descriptor, "kevent", access, pipe) || !(access & FILE_WRITE_DATA)) return false;
    *eof = pipe.state == PipeClosingState;
    *writable = pipe.writeQuotaAvailable >= PipeAtomicBytes;
    *data = *eof ? 0 : pipe.writeQuotaAvailable;
#elif defined(__linux__)
    if (!IsPipe(descriptor, 0) || (::fcntl(descriptor, F_GETFL) & O_ACCMODE) == O_RDONLY) return false;
    pollfd entry{descriptor, POLLOUT, 0};
    int queued = 0;
    const int capacity = ::fcntl(descriptor, F_GETPIPE_SZ);
    if (::poll(&entry, 1, 0) < 0 || ::ioctl(descriptor, FIONREAD, &queued) != 0 || capacity < 0)
        throw std::runtime_error("kevent: querying pipe descriptor " + std::to_string(descriptor) + " failed");
    *eof = (entry.revents & POLLERR) != 0;
    *writable = (entry.revents & POLLOUT) != 0;
    *data = *eof ? 0 : std::max<std::int64_t>(0, capacity - queued);
#elif defined(__APPLE__)
    if (!IsPipe(descriptor, 0) || (::fcntl(descriptor, F_GETFL) & O_ACCMODE) == O_RDONLY) return false;
    char path[MAXPATHLEN];
    if (::fcntl(descriptor, F_GETPATH, path) >= 0) NotImplemented_nid_no_patch(__func__);
    if (errno != EBADF)
        throw std::runtime_error("kevent: identifying pipe descriptor " + std::to_string(descriptor) + " failed, host errno " + std::to_string(errno));
    const int queue = ::kqueue();
    if (queue < 0)
        throw std::runtime_error("kevent: querying pipe descriptor " + std::to_string(descriptor) + " failed, host errno " + std::to_string(errno));
    struct Queue {
        int descriptor;
        ~Queue() { ::close(descriptor); }
    } query{queue};
    if (::fcntl(queue, F_SETFD, FD_CLOEXEC) < 0)
        throw std::runtime_error("kevent: querying pipe descriptor " + std::to_string(descriptor) + " failed, host errno " + std::to_string(errno));
    struct kevent interest{}, ready{};
    EV_SET(&interest, static_cast<std::uintptr_t>(descriptor), EVFILT_WRITE, EV_ADD, 0, 0, nullptr);
    const timespec timeout{};
    int count;
    do {
        count = ::kevent(queue, &interest, 1, &ready, 1, &timeout);
    } while (count < 0 && errno == EINTR);
    if (count < 0 || (count > 0 && (ready.flags & EV_ERROR))) {
        const int error = count < 0 ? errno : static_cast<int>(ready.data);
        throw std::runtime_error("kevent: querying pipe descriptor " + std::to_string(descriptor) + " failed, host errno " + std::to_string(error));
    }
    *eof = count > 0 && (ready.flags & EV_EOF) != 0;
    *writable = count > 0 && !*eof;
    *data = *writable ? ready.data : 0;
#else
    if (!IsPipe(descriptor, 0) || (::fcntl(descriptor, F_GETFL) & O_ACCMODE) == O_RDONLY) return false;
    NotImplemented_nid_no_patch("kevent: EVFILT_WRITE on a pipe on macOS");
#endif
    return true;
}

int GuestSockets::Poll(KernelSocketPoll::Entry* entries, int count, int timeoutMilliseconds) {
    std::vector<std::shared_ptr<Socket>> held;
    std::vector<NativePollDescriptor> native;
#ifdef _WIN32
    constexpr short NativeReadable = POLLRDNORM;
    constexpr short NativeUrgent = POLLRDBAND;
    constexpr short NativeWritable = POLLWRNORM;
#else
    constexpr short NativeReadable = POLLIN;
    constexpr short NativeUrgent = POLLPRI;
    constexpr short NativeWritable = POLLOUT;
#endif
    std::vector<int> readers;
    for (int index = 0; index < count; ++index) {
        auto socket = Find(entries[index].descriptor);
        if (!socket) return -9;
        NativePollDescriptor request{};
        request.fd = socket->value;
        if (entries[index].events & KernelSocketPoll::Readable) {
            request.events = static_cast<short>(request.events | NativeReadable);
            readers.push_back(entries[index].descriptor);
        }
        if (entries[index].events & KernelSocketPoll::Urgent) request.events = static_cast<short>(request.events | NativeUrgent);
        if (entries[index].events & KernelSocketPoll::Writable) request.events = static_cast<short>(request.events | NativeWritable);
        native.push_back(request);
        held.push_back(std::move(socket));
    }
    if (native.empty()) return 0;
    const auto shutReader = [&](int index) {
        return (entries[index].events & KernelSocketPoll::Readable) != 0 && ReceiveShut(*held[static_cast<std::size_t>(index)]);
    };
    const ReadWatch watch(readers);
    if (!readers.empty()) {
        NativePollDescriptor request{};
        request.fd = ThreadWaker().value;
        request.events = NativeReadable;
        native.push_back(request);
    }
    for (int index = 0; index < count; ++index)
        if (shutReader(index)) timeoutMilliseconds = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMilliseconds < 0 ? 0 : timeoutMilliseconds);
    int result = 0;
    for (;;) {
        for (std::size_t index = 0; index < held.size(); ++index) {
            const auto current = Find(entries[index].descriptor);
            if (current == held[index]) continue;
            if (current || DescriptorIsOpen_nid_no_patch(entries[index].descriptor)) {
                entries[index].revents = KernelSocketPoll::Unknown;
                return 0;
            }
            return -9;
        }
        int slice = SocketCloseCheckMilliseconds;
        if (timeoutMilliseconds >= 0) {
            const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
            slice = static_cast<int>(std::clamp<long long>(left, 0, SocketCloseCheckMilliseconds));
        }
#ifdef _WIN32
        result = WSAPoll(native.data(), static_cast<ULONG>(native.size()), slice);
#else
        result = ::poll(native.data(), static_cast<nfds_t>(native.size()), slice);
#endif
        if (result < 0) return -NativeError();
        for (std::size_t index = 0; index < held.size(); ++index) {
            const auto current = Find(entries[index].descriptor);
            if (current == held[index]) continue;
            if (current || DescriptorIsOpen_nid_no_patch(entries[index].descriptor)) {
                entries[index].revents = KernelSocketPoll::Unknown;
                return result;
            }
            return -9;
        }
        if (result != 0 || (timeoutMilliseconds >= 0 && std::chrono::steady_clock::now() >= deadline)) break;
    }
    if (!readers.empty() && native[static_cast<std::size_t>(count)].revents != 0) DrainWaker(native[static_cast<std::size_t>(count)].fd);
    int ready = 0;
    for (int index = 0; index < count; ++index) {
        const short events = native[static_cast<std::size_t>(index)].revents;
        short revents = 0;
        if ((events & NativeReadable) || shutReader(index)) revents = static_cast<short>(revents | KernelSocketPoll::Readable);
        if (events & NativeUrgent) revents = static_cast<short>(revents | KernelSocketPoll::Urgent);
        if (events & NativeWritable) revents = static_cast<short>(revents | KernelSocketPoll::Writable);
        if (events & (POLLERR | POLLNVAL)) revents = static_cast<short>(revents | KernelSocketPoll::Error);
        if (events & POLLHUP) revents = static_cast<short>(revents | KernelSocketPoll::HangUp);
        entries[index].revents = revents;
        if (revents != 0) ++ready;
    }
    return ready;
}

}

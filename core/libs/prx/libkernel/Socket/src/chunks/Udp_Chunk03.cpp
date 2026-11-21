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
}
}

std::int64_t APS5_VABI send_nid_postfix(int descriptor, const void* buffer, std::uint64_t length, int flags) {
    if (const auto* forwards = ForwardsFor(descriptor).forwards; forwards != nullptr) {
        if ((flags & ~GuestSendFlags) != 0) return Fail(45);
        if (length > INT_MAX) return Fail(40);
        if (!buffer && length) return Fail(14);
        int error = 0;
        const auto sent = forwards->sendto(descriptor, buffer, length, flags, nullptr, 0, &error);
        return error == 0 ? sent : Fail(error);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestSendFlags) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (SendBeforeConnect(*socket)) return Fail(57);
    if (RecordOverSendBuffer(*socket, length)) return Fail(40);
    if (!buffer && length) return Fail(14);
#ifdef _WIN32
    if (socket->type == 1 && PairPeerGone(*socket)) return Fail(32);
#endif
    if (const int waited = WaitForTransfer(*socket, true, flags)) return Fail(waited);
    const auto result = ::send(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), SocketSendFlags(*socket, flags));
    return result < 0 ? Fail(SendError(*socket)) : result;
}
std::int64_t APS5_VABI recv_nid_postfix(int descriptor, void* buffer, std::uint64_t length, int flags) {
    if (const auto* forwards = ForwardsFor(descriptor).forwards; forwards != nullptr) {
        if ((flags & ~GuestReceiveFlags) != 0) return Fail(45);
        if (length > INT_MAX) return Fail(40);
        if (!buffer && length) return Fail(14);
        int error = 0;
        const auto received = forwards->recvfrom(descriptor, buffer, length, flags, nullptr, nullptr, &error);
        return error == 0 ? received : Fail(error);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestReceiveFlags) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    const GuestArena::HostWrite destination(length == 0 ? nullptr : buffer, static_cast<std::size_t>(length));
    if (!destination.Open()) return Fail(14);
    if (const int answered = ReceiveAnsweredHere(*socket, length); answered >= 0) return answered == 0 ? 0 : Fail(answered);
#ifdef _WIN32
    if (socket->type == 2 && PairPeerGone(*socket) && !DatagramQueued(*socket)) return Fail(54);
#endif
    if (const int waited = WaitForTransfer(*socket, false, flags, descriptor)) return Fail(waited);
    if (ReceiveShut(*socket)) return 0;
    const auto result = ::recv(socket->value, static_cast<char*>(buffer), static_cast<int>(length),
        NativeReceiveFlags(flags, socket->type));
#ifdef _WIN32
    if (result < 0 && socket->type == 2 && WSAGetLastError() == WSAEMSGSIZE) return static_cast<std::int64_t>(length);
#endif
    return result < 0 ? Fail(NativeError()) : result;
}
int APS5_VABI bind_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    if (const int error = AddressLengthError(address, length)) return Fail(error);
    std::array<unsigned char, 255> addressCopy;
    if (!GuestCopyIn_nid_no_patch(addressCopy.data(), address, length)) return Fail(14);
    address = addressCopy.data();
    if (const auto [forwards, family] = ForwardsFor(descriptor); forwards != nullptr) {
        sockaddr_storage checked{};
        socklen_t checkedSize;
        if (!Address(family, address, length, checked, checkedSize)) return -1;
        const int error = forwards->bind(descriptor, address, length);
        return error == 0 ? 0 : Fail(error);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    sockaddr_storage native{};
    socklen_t size;
    const int mapped = LongMappedDualStack(*socket, address, length);
    if (mapped < 0) return -1;
    if (!Address(socket->family, address, mapped ? 28 : length, native, size)) return -1;
    if (socket->type == 1 && Multicast(native)) return Fail(47);
    std::unique_lock lock(socket->modeMutex, std::defer_lock);
    if (socket->type == 2) lock.lock();
    if (::bind(socket->value, reinterpret_cast<sockaddr*>(&native), size)) return Fail(NativeError());
    if (socket->type == 2) socket->localAddressCleared = false;
    return 0;
}
namespace {
bool AlreadyShut(NativeSocket socket, int how) {
#ifdef __APPLE__
    if (errno != ENOTCONN) return false;
    sockaddr_storage peer{};
    socklen_t size = sizeof(peer);
    if (::getpeername(socket, reinterpret_cast<sockaddr*>(&peer), &size)) {
        errno = ENOTCONN;
        return false;
    }
    return how != SHUT_RDWR || ::shutdown(socket, SHUT_WR) == 0 || errno == ENOTCONN;
#else
    (void)socket;
    (void)how;
    return false;
#endif
}
}

int APS5_VABI shutdown_nid_postfix(int descriptor, int how) {
    if (const auto* forwards = ForwardsFor(descriptor).forwards; forwards != nullptr) {
        if (const int error = forwards->shutdown(descriptor, how)) return Fail(error);
        if (how != 1) {
            EqueueDescriptorReadShut(descriptor);
            WakeReadWatchers(descriptor);
        }
        return 0;
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (how < 0 || how > 2) return Fail(22);
    sockaddr_storage peer{};
    socklen_t size = sizeof(peer);
    const bool unconnected = socket->type == 2 && ::getpeername(socket->value, reinterpret_cast<sockaddr*>(&peer), &size) != 0;
    {
        std::lock_guard lock(socket->modeMutex);
        if (::shutdown(socket->value, how) && !unconnected && !AlreadyShut(socket->value, how)) return Fail(NativeError());
        if (how != 1) socket->receiveShut = true;
        if (how != 0) socket->sendShut = true;
    }
    if (how != 1) {
        for (const int alias : DescriptorsOf(socket)) {
            EqueueDescriptorReadShut(alias);
            WakeReadWatchers(alias);
        }
    }
    return unconnected ? Fail(57) : 0;
}
namespace {
int CopyAddressOut(const sockaddr_storage& native, bool unnamed, void* address, std::uint32_t* length, std::uint32_t size) {
    unsigned char bytes[28]{};
    if (unnamed) UnnamedAddress(bytes, &size);
    else GuestAddress(native, bytes, &size);
    if (size != 0 && !GuestCopyOut_nid_no_patch(address, bytes, size)) return Fail(14);
    if (!GuestCopyOut_nid_no_patch(length, &size, sizeof(size))) return Fail(14);
    return 0;
}
}
int APS5_VABI getsockname_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    std::uint32_t capacity = 0;
    if (!GuestCopyIn_nid_no_patch(&capacity, length, sizeof(capacity))) return Fail(14);
    if (const auto* forwards = ForwardsFor(descriptor).forwards; forwards != nullptr) {
        unsigned char name[28]{};
        std::uint32_t size = std::min<std::uint32_t>(capacity, sizeof(name));
        if (const int error = forwards->getsockname(descriptor, name, &size)) return Fail(error);
        if (size != 0 && !GuestCopyOut_nid_no_patch(address, name, size)) return Fail(14);
        return GuestCopyOut_nid_no_patch(length, &size, sizeof(size)) ? 0 : Fail(14);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (socket->family == GuestUnix) return CopyAddressOut({}, true, address, length, capacity);
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    if (LocalAddress(*socket, native, size)) {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEINVAL) return Fail(NativeError());
        native = {};
        native.ss_family = socket->family == 2 ? AF_INET : AF_INET6;
#else
        return Fail(NativeError());
#endif
    }
    return CopyAddressOut(native, false, address, length, capacity);
}
int APS5_VABI getpeername_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    std::uint32_t capacity = 0;
    if (!GuestCopyIn_nid_no_patch(&capacity, length, sizeof(capacity))) return Fail(14);
    std::shared_ptr<void> owner;
    std::uintptr_t foreign = 0;
    const bool forwarded = !GuestSockets::IsOpen(descriptor)
        && KernelFindSocketNative_nid_no_patch(descriptor, &owner, &foreign);
    const auto socket = forwarded ? nullptr : Lookup(descriptor);
    if (!forwarded && !socket) return -1;
    if (socket && socket->family == GuestUnix) {
        if (!socket->externalPeer && socket->peer.expired()) return Fail(57);
        return CopyAddressOut({}, true, address, length, capacity);
    }
    const auto nativeSocket = forwarded ? static_cast<decltype(Socket::value)>(foreign) : socket->value;
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    if (::getpeername(nativeSocket, reinterpret_cast<sockaddr*>(&native), &size)) return Fail(NativeError());
    return CopyAddressOut(native, false, address, length, capacity);
}
int APS5_VABI ioctl_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    if (!GuestSockets::IsOpen(descriptor)) {
        const auto command = static_cast<std::uint32_t>(request);
        std::shared_ptr<void> owner;
        std::uintptr_t native = 0;
        if (command == 0x20006601 || command == 0x20006602 || !KernelFindSocketNative_nid_no_patch(descriptor, &owner, &native)) {
            return File::Ioctl(descriptor, request, argument);
        }
        if (File::IoctlRequestMalformed(request)) return Fail(25);
        if (!argument && (request & 0x1fff0000 || !(request & 0x20000000))) return Fail(14);
        int error = 0;
        if (!KernelControlSocketIoctl_nid_no_patch(descriptor, command, argument, &error)) return Fail(9);
        return error == 0 ? 0 : Fail(error);
    }
    if (File::IoctlRequestMalformed(request)) return Fail(25);
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    const auto command = static_cast<std::uint32_t>(request);
    if (command == 0x20006601 || command == 0x20006602) {
        std::lock_guard registry(socketsMutex);
        if (command == 0x20006601) closeOnExecDescriptors.insert(descriptor);
        else closeOnExecDescriptors.erase(descriptor);
        return 0;
    }
    if (!argument && (request & 0x1fff0000 || !(request & 0x20000000))) return Fail(14);
    if (request != 0x8004667e && request != 0x4004667f) {
        const char group = static_cast<char>((request >> 8) & 0xff);
        const bool internet = socket->family == 2 || socket->family == 28;
        return Fail(internet && group != 'i' && group != 'r' && group != 'f' ? 49 : 25);
    }
    std::lock_guard lock(socket->modeMutex);
    if (request == 0x4004667f && socket->receiveShut) {
        const int zero = 0;
        return GuestCopyOut_nid_no_patch(argument, &zero, sizeof(zero)) ? 0 : Fail(14);
    }
    unsigned long value = 0;
    if (request == 0x8004667e) {
        int enable = 0;
        if (!GuestCopyIn_nid_no_patch(&enable, argument, sizeof(enable))) return Fail(14);
        value = enable != 0;
    }
#ifdef _WIN32
    const auto result = ioctlsocket(socket->value, request == 0x8004667e ? FIONBIO : FIONREAD, &value);
#else
    int nativeValue = static_cast<int>(value);
    const auto result = request == 0x8004667e ? ::ioctl(socket->value, FIONBIO, &nativeValue) : QueuedBytes(*socket, nativeValue);
    value = static_cast<unsigned long>(nativeValue);
#endif
    if (result) return Fail(NativeError());
    if (request == 0x8004667e) socket->options.nonblocking = value != 0;
    if (request == 0x4004667f) {
        const int queued = static_cast<int>(value);
        if (!GuestCopyOut_nid_no_patch(argument, &queued, sizeof(queued))) return Fail(14);
    }
    return 0;
}
int APS5_VABI _ioctl_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    return ioctl_nid_postfix(descriptor, request, argument);
}
std::int64_t APS5_VABI sendto_nid_postfix(int descriptor, const void* buffer, std::uint64_t length,
    int flags, const void* address, std::uint32_t addressLength) {
    std::array<unsigned char, 255> destinationCopy;
    if (address) {
        if (const int error = AddressLengthError(address, addressLength)) return Fail(error);
        if (!GuestCopyIn_nid_no_patch(destinationCopy.data(), address, addressLength)) return Fail(14);
        address = destinationCopy.data();
    }
    if (const auto [forwards, family] = ForwardsFor(descriptor); forwards != nullptr) {
        if ((flags & ~GuestSendFlags) != 0) return Fail(45);
        if (length > INT_MAX) return Fail(40);
        if (!buffer && length) return Fail(14);
        sockaddr_storage checked{};
        socklen_t checkedSize;
        if (address && !Address(family, address, addressLength, checked, checkedSize)) return -1;
        int error = 0;
        const auto sent = forwards->sendto(descriptor, buffer, length, flags, address, addressLength, &error);
        return error == 0 ? sent : Fail(error);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestSendFlags) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!address && SendBeforeConnect(*socket)) return Fail(57);
    if (RecordOverSendBuffer(*socket, length)) return Fail(40);
    if (!buffer && length) return Fail(14);
    bool sendShut = false;
    if (address && socket->family == GuestUnix) {
        std::lock_guard lock(socket->modeMutex);
        sendShut = socket->sendShut;
    }
    const bool paired = address && socket->family == GuestUnix && (socket->type != 2 || sendShut || !socket->peer.expired());
    if (paired && socket->type == 2 && !sendShut) return Fail(56);
    if (const int waited = WaitForTransfer(*socket, true, flags)) return Fail(waited);
    int result;
    if (!address || paired || (socket->type == 1 && PeerConnected(*socket))) {
        if (!address && addressLength != 0) return Fail(22);
#ifdef _WIN32
        if (SourceFaults(buffer, length)) return Fail(14);
#endif
        result = ::send(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), SocketSendFlags(*socket, flags));
    } else {
        sockaddr_storage native{};
        socklen_t size;
        if (!Address(socket->family, address, addressLength, native, size)) return socket->type == 2 && SourceFaults(buffer, length) ? Fail(14) : -1;
        if (const auto answered = ZeroPortSend(*socket, native, length); answered != NotAnswered)
            return socket->type == 2 && SourceFaults(buffer, length) ? Fail(14) : answered;
#ifdef _WIN32
        LoopbackForUnspecifiedInet(native);
        if (SourceFaults(buffer, length)) return Fail(14);
#endif
        result = static_cast<int>(::sendto(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), SocketSendFlags(*socket, flags),
            reinterpret_cast<sockaddr*>(&native), size));
    }
    if (result >= 0) return result;
    const int error = SendError(*socket);
    return socket->type == 2 && address && SourceFaults(buffer, length) ? Fail(14) : Fail(error);
}
std::int64_t APS5_VABI recvfrom_nid_postfix(int descriptor, void* buffer, std::uint64_t length,
    int flags, void* address, std::uint32_t* addressLength) {
    std::uint32_t capacity = 0;
    if (addressLength && !GuestCopyIn_nid_no_patch(&capacity, addressLength, sizeof(capacity))) return Fail(14);
    if (const auto* forwards = ForwardsFor(descriptor).forwards; forwards != nullptr) {
        if ((flags & ~GuestReceiveFlags) != 0) return Fail(45);
        if (length > INT_MAX) return Fail(40);
        if (!buffer && length) return Fail(14);
        unsigned char name[28]{};
        std::uint32_t reported = std::min<std::uint32_t>(capacity, sizeof(name));
        int error = 0;
        const auto received = forwards->recvfrom(descriptor, buffer, length, flags, address ? name : nullptr, address ? &reported : nullptr, &error);
        if (error != 0) return Fail(error);
        if (!addressLength) return received;
        return ReportAddress(address, addressLength, name, address ? reported : capacity) ? received : Fail(14);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestReceiveFlags) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    const GuestArena::HostWrite destination(length == 0 ? nullptr : buffer, static_cast<std::size_t>(length));
    if (!destination.Open()) return Fail(14);
    if (const int answered = ReceiveAnsweredHere(*socket, length); answered >= 0) {
        if (answered > 0) return Fail(answered);
        if (addressLength && !ReportAddress(address, addressLength, nullptr, address ? 0 : capacity)) return Fail(14);
        return 0;
    }
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    if (const int waited = WaitForTransfer(*socket, false, flags, descriptor)) return Fail(waited);
    if (ReceiveShut(*socket)) {
        if (addressLength && !ReportAddress(address, addressLength, nullptr, address ? 0 : capacity)) return Fail(14);
        return 0;
    }
    std::uint64_t limit = length;
#ifdef _WIN32
    if (length != 0 && (limit = GuestPrefix(buffer, length, WritableProtection)) < length) {
        int error = 0;
        if (!PendingFits(*socket, buffer, limit, flags, error)) {
            DropDatagram(*socket, flags);
            return Fail(14);
        }
        if (error) return Fail(error);
    }
#endif
    std::int64_t result = ::recvfrom(socket->value, static_cast<char*>(buffer), static_cast<int>(limit),
        NativeReceiveFlags(flags, socket->type), reinterpret_cast<sockaddr*>(&native), &size);
#ifdef _WIN32
    if (result < 0 && socket->type == 2 && WSAGetLastError() == WSAEMSGSIZE) result = static_cast<int>(limit);
#endif
    if (result < 0) {
        const int error = NativeError();
#ifdef __APPLE__
        if (error == 14) DropFaultedDatagram(*socket, flags);
#endif
        return Fail(error);
    }
    if (!addressLength) return result;
    unsigned char name[28]{};
    std::uint32_t reported = address ? 0 : capacity;
    if (address && socket->family == GuestUnix) {
        if (socket->type != 1) {
            reported = capacity;
            UnnamedAddress(name, &reported);
        }
    } else if (address && (native.ss_family == AF_INET || native.ss_family == AF_INET6)) {
        reported = capacity;
        GuestAddress(native, name, &reported);
    }
    return ReportAddress(address, addressLength, name, reported) ? result : Fail(14);
}

static_assert(sizeof(GuestMsghdr) == 48 && offsetof(GuestMsghdr, iov) == 16 && offsetof(GuestMsghdr, control) == 32 && offsetof(GuestMsghdr, flags) == 44);
static std::int64_t MessageLength(const GuestMsghdr* message) {
    if (!message) return Fail(14);
    if (message->iovLength < 0 || message->iovLength > 1024) return Fail(40);
    if (message->iovLength && !message->iov) return Fail(14);
    std::uint64_t total = 0;
    for (int i = 0; i < message->iovLength; ++i) {
        const auto& entry = message->iov[i];
        if (!entry.base && entry.length) return Fail(14);
        if (entry.length > INT_MAX - total) return Fail(22);
        total += entry.length;
    }
    return static_cast<std::int64_t>(total);
}
constexpr int GuestControlCloseOnExec = 0x40000;
#ifndef _WIN32
constexpr int GuestSocketLevel = 0xffff;
constexpr int GuestRights = 1;
constexpr int GuestControlTruncated = 0x20;
constexpr std::size_t GuestControlHeader = 16;
constexpr std::size_t MostPassedDescriptors = 253;
std::size_t GuestControlSpace(std::size_t bytes) {
    return GuestControlHeader + ((bytes + 7u) & ~std::size_t{7});
}
int PassedDescriptors(const GuestMsghdr& message, std::vector<int>& descriptors) {
    const auto* control = static_cast<const unsigned char*>(message.control);
    std::size_t left = message.controlLength;
    std::size_t at = 0;
    while (left != 0) {
        std::uint32_t length = 0;
        int level = 0;
        int type = 0;
        if (left < 12) return 22;
        std::memcpy(&length, control + at, sizeof(length));
        std::memcpy(&level, control + at + 4, sizeof(level));
        std::memcpy(&type, control + at + 8, sizeof(type));
        if (level != GuestSocketLevel || length > left || length < GuestControlHeader) return 22;
        if (type != GuestRights) throw std::runtime_error("sendmsg: control message type " + std::to_string(type) + " is not implemented");
        const std::size_t count = (length - GuestControlHeader) / sizeof(int);
        for (std::size_t index = 0; index < count; ++index) {
            int passed = -1;
            std::memcpy(&passed, control + at + GuestControlHeader + index * sizeof(int), sizeof(passed));
            if (passed < 0 || !DescriptorIsOpen_nid_no_patch(passed)) return 9;
            if (IsKqueueDescriptor_nid_no_patch(passed)) return 45;
            File::RefuseArchivedFile(passed, "sendmsg");
            const char* emulated = File::IsRandomDevice(passed) ? "a random device" : File::IsDirectoryDescriptor(passed) ? "a directory" : IsJitSharedMemory(passed) ? "a JIT shared memory" : nullptr;
            if (emulated != nullptr) NotImplemented_nid_no_patch(("sendmsg: passing " + std::string(emulated) + " descriptor, whose state is libkernel's, fd=" + std::to_string(passed)).c_str());
            descriptors.push_back(passed);
        }
        const auto space = GuestControlSpace(length - GuestControlHeader);
        if (space >= left) break;
        at += space;
        left -= space;
    }
    return 0;
}
static bool ReadGuest(void* destination, const void* source, std::size_t bytes);
std::int64_t SendPassing(Socket& socket, const GuestMsghdr& message, std::vector<char>& data, int flags) {
    constexpr std::size_t MostControlBytes = 2048;
    if (message.controlLength < 12 || message.controlLength > MostControlBytes) return Fail(22);
    std::vector<unsigned char> controlCopy(message.controlLength);
    if (!ReadGuest(controlCopy.data(), message.control, controlCopy.size())) return Fail(14);
    GuestMsghdr checked = message;
    checked.control = controlCopy.data();
    std::vector<int> descriptors;
    if (const int error = PassedDescriptors(checked, descriptors)) return Fail(error);
    if ((flags & ~GuestSendFlags) != 0) return Fail(45);
    if (message.name) return Fail(47);
    if (!descriptors.empty() && data.empty() && socket.type == 1) NotImplemented_nid_no_patch("sendmsg: SCM_RIGHTS without data on a stream socket");
    if (const int waited = WaitForTransfer(socket, true, flags)) return Fail(waited);
    iovec vector{data.data(), data.size()};
    msghdr host{};
    host.msg_iov = &vector;
    host.msg_iovlen = 1;
    std::vector<char> control(CMSG_SPACE(descriptors.size() * sizeof(int)));
    if (!descriptors.empty()) {
        host.msg_control = control.data();
        host.msg_controllen = control.size();
        auto* header = CMSG_FIRSTHDR(&host);
        header->cmsg_level = SOL_SOCKET;
        header->cmsg_type = SCM_RIGHTS;
        header->cmsg_len = CMSG_LEN(descriptors.size() * sizeof(int));
        std::memcpy(CMSG_DATA(header), descriptors.data(), descriptors.size() * sizeof(int));
    }
    const auto result = ::sendmsg(socket.value, &host, SocketSendFlags(socket, flags));
    return result < 0 ? Fail(SendError(socket)) : result;
}
int GuestFromHostSocket(int domain) {
    return domain == AF_UNIX ? GuestUnix : domain == AF_INET ? 2 : domain == AF_INET6 ? 28 : -1;
}
int GuestFromHostSocketType(int type) {
    return type == SOCK_STREAM ? 1 : type == SOCK_DGRAM ? 2 : type == SOCK_SEQPACKET ? 5 : -1;
}
std::shared_ptr<Socket> PassedSocket(int native) {
    struct stat status{};
    if (::fstat(native, &status) != 0) throw std::runtime_error("recvmsg: a received descriptor cannot be queried, errno " + std::to_string(errno));
    if (!S_ISSOCK(status.st_mode)) return nullptr;
    sockaddr_storage address{};
    socklen_t addressLength = sizeof(address);
    int type = 0;
    socklen_t length = sizeof(type);
    if (::getsockname(native, reinterpret_cast<sockaddr*>(&address), &addressLength) != 0 || ::getsockopt(native, SOL_SOCKET, SO_TYPE, &type, &length) != 0)
        throw std::runtime_error("recvmsg: a received socket cannot be queried, errno " + std::to_string(errno));
    const int domain = address.ss_family;
    const int family = GuestFromHostSocket(domain);
    const int guestType = GuestFromHostSocketType(type);
    if (family < 0 || guestType < 0) throw std::runtime_error("recvmsg: a received socket of host domain " + std::to_string(domain) + " and type " + std::to_string(type) + " is not supported");
    auto socket = std::make_shared<Socket>(native, family, guestType);
    sockaddr_storage peer{};
    socklen_t peerLength = sizeof(peer);
    socket->connectStarted = guestType != 1 || ::getpeername(native, reinterpret_cast<sockaddr*>(&peer), &peerLength) == 0;
    return socket;
}
class ReceivedRights {
public:
    ReceivedRights() = default;
    ReceivedRights(const ReceivedRights&) = delete;
    ReceivedRights& operator=(const ReceivedRights&) = delete;
    ~ReceivedRights() { Close(); }
    void Hold(int native) { natives.push_back(native); }
    int Install(bool closeOnExec) {
        while (!natives.empty()) {
            auto socket = PassedSocket(natives.front());
            const int native = natives.front();
            natives.erase(natives.begin());
            if (!socket) {
                numbers.push_back(native);
                continue;
            }
            const int number = ::Install(std::move(socket));
            if (number < 0) return *__error_nid_postfix();
            numbers.push_back(number);
            if (closeOnExec) {
                std::lock_guard lock(socketsMutex);
                closeOnExecDescriptors.insert(number);
            }
        }
        return 0;
    }
    const std::vector<int>& Numbers() const { return numbers; }
    void Close() {
        for (const int number : numbers) {
            if (GuestSockets::IsOpen(number)) GuestSockets::Close(number);
            else ::close(number);
        }
        for (const int native : natives) ::close(native);
        numbers.clear();
        natives.clear();
    }
    void Commit() {
        numbers.clear();
        natives.clear();
    }
private:
    std::vector<int> natives;
    std::vector<int> numbers;
};
#endif
static bool ReadGuest(void* destination, const void* source, std::size_t bytes);
std::int64_t APS5_VABI sendmsg_nid_postfix(int descriptor, const GuestMsghdr* message, int flags) {
    GuestMsghdr header{};
    if (!message || !ReadGuest(&header, message, sizeof(header))) return Fail(14);
    if (header.iovLength < 0 || header.iovLength > 1024) return Fail(40);
    std::vector<KernelIovec> vectors(static_cast<std::size_t>(header.iovLength));
    if (!ReadGuest(vectors.data(), header.iov, vectors.size() * sizeof(KernelIovec))) return Fail(14);
    header.iov = vectors.data();
    const auto total = MessageLength(&header);
    if (total < 0) return -1;
    unsigned char name[255];
    if (header.name && !ReadGuest(name, header.name, std::min<std::size_t>(header.nameLength, sizeof(name)))) return Fail(14);
    if (!Lookup(descriptor)) return -1;
    const bool control = header.control && header.controlLength;
#ifdef _WIN32
    if (control) throw std::runtime_error("sendmsg: control data is not supported on Windows");
#else
    std::shared_ptr<Socket> passing;
    if (control) {
        passing = Lookup(descriptor);
        if (!passing) return -1;
        if (passing->family != GuestUnix) throw std::runtime_error("sendmsg: control data on an IP socket is not implemented");
    }
#endif
    std::vector<char> buffer;
    try {
        buffer.resize(static_cast<std::size_t>(total));
    } catch (const std::bad_alloc&) {
        return Fail(55);
    }
    std::size_t offset = 0;
    for (const auto& vector : vectors) {
        if (!ReadGuest(buffer.data() + offset, vector.base, vector.length)) return Fail(14);
        offset += vector.length;
    }
#ifndef _WIN32
    if (passing) return SendPassing(*passing, header, buffer, flags);
#endif
    return sendto_nid_postfix(descriptor, buffer.data(), buffer.size(), flags, header.name ? name : nullptr, header.name ? header.nameLength : 0u);
}
#ifdef _WIN32
}

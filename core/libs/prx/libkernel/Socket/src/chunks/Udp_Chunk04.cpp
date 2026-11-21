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

static bool GuestPagesAllow(std::uintptr_t start, std::size_t bytes, DWORD allowed) {
    return GuestPrefix(reinterpret_cast<const void*>(start), bytes, allowed) == bytes;
}
#elif defined(__linux__)
static bool Transferred(ssize_t copied, std::size_t bytes, const char* call, int saved) {
    if (copied >= 0) return static_cast<std::size_t>(copied) == bytes;
    if (errno == EFAULT) {
        errno = saved;
        return false;
    }
    throw std::runtime_error(std::string("recvmsg: ") + call + " failed with errno " + std::to_string(errno));
}
#endif
static bool GuestRange(const void* pointer, std::size_t bytes) {
    const auto start = reinterpret_cast<std::uintptr_t>(pointer);
    return start != 0 && bytes <= UINTPTR_MAX - start;
}
static bool ReadGuest(void* destination, const void* source, std::size_t bytes) {
    if (bytes == 0) return true;
    if (!GuestRange(source, bytes)) return false;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, bytes);
#ifdef _WIN32
    if (!GuestPagesAllow(reinterpret_cast<std::uintptr_t>(source), bytes,
            PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) return false;
    std::memcpy(destination, source, bytes);
    return true;
#elif defined(__linux__)
    iovec local{destination, bytes};
    iovec remote{const_cast<void*>(source), bytes};
    const int saved = errno;
    return Transferred(::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0), bytes, "process_vm_readv", saved);
#elif defined(__APPLE__)
    mach_vm_size_t copied = 0;
    return ::mach_vm_read_overwrite(::mach_task_self(), reinterpret_cast<mach_vm_address_t>(source), bytes,
        reinterpret_cast<mach_vm_address_t>(destination), &copied) == KERN_SUCCESS && copied == bytes;
#else
    std::memcpy(destination, source, bytes);
    return true;
#endif
}
static bool WriteGuest(void* destination, const void* source, std::size_t bytes) {
    if (bytes == 0) return true;
    if (!GuestRange(destination, bytes)) return false;
    const GuestArena::HostWrite guard(destination, bytes);
    if (!guard.Open()) return false;
#ifdef _WIN32
    if (!GuestPagesAllow(reinterpret_cast<std::uintptr_t>(destination), bytes,
            PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) return false;
    std::memcpy(destination, source, bytes);
    return true;
#elif defined(__linux__)
    iovec local{const_cast<void*>(source), bytes};
    iovec remote{destination, bytes};
    const int saved = errno;
    return Transferred(::process_vm_writev(::getpid(), &local, 1, &remote, 1, 0), bytes, "process_vm_writev", saved);
#elif defined(__APPLE__)
    return ::mach_vm_write(::mach_task_self(), reinterpret_cast<mach_vm_address_t>(destination),
        reinterpret_cast<vm_offset_t>(source), static_cast<mach_msg_type_number_t>(bytes)) == KERN_SUCCESS;
#else
    std::memcpy(destination, source, bytes);
    return true;
#endif
}
static std::vector<unsigned char> ArrivalTimestamps(const Socket& socket, int keptFlags) {
    std::vector<unsigned char> records;
    if (socket.type != 2 || (socket.family != 2 && socket.family != 28)) return records;
    const bool binary = socket.family == 2 && (keptFlags & 0x2000) != 0;
    const bool micro = (keptFlags & 0x400) != 0;
    if (!binary && !micro) return records;
    KernelTimespec now{};
    if (clock_gettime_nid_postfix(0, &now) != 0) throw std::runtime_error("recvmsg: reading CLOCK_REALTIME for the arrival time failed");
    const auto append = [&records](int type, std::int64_t seconds, std::uint64_t fraction) {
        constexpr std::uint32_t length = 32;
        constexpr int level = 0xffff;
        const auto at = records.size();
        records.resize(at + length);
        std::memcpy(records.data() + at, &length, sizeof(length));
        std::memcpy(records.data() + at + 4, &level, sizeof(level));
        std::memcpy(records.data() + at + 8, &type, sizeof(type));
        std::memcpy(records.data() + at + 16, &seconds, sizeof(seconds));
        std::memcpy(records.data() + at + 24, &fraction, sizeof(fraction));
    };
    const auto seconds = static_cast<std::int64_t>(now.tv_sec);
    const auto nanoseconds = static_cast<std::uint64_t>(now.tv_nsec);
    if (binary) append(4, seconds, nanoseconds * 18446744073ull);
    if (micro) append(2, seconds, nanoseconds / 1000);
    return records;
}
std::int64_t APS5_VABI recvmsg_nid_postfix(int descriptor, GuestMsghdr* message, int flags) {
    GuestMsghdr header{};
    if (!message || !ReadGuest(&header, message, sizeof(header))) return Fail(14);
    if (header.iovLength < 0 || header.iovLength > 1024) return Fail(40);
    std::vector<KernelIovec> vectors(static_cast<std::size_t>(header.iovLength));
    if (!ReadGuest(vectors.data(), header.iov, vectors.size() * sizeof(KernelIovec))) return Fail(14);
    header.iov = vectors.data();
    const auto total = MessageLength(&header);
    if (total < 0) return -1;
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    const std::size_t controlCapacity = header.controlLength;
#ifdef _WIN32
    const bool passing = false;
#else
    const bool passing = socket->family == GuestUnix && header.control && header.controlLength && (flags & GuestPeek) == 0;
#endif
    if ((flags & ~(GuestReceiveFlags | (passing ? GuestControlCloseOnExec : 0))) != 0) return Fail(45);
    const bool records = socket->type == 2 || socket->type == 5;
    std::vector<char> buffer;
    try {
        buffer.resize(static_cast<std::size_t>(total) + (records ? 1u : 0u));
    } catch (const std::bad_alloc&) {
        return Fail(55);
    }
    sockaddr_storage native{};
    socklen_t size = 0;
    std::int64_t received = 0;
#ifndef _WIN32
    ReceivedRights rights;
    bool controlTruncated = false;
#endif
    bool shut = ReceiveShut(*socket);
    std::vector<unsigned char> stamps;
    if (!shut && (total != 0 || socket->type == 1 || ReadableNow(*socket))) {
        size = sizeof(native);
        if (const int waited = WaitForTransfer(*socket, false, flags, descriptor)) return Fail(waited);
        shut = ReceiveShut(*socket);
#ifndef _WIN32
        if (passing) {
            std::vector<char> control(CMSG_SPACE(MostPassedDescriptors * sizeof(int)));
            iovec vector{buffer.data(), buffer.size()};
            msghdr host{};
            host.msg_name = &native;
            host.msg_namelen = size;
            host.msg_iov = &vector;
            host.msg_iovlen = 1;
            host.msg_control = control.data();
            host.msg_controllen = control.size();
#ifdef MSG_CMSG_CLOEXEC
            constexpr int receiveCloseOnExec = MSG_CMSG_CLOEXEC;
#else
            constexpr int receiveCloseOnExec = 0;
#endif
            received = ::recvmsg(socket->value, &host, NativeReceiveFlags(flags, socket->type) | ((flags & GuestControlCloseOnExec) != 0 ? receiveCloseOnExec : 0));
            if (received < 0) return Fail(NativeError());
            size = host.msg_namelen;
            controlTruncated = (host.msg_flags & MSG_CTRUNC) != 0;
            for (auto* header = CMSG_FIRSTHDR(&host); header; header = CMSG_NXTHDR(&host, header)) {
                if (header->cmsg_level != SOL_SOCKET || header->cmsg_type != SCM_RIGHTS) continue;
                const auto count = (header->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                for (std::size_t index = 0; index < count; ++index) {
                    int handed = -1;
                    std::memcpy(&handed, CMSG_DATA(header) + index * sizeof(int), sizeof(handed));
                    if constexpr (receiveCloseOnExec == 0) {
                        if ((flags & GuestControlCloseOnExec) != 0) ::fcntl(handed, F_SETFD, FD_CLOEXEC);
                    }
                    rights.Hold(handed);
                }
            }
            if (const int error = rights.Install((flags & GuestControlCloseOnExec) != 0)) return Fail(error);
        } else
#endif
        if (!shut) received = ::recvfrom(socket->value, buffer.data(), static_cast<int>(buffer.size()),
            NativeReceiveFlags(flags, socket->type), reinterpret_cast<sockaddr*>(&native), &size);
        if (received < 0) {
#ifdef _WIN32
            if (!records || WSAGetLastError() != WSAEMSGSIZE) return Fail(NativeError());
            received = static_cast<std::int64_t>(buffer.size());
#else
            return Fail(NativeError());
#endif
        }
        if (header.control && !(received == 0 && ReceiveShut(*socket))) {
            int keptFlags = 0;
            {
                std::lock_guard lock(socket->modeMutex);
                keptFlags = socket->options.keptFlags;
            }
            stamps = ArrivalTimestamps(*socket, keptFlags);
        }
    }
    header.flags = flags;
    if (received > total) {
        received = total;
        header.flags |= 0x10;
    }
    std::size_t offset = 0;
    for (const auto& vector : vectors) {
        if (offset >= static_cast<std::size_t>(received)) break;
        const auto count = std::min<std::size_t>(vector.length, static_cast<std::size_t>(received) - offset);
        if (!WriteGuest(vector.base, buffer.data() + offset, count)) return Fail(14);
        offset += count;
    }
    if (header.name) {
        unsigned char address[28]{};
        if (socket->family == GuestUnix) {
            if (socket->type == 1 || shut) header.nameLength = 0; else UnnamedAddress(address, &header.nameLength);
        } else if (size > 0 && (native.ss_family == AF_INET || native.ss_family == AF_INET6)) {
            GuestAddress(native, address, &header.nameLength);
        } else {
            header.nameLength = 0;
        }
        if (!WriteGuest(header.name, address, header.nameLength)) return Fail(14);
        if (!WriteGuest(&message->nameLength, &header.nameLength, sizeof(header.nameLength))) return Fail(14);
    }
    header.controlLength = 0;
#ifndef _WIN32
    const auto& passed = rights.Numbers();
    if (passing && !passed.empty()) {
        const auto bytes = passed.size() * sizeof(int);
        std::vector<unsigned char> record(GuestControlSpace(bytes), 0);
        const auto length = static_cast<std::uint32_t>(GuestControlHeader + bytes);
        std::memcpy(record.data(), &length, sizeof(length));
        std::memcpy(record.data() + 4, &GuestSocketLevel, sizeof(GuestSocketLevel));
        std::memcpy(record.data() + 8, &GuestRights, sizeof(GuestRights));
        std::memcpy(record.data() + GuestControlHeader, passed.data(), bytes);
        const bool fits = record.size() <= controlCapacity;
        if (fits && WriteGuest(header.control, record.data(), record.size())) {
            header.controlLength = static_cast<std::uint32_t>(record.size());
        } else {
            if (fits) return Fail(14);
            rights.Close();
            controlTruncated = true;
        }
    }
    if (controlTruncated) header.flags |= GuestControlTruncated;
#endif
    if (!stamps.empty()) {
        const std::size_t copied = std::min(stamps.size(), controlCapacity);
        if (copied != 0 && !WriteGuest(header.control, stamps.data(), copied)) return Fail(14);
        if (copied < stamps.size()) header.flags |= 0x20;
        header.controlLength = static_cast<std::uint32_t>(copied);
    }
    constexpr auto results = offsetof(GuestMsghdr, controlLength);
    if (!WriteGuest(reinterpret_cast<char*>(message) + results, reinterpret_cast<const char*>(&header) + results, sizeof(header) - results)) return Fail(14);
#ifndef _WIN32
    rights.Commit();
#endif
    return received;
}

static_assert(sizeof(GuestSendfileHeaders) == 32 && offsetof(GuestSendfileHeaders, trailers) == 16);

static bool SendfileIovecs(const KernelIovec* iov, int count) {
    if (!iov) return true;
    if (count < 0 || count > 1024) return false;
    std::uint64_t total = 0;
    for (int i = 0; i < count; ++i) {
        if (iov[i].length > INT_MAX - total) return false;
        total += iov[i].length;
    }
    return true;
}
static int SendAll(const Socket& socket, const void* data, std::uint64_t length, std::int64_t& sent) {
    if (!data && length) return 14;
    const char* bytes = static_cast<const char*>(data);
    while (length) {
        const auto result = ::send(socket.value, bytes, static_cast<int>(std::min<std::uint64_t>(length, INT_MAX)), NativeSendFlags(GuestNoSignal));
        if (result < 0) return NativeError();
        bytes += result;
        length -= static_cast<std::uint64_t>(result);
        sent += result;
    }
    return 0;
}
static int SendIovecs(const Socket& socket, const KernelIovec* iov, int count, std::int64_t& sent) {
    for (int i = 0; iov && i < count; ++i)
        if (const int error = SendAll(socket, iov[i].base, iov[i].length, sent)) return error;
    return 0;
}
int APS5_VABI sendfile_nid_postfix(int file, int descriptor, std::int64_t offset, std::uint64_t length,
    const GuestSendfileHeaders* headers, std::int64_t* sbytes, int flags) {
    (void)flags;
    if (offset < 0) return Fail(22);
    GuestSendfileHeaders localHeaders{};
    std::vector<KernelIovec> headerIovecs;
    std::vector<KernelIovec> trailerIovecs;
    if (headers != nullptr) {
        if (!GuestCopyIn_nid_no_patch(&localHeaders, headers, sizeof(localHeaders))) return Fail(14);
        if (localHeaders.headers != nullptr) {
            if (localHeaders.headerCount < 0 || localHeaders.headerCount > 1024) return Fail(22);
            if (localHeaders.headerCount > 0) {
                try {
                    headerIovecs.resize(localHeaders.headerCount);
                } catch (const std::bad_alloc&) {
                    return Fail(12);
                }
                if (!GuestCopyIn_nid_no_patch(headerIovecs.data(), localHeaders.headers, headerIovecs.size() * sizeof(KernelIovec)))
                    return Fail(14);
                if (!SendfileIovecs(headerIovecs.data(), localHeaders.headerCount)) return Fail(22);
            }
        }
        if (localHeaders.trailers != nullptr) {
            if (localHeaders.trailerCount < 0 || localHeaders.trailerCount > 1024) return Fail(22);
            if (localHeaders.trailerCount > 0) {
                try {
                    trailerIovecs.resize(localHeaders.trailerCount);
                } catch (const std::bad_alloc&) {
                    return Fail(12);
                }
                if (!GuestCopyIn_nid_no_patch(trailerIovecs.data(), localHeaders.trailers, trailerIovecs.size() * sizeof(KernelIovec)))
                    return Fail(14);
                if (!SendfileIovecs(trailerIovecs.data(), localHeaders.trailerCount)) return Fail(22);
            }
        }
    }
    std::int64_t sent = 0;
    const auto finish = [&](int error) {
        if (sbytes != nullptr) GuestCopyOut_nid_no_patch(sbytes, &sent, sizeof(sent));
        return error ? Fail(error) : 0;
    };
    if (DescriptorIsOpen_nid_no_patch(file) && !File::IsVnode(file) && !File::IsSharedMemoryDescriptor_nid_no_patch(file)) return finish(22);
    std::vector<char> chunk;
    try {
        chunk.resize(0x10000);
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
    const auto read = [&](std::int64_t position, std::uint64_t remaining) {
        const auto result = sceKernelPread(file, chunk.data(), static_cast<std::size_t>(std::min<std::uint64_t>(remaining, chunk.size())), position);
        return result < 0 ? -(result & 0xffff) : result;
    };
    std::uint64_t remaining = length ? length : UINT64_MAX;
    auto received = read(offset, remaining);
    if (received == -9) return Fail(9);
    if (received < 0) return finish(received == -21 || received == -29 ? 22 : static_cast<int>(-received));
    const auto socket = Lookup(descriptor);
    if (!socket) return finish(*__error_nid_postfix());
    if (socket->type != 1) return finish(22);
    sockaddr_storage peer{};
    socklen_t size = sizeof(peer);
    if (::getpeername(socket->value, reinterpret_cast<sockaddr*>(&peer), &size)) return finish(57);
    if (!headerIovecs.empty())
        if (const int error = SendIovecs(*socket, headerIovecs.data(), static_cast<int>(headerIovecs.size()), sent)) return finish(error);
    while (received > 0) {
        if (const int error = SendAll(*socket, chunk.data(), static_cast<std::uint64_t>(received), sent)) return finish(error);
        offset += received;
        remaining -= static_cast<std::uint64_t>(received);
        if (remaining == 0) break;
        received = read(offset, remaining);
        if (received < 0) return finish(static_cast<int>(-received));
    }
    if (!trailerIovecs.empty())
        if (const int error = SendIovecs(*socket, trailerIovecs.data(), static_cast<int>(trailerIovecs.size()), sent)) return finish(error);
    return finish(0);
}
static bool SocketStillHeld(int descriptor, const void* owner) {
    if (auto socket = Find(descriptor)) return socket.get() == owner;
    std::shared_ptr<void> current;
    std::uintptr_t value = 0;
    return KernelFindSocketNative_nid_no_patch(descriptor, &current, &value) && current.get() == owner;
}

[[noreturn]] static void UnsupportedPollEvents(short events, const char* kind) {
    throw std::runtime_error("poll: unsupported event flags " + std::to_string(events) + " on " + kind);
}

static int PollDescriptors(GuestPollDescriptor* descriptors, std::uint32_t count, int timeout) {
    if (timeout < -1) return Fail(22);
    if (count && !descriptors) return Fail(14);
    std::vector<std::shared_ptr<void>> held;
    std::vector<NativePollDescriptor> native;
    std::vector<std::uint32_t> owners;
    std::vector<std::uint32_t> pipes;
    std::vector<std::uint32_t> kqueues;
    std::vector<int> readers;
    std::vector<std::pair<std::size_t, std::shared_ptr<Socket>>> guestSockets;
    std::vector<std::size_t> forwardedSockets;
    int ready = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        auto& entry = descriptors[i];
        entry.revents = 0;
        if (entry.descriptor < 0) continue;
        const bool unknownEvents = (entry.events & ~GuestPollAccepted) != 0;
        std::shared_ptr<void> owner;
        std::uintptr_t value = 0;
        if (auto socket = Find(entry.descriptor)) {
            if ((entry.events & GuestPollIgnoreEof) != 0) UnsupportedPollEvents(entry.events, "a socket");
            value = static_cast<std::uintptr_t>(socket->value);
            if (entry.events & GuestPollReadable) readers.push_back(entry.descriptor);
            guestSockets.emplace_back(native.size(), socket);
            owner = std::move(socket);
        } else if (!KernelFindSocketNative_nid_no_patch(entry.descriptor, &owner, &value)) {
            if (!DescriptorIsOpen_nid_no_patch(entry.descriptor)) {
                entry.revents = GuestPollInvalid;
                ++ready;
                continue;
            }
            if (IsKqueueDescriptor_nid_no_patch(entry.descriptor)) {
                kqueues.push_back(i);
                continue;
            }
            if (File::IsRandomDevice(entry.descriptor)) {
                if (unknownEvents) UnsupportedPollEvents(entry.events, "the random device");
                const short readable = static_cast<short>(entry.events & GuestPollReadable);
                entry.revents = readable != 0 ? readable : entry.events;
                if (entry.revents) ++ready;
                continue;
            }
            if (File::IsVnode(entry.descriptor)) {
                if (unknownEvents) UnsupportedPollEvents(entry.events, "a file or directory");
                entry.revents = static_cast<short>(entry.events & GuestPollVnodeReady);
                if (entry.revents) ++ready;
                continue;
            }
            if (entry.descriptor <= 2) {
                if (unknownEvents) UnsupportedPollEvents(entry.events, "a standard descriptor");
                entry.revents = static_cast<short>(entry.descriptor == 0 ? 0 : entry.events & GuestPollWritable);
                if (entry.revents) ++ready;
                continue;
            }
#ifdef _WIN32
            short revents = 0;
            if (!PipeReadiness(entry.descriptor, entry.events, revents))
#else
            if (!IsPipe(entry.descriptor, entry.events))
#endif
            {
                if (!DescriptorIsOpen_nid_no_patch(entry.descriptor)) {
                    entry.revents = GuestPollInvalid;
                    ++ready;
                    continue;
                }
                throw std::runtime_error("poll: descriptor " + std::to_string(entry.descriptor) + " is neither a socket, a pipe, a file, a directory nor a kqueue");
            }
            if ((entry.events & GuestPollIgnoreEof) != 0) UnsupportedPollEvents(entry.events, "a pipe");
            pipes.push_back(i);
            continue;
        } else {
            if (entry.events & GuestPollReadable) readers.push_back(entry.descriptor);
            if ((entry.events & GuestPollIgnoreEof) != 0) UnsupportedPollEvents(entry.events, "a socket");
            forwardedSockets.push_back(native.size());
        }
        NativePollDescriptor request{};
        request.fd = static_cast<decltype(request.fd)>(value);
        for (const auto& flag : RequestFlags)
            if (entry.events & flag.guest) request.events = static_cast<short>(request.events | flag.native);
        native.push_back(request);
        owners.push_back(i);
        held.push_back(std::move(owner));
    }
#ifndef _WIN32
    const std::size_t socketCount = native.size();
    for (const auto index : pipes) {
        NativePollDescriptor request{};
        request.fd = descriptors[index].descriptor;
        for (const auto& flag : RequestFlags)
            if (descriptors[index].events & flag.guest) request.events = static_cast<short>(request.events | flag.native);
        native.push_back(request);
        owners.push_back(index);
    }
#endif
    if (native.empty() && pipes.empty() && kqueues.empty()) {
        if (ready || timeout == 0) return ready;
        if (timeout < 0) throw std::runtime_error("poll: an infinite wait without sockets or pipes never returns");
        PreciseSleepUs(static_cast<unsigned long long>(timeout) * 1000);
        return 0;
    }
    const ReadWatch watch(readers);
    const std::size_t watched = native.size();
    if (!readers.empty()) {
        NativePollDescriptor request{};
        request.fd = ThreadWaker().value;
#ifdef _WIN32
        request.events = POLLRDNORM;
#else
        request.events = POLLIN;
#endif
        native.push_back(request);
    }
    std::vector<short> shutAt(watched, 0);
    const auto refreshShut = [&] {
        int shut = 0;
        const auto refresh = [&](std::size_t index, bool receiveShut, bool sendShut) {
            const short readable = receiveShut ? static_cast<short>(descriptors[owners[index]].events & GuestPollReadable) : short{0};
            const short hangUp = receiveShut && sendShut ? GuestPollHangUp : short{0};
            shutAt[index] = static_cast<short>(readable | hangUp);
            if (shutAt[index]) ++shut;
        };
        for (const auto& [index, socket] : guestSockets) {
            std::lock_guard lock(socket->modeMutex);
            refresh(index, socket->receiveShut, socket->sendShut);
        }
        for (const auto index : forwardedSockets) {
            const auto* socket = static_cast<const KernelSocketPoll::NativeOwner*>(held[index].get());
            const unsigned flags = socket->shutdownFlags.load();
            refresh(index, (flags & KernelSocketPoll::ReceiveShut) != 0, (flags & KernelSocketPoll::SendShut) != 0);
        }
        return shut;
    };
    const auto collect = [&](std::size_t i, bool pipe) {
        auto& entry = descriptors[owners[i]];
        if (entry.revents == GuestPollInvalid) {
            ++ready;
            return;
        }
#if defined(_WIN32) || defined(__APPLE__)
        if (!pipe) native[i].revents = ClosedStreamEvents(entry.descriptor, native[i].revents, native[i].events);
#endif
        short revents = native[i].revents;
        if (!pipe && (entry.events & GuestPollPriority) && (revents & NativePollPriority) == 0 && UrgentPending(native[i].fd)) {
            revents = static_cast<short>(revents | NativePollPriority | POLLRDBAND);
        }
        for (const auto& flag : RequestFlags)
            if ((entry.events & flag.guest) && (revents & flag.native))
                entry.revents = static_cast<short>(entry.revents | flag.guest);
        for (const auto& flag : StatusFlags)
            if (revents & flag.native) entry.revents = static_cast<short>(entry.revents | flag.guest);
        const auto socket = pipe ? nullptr : Find(entry.descriptor);
        const bool stream = socket && socket->type == 1;
        if ((pipe || stream) && (entry.revents & GuestPollError)) entry.revents = static_cast<short>((entry.revents & ~GuestPollError) | GuestPollHangUp);
        if ((pipe || stream) && (entry.revents & GuestPollHangUp))
            entry.revents = static_cast<short>((entry.revents & ~GuestPollWritable) | (entry.events & GuestPollReadable));
        if (!pipe) entry.revents = static_cast<short>(entry.revents | shutAt[i]);
#ifndef _WIN32
        if (entry.revents & GuestPollHangUp) entry.revents = static_cast<short>(entry.revents & ~GuestPollWritable);
#else
        if (shutAt[i] & GuestPollHangUp) entry.revents = static_cast<short>(entry.revents & ~GuestPollWritable);
#endif
        if (entry.revents) ++ready;
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout < 0 ? 0 : timeout);
    const auto repollAfterReuse = [&] {
        if (timeout < 0) return poll_nid_postfix(descriptors, count, -1);
        const auto left = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
        return poll_nid_postfix(descriptors, count, static_cast<int>(std::max<long long>(0, (left + 999) / 1000)));
    };
#ifdef __APPLE__
    bool waitsForUrgent = false;
    for (std::size_t i = 0; i < socketCount && !waitsForUrgent; ++i) {
        waitsForUrgent = (descriptors[owners[i]].events & GuestPollPriority) != 0;
    }
#else
    constexpr bool waitsForUrgent = false;
#endif
#ifdef _WIN32
    const bool fastSlices = !pipes.empty() || !kqueues.empty();
#else
    const bool fastSlices = !kqueues.empty() || waitsForUrgent;
#endif
    const bool sliced = fastSlices || !held.empty();
    const int step = fastSlices ? 1 : SocketCloseCheckMilliseconds;
    const auto anyUrgentPending = [&]() -> bool {
#ifdef __APPLE__
        for (std::size_t i = 0; i < socketCount; ++i) {
            const auto& entry = descriptors[owners[i]];
            if ((entry.events & GuestPollPriority) != 0 && (native[i].revents & NativePollPriority) == 0 && UrgentPending(native[i].fd)) return true;
        }
#endif
        return false;
    };
    for (;;) {
        int checkedReady = 0;
        bool closed = false;
        for (std::size_t i = 0; i < held.size(); ++i) {
            if (SocketStillHeld(descriptors[owners[i]].descriptor, held[i].get())) continue;
            if (Find(descriptors[owners[i]].descriptor)) return repollAfterReuse();
            descriptors[owners[i]].revents = GuestPollInvalid;
            closed = true;
        }
        const bool urgent = anyUrgentPending();
#ifdef _WIN32
        for (const auto index : pipes) {
            auto& entry = descriptors[index];
            PipeReadiness(entry.descriptor, entry.events, entry.revents);
            if (entry.revents) ++checkedReady;
        }
#endif
        for (const auto index : kqueues) {
            auto& entry = descriptors[index];
            const auto readable = KqueueReadable(entry.descriptor);
            if (!readable && DescriptorIsOpen_nid_no_patch(entry.descriptor)) {
                if (timeout < 0) return poll_nid_postfix(descriptors, count, -1);
                const auto left = std::chrono::duration_cast<std::chrono::microseconds>(deadline - std::chrono::steady_clock::now()).count();
                return poll_nid_postfix(descriptors, count, static_cast<int>(std::max<long long>(0, (left + 999) / 1000)));
            }
            entry.revents = !readable ? GuestPollInvalid : *readable ? static_cast<short>(entry.events & GuestPollReadable) : short{0};
            if (entry.revents) ++checkedReady;
        }
        int slice = 0;
        if (!ready && !checkedReady && !urgent && !closed && !refreshShut()) {
            if (!sliced) {
                const auto left = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
                slice = timeout < 0 ? -1 : static_cast<int>(std::clamp<long long>(left, 0, INT_MAX));
            } else if (timeout < 0) slice = step;
            else {
                const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
                slice = static_cast<int>(std::clamp<long long>(left, 0, step));
            }
        }
        if (!native.empty()) {
            for (auto& request : native) request.revents = 0;
#ifdef _WIN32
            if (WSAPoll(native.data(), static_cast<ULONG>(native.size()), slice) < 0) return Fail(NativeError());
#else
            if (::poll(native.data(), static_cast<nfds_t>(native.size()), slice) < 0) return Fail(NativeError());
#endif
        } else if (!ready && !checkedReady && !urgent) {
            const auto wake = std::chrono::steady_clock::now() + std::chrono::milliseconds(1);
            PreciseSleepUntil(timeout < 0 ? wake : std::min(deadline, wake));
        }
        for (std::size_t i = 0; i < held.size(); ++i) {
            if (SocketStillHeld(descriptors[owners[i]].descriptor, held[i].get())) continue;
            if (Find(descriptors[owners[i]].descriptor)) return repollAfterReuse();
            descriptors[owners[i]].revents = GuestPollInvalid;
            closed = true;
        }
        int socketReady = 0;
        for (std::size_t i = 0; i < watched; ++i)
            if (native[i].revents) ++socketReady;
        const bool woken = !readers.empty() && native[watched].revents != 0;
        if (woken) DrainWaker(native[watched].fd);
        const int shutReady = refreshShut();
        const bool expired = timeout >= 0 && std::chrono::steady_clock::now() >= deadline;
        if (ready || checkedReady || urgent || socketReady || shutReady || expired || closed || (!sliced && !woken)) {
            ready += checkedReady;
#ifdef _WIN32
            for (std::size_t i = 0; i < watched; ++i) collect(i, false);
#else
            for (std::size_t i = 0; i < watched; ++i) collect(i, i >= socketCount);
#endif
            return ready;
        }
    }
}

}

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

int APS5_VABI poll_nid_postfix(GuestPollDescriptor* descriptors, std::uint32_t count, int timeout) {
    if (timeout < -1) return Fail(22);
    if (count > 1024) {
        GuestResourceLimit limit{};
        if (getrlimit_nid_postfix(8, &limit) == 0 && static_cast<std::int64_t>(count) > limit.rlim_max) return Fail(22);
    }
    if (count && !descriptors) return Fail(14);
    std::vector<GuestPollDescriptor> local;
    try {
        local.resize(count);
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
    const auto bytes = local.size() * sizeof(GuestPollDescriptor);
    if (count && !GuestCopyIn_nid_no_patch(local.data(), descriptors, bytes)) return Fail(14);
    const int result = PollDescriptors(local.data(), count, timeout);
    if (result >= 0 && count && !GuestCopyOut_nid_no_patch(descriptors, local.data(), bytes)) return Fail(14);
    return result;
}
}

extern "C" bool GuestSocketReadGuest_nid_no_patch(void* destination, const void* source, std::size_t bytes) {
    return ::ReadGuest(destination, source, bytes);
}

std::int64_t GuestSockets::Read(int descriptor, void* buffer, std::size_t length) {
    if (length == 0) return IsOpen(descriptor) ? 0 : Fail(9);
    return recv_nid_postfix(descriptor, buffer, std::min<std::size_t>(length, INT_MAX), 0);
}

std::int64_t GuestSockets::Write(int descriptor, const void* buffer, std::size_t length) {
    return send_nid_postfix(descriptor, buffer, length, 0);
}

std::int64_t GuestSockets::Readv(int descriptor, const Iovec* iov, int iovcnt) {
    std::size_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].base == nullptr && iov[i].length != 0) return Fail(14);
        if (iov[i].length > static_cast<std::size_t>(INT_MAX) - total) return Fail(22);
        total += iov[i].length;
    }
    if (total == 0) return IsOpen(descriptor) ? 0 : Fail(9);
    std::vector<char> buffer;
    try {
        buffer.resize(total);
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
    const auto result = Read(descriptor, buffer.data(), total);
    if (result < 0) return result;
    std::size_t copied = 0;
    for (int i = 0; i < iovcnt && copied < static_cast<std::size_t>(result); ++i) {
        const auto amount = std::min(iov[i].length, static_cast<std::size_t>(result) - copied);
        if (amount != 0 && !GuestCopyOut_nid_no_patch(iov[i].base, buffer.data() + copied, amount)) return Fail(14);
        copied += amount;
    }
    return result;
}

std::int64_t GuestSockets::Writev(int descriptor, const Iovec* iov, int iovcnt) {
    std::size_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].base == nullptr && iov[i].length != 0) return Fail(14);
        if (iov[i].length > static_cast<std::size_t>(INT_MAX) - total) return Fail(22);
        total += iov[i].length;
    }
    if (total == 0) return Write(descriptor, nullptr, 0);
    std::vector<char> buffer;
    try {
        buffer.resize(total);
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
    std::size_t copied = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].length != 0 && !GuestCopyIn_nid_no_patch(buffer.data() + copied, iov[i].base, iov[i].length)) return Fail(14);
        copied += iov[i].length;
    }
    return Write(descriptor, buffer.data(), total);
}

extern "C" {
int APS5_VABI _setsockopt_nid_postfix(int descriptor, int level, int option, const void* value, std::uint32_t length) {
    return setsockopt_nid_postfix(descriptor, level, option, value, length);
}
int APS5_VABI _getsockopt_nid_postfix(int descriptor, int level, int option, void* value, std::uint32_t* length) {
    return getsockopt_nid_postfix(descriptor, level, option, value, length);
}
int APS5_VABI _connect_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return connect_nid_postfix(descriptor, address, length);
}
int APS5_VABI _listen_nid_postfix(int descriptor, int backlog) {
    return listen_nid_postfix(descriptor, backlog);
}
int APS5_VABI _accept_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return accept_nid_postfix(descriptor, address, length);
}
int APS5_VABI _bind_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    return bind_nid_postfix(descriptor, address, length);
}
int APS5_VABI _getsockname_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return getsockname_nid_postfix(descriptor, address, length);
}
int APS5_VABI _getpeername_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    return getpeername_nid_postfix(descriptor, address, length);
}
std::int64_t APS5_VABI _sendto_nid_postfix(int descriptor, const void* buffer, std::uint64_t length,
    int flags, const void* address, std::uint32_t addressLength) {
    return sendto_nid_postfix(descriptor, buffer, length, flags, address, addressLength);
}
std::int64_t APS5_VABI _recvfrom_nid_postfix(int descriptor, void* buffer, std::uint64_t length,
    int flags, void* address, std::uint32_t* addressLength) {
    return recvfrom_nid_postfix(descriptor, buffer, length, flags, address, addressLength);
}
std::int64_t APS5_VABI _sendmsg_nid_postfix(int descriptor, const GuestMsghdr* message, int flags) {
    return sendmsg_nid_postfix(descriptor, message, flags);
}
std::int64_t APS5_VABI _recvmsg_nid_postfix(int descriptor, GuestMsghdr* message, int flags) {
    return recvmsg_nid_postfix(descriptor, message, flags);
}
}

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
#include "prx/libkernel/File/include/DescriptorControl.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libkernel/Time/include/TimedWait.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/Socket/include/SocketConnect.hpp"
#include <algorithm>
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
    std::uint32_t inode = 0;
    std::weak_ptr<Socket> peer;
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
int Disconnect(const Socket& socket) {
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
#else
    sockaddr unspecified{};
    unspecified.sa_family = AF_UNSPEC;
    if (::connect(socket.value, &unspecified, sizeof(unspecified)) != 0) return Fail(NativeError());
#endif
    sockaddr_storage current{};
    socklen_t currentSize = sizeof(current);
    if (::getsockname(socket.value, reinterpret_cast<sockaddr*>(&current), &currentSize) != 0) return Fail(NativeError());
    if (PortOf(current) != 0) return 0;
    sockaddr_storage wildcard{};
    wildcard.ss_family = local.ss_family;
    PortOf(wildcard) = PortOf(local);
    if (::bind(socket.value, reinterpret_cast<sockaddr*>(&wildcard), localSize) != 0) return Fail(NativeError());
    return 0;
}
bool ZeroPortConnect(const Socket& socket, const void* address, std::uint32_t length) {
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
    char drained[16];
    while (recv(waker, drained, sizeof(drained), 0) > 0) {}
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
constexpr short GuestPollAccepted = 0x1 | 0x2 | 0x4 | 0x8 | 0x10 | 0x20 | 0x40 | 0x80 | 0x100;
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
    return socket->sendShut ? revents : static_cast<short>(revents & ~POLLHUP);
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
        query(handle, &status, &pipe, sizeof(pipe), FilePipeLocalInformationClass) < 0)
        throw std::runtime_error(std::string(function) + ": querying pipe descriptor " + std::to_string(descriptor) + " failed");
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
    if (replaced) EqueueDescriptorClosed(target);
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
int ReceiveAnsweredHere(Socket& socket, std::uint64_t length) {
    if (ReceiveShut(socket)) return 0;
    if (length != 0) return -1;
    if (socket.type != 1) return ReadableNow(socket) ? -1 : 0;
    sockaddr_storage peer{};
    socklen_t size = sizeof(peer);
    return ::getpeername(socket.value, reinterpret_cast<sockaddr*>(&peer), &size) == 0 ? 0 : NativeError();
}
int SocketSendFlags(Socket& socket, int flags) {
    std::lock_guard lock(socket.modeMutex);
    return NativeSendFlags(socket.options.noSignal ? flags | GuestNoSignal : flags);
}
int WaitForTransfer(Socket& socket, bool send, int flags) {
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
    if (timeout == 0) return 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout >> 32) +
        std::chrono::nanoseconds((static_cast<std::int64_t>(static_cast<std::uint32_t>(timeout)) * 1000000000) >> 32);
    for (;;) {
        const auto left = deadline - std::chrono::steady_clock::now();
        if (left <= std::chrono::steady_clock::duration::zero()) return 35;
        const int milliseconds = static_cast<int>(std::min<std::int64_t>(std::chrono::ceil<std::chrono::milliseconds>(left).count(), INT_MAX));
        NativePollDescriptor entry{};
        entry.fd = socket.value;
#ifdef _WIN32
        entry.events = static_cast<short>(send ? POLLWRNORM : POLLRDNORM);
        const int ready = WSAPoll(&entry, 1, milliseconds);
        if (ready != 0) return 0;
#else
        entry.events = static_cast<short>(send ? POLLOUT : POLLIN);
        const int ready = ::poll(&entry, 1, milliseconds);
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
int QueuedBytes(const Socket& socket, int& count) {
#ifdef __APPLE__
    if (socket.type == 2) {
        socklen_t size = sizeof(count);
        return ::getsockopt(socket.value, SOL_SOCKET, SO_NREAD, &count, &size);
    }
#endif
    return ::ioctl(socket.value, FIONREAD, &count);
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
    for (int index = 0; index < count; ++index) {
        auto socket = Find(entries[index].descriptor);
        if (!socket) return -9;
        NativePollDescriptor request{};
        request.fd = socket->value;
        if (entries[index].events & KernelSocketPoll::Readable) request.events = static_cast<short>(request.events | NativeReadable);
        if (entries[index].events & KernelSocketPoll::Urgent) request.events = static_cast<short>(request.events | NativeUrgent);
        if (entries[index].events & KernelSocketPoll::Writable) request.events = static_cast<short>(request.events | NativeWritable);
        native.push_back(request);
        held.push_back(std::move(socket));
    }
    if (native.empty()) return 0;
#ifdef _WIN32
    const int result = WSAPoll(native.data(), static_cast<ULONG>(native.size()), timeoutMilliseconds);
#else
    const int result = ::poll(native.data(), static_cast<nfds_t>(native.size()), timeoutMilliseconds);
#endif
    if (result < 0) return -NativeError();
    int ready = 0;
    for (int index = 0; index < count; ++index) {
        const short events = native[static_cast<std::size_t>(index)].revents;
        short revents = 0;
        if (events & NativeReadable) revents = static_cast<short>(revents | KernelSocketPoll::Readable);
        if (events & NativeUrgent) revents = static_cast<short>(revents | KernelSocketPoll::Urgent);
        if (events & NativeWritable) revents = static_cast<short>(revents | KernelSocketPoll::Writable);
        if (events & (POLLERR | POLLNVAL)) revents = static_cast<short>(revents | KernelSocketPoll::Error);
        if (events & POLLHUP) revents = static_cast<short>(revents | KernelSocketPoll::HangUp);
        entries[index].revents = revents;
        if (revents != 0) ++ready;
    }
    return ready;
}

extern "C" bool KernelLookupSocketNative_nid_no_patch(int descriptor, std::shared_ptr<void>* owner, std::uintptr_t* native) {
    if (auto socket = Find(descriptor)) {
        *native = static_cast<std::uintptr_t>(socket->value);
        *owner = std::static_pointer_cast<KernelSocketPoll::NativeOwner>(std::move(socket));
        return true;
    }
    return KernelFindSocketNative_nid_no_patch(descriptor, owner, native);
}

extern "C" {
int APS5_VABI fcntl_nid_postfix(int descriptor, int command, ...) {
    const auto type = File::FcntlArgumentOf(command);
    std::intptr_t argument = 0;
    if (type != File::FcntlArgument::None) {
#ifdef _WIN32
        __builtin_sysv_va_list arguments;
        __builtin_sysv_va_start(arguments, command);
        argument = type == File::FcntlArgument::Int ? __builtin_va_arg(arguments, int)
                                                    : reinterpret_cast<std::intptr_t>(__builtin_va_arg(arguments, void*));
        __builtin_sysv_va_end(arguments);
#else
        std::va_list arguments;
        va_start(arguments, command);
        argument = type == File::FcntlArgument::Int ? va_arg(arguments, int)
                                                    : reinterpret_cast<std::intptr_t>(va_arg(arguments, void*));
        va_end(arguments);
#endif
    }
    if (!GuestSockets::IsOpen(descriptor)) {
        int result = 0;
        int netError = 0;
        if (KernelControlSocketStatus_nid_no_patch(descriptor, command, static_cast<int>(argument), &result, &netError)) {
            return netError == 0 ? result : Fail(netError);
        }
        return File::Fcntl(descriptor, command, argument);
    }
    if (command == 0) return DuplicateSocket_nid_no_patch(descriptor, static_cast<int>(argument));
    if (command == 17) return Fail(45);
    if (command == 10) return dup2_nid_postfix(descriptor, static_cast<int>(argument));
    if (command == 18) return DuplicateSocket_nid_no_patch(descriptor, 0);
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    std::lock_guard lock(socket->modeMutex);
    if (command == 1) {
        std::lock_guard registry(socketsMutex);
        return closeOnExecDescriptors.count(descriptor) != 0 ? 1 : 0;
    }
    if (command == 2) {
        const int flags = static_cast<int>(argument);
        std::lock_guard registry(socketsMutex);
        if ((flags & 1) != 0) closeOnExecDescriptors.insert(descriptor);
        else closeOnExecDescriptors.erase(descriptor);
        return 0;
    }
    if (command == 3) return 2 | (socket->options.nonblocking ? 4 : 0); // F_GETFL, O_RDWR
    if (command != 4) return Fail(22);
    const int flags = static_cast<int>(argument);
    if ((flags & ~7) != 0) return Fail(45);
#ifdef _WIN32
    unsigned long enabled = (flags & 4) != 0;
    if (ioctlsocket(socket->value, FIONBIO, &enabled)) return Fail(NativeError());
#else
    int enabled = (flags & 4) != 0;
    if (::ioctl(socket->value, FIONBIO, &enabled)) return Fail(NativeError());
#endif
    socket->options.nonblocking = enabled != 0;
    return 0;
}
int APS5_VABI _fcntl_nid_postfix(int descriptor, int command, ...) {
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, command);
    const auto argument = __builtin_va_arg(arguments, std::uint64_t);
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, command);
    const auto argument = va_arg(arguments, std::uint64_t);
    va_end(arguments);
#endif
    return fcntl_nid_postfix(descriptor, command, argument);
}
int APS5_VABI setsockopt_nid_postfix(int descriptor, int level, int option,
                                    const void* value, std::uint32_t length) {
    if (!value && length != 0) return Fail(14);
    int netError = 0;
    if (!GuestSockets::IsOpen(descriptor) && KernelSetSocketOption_nid_no_patch(descriptor, level, option, value, length, &netError)) {
        return netError == 0 ? 0 : Fail(netError);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (level != 0xffff && level != 0 && level != 6 && level != 41) return Fail(22);
    std::lock_guard lock(socket->modeMutex);
    const auto native = static_cast<std::uintptr_t>(socket->value);
    const int error = level == 0    ? GuestSockets::SetIpLevelOption(native, socket->options, socket->family == 2, socket->type == 2, option, value, length)
                      : level == 6  ? GuestSockets::SetTcpLevelOption(native, socket->options, socket->family != GuestUnix && socket->type == 1, option, value, length)
                      : level == 41 ? GuestSockets::SetIpv6LevelOption(native, socket->family == 28, option, value, length)
                                    : GuestSockets::SetSocketLevelOption(native, socket->options, socket->family, socket->type, option, value, length);
    return error == 0 ? 0 : Fail(error);
}
int APS5_VABI getsockopt_nid_postfix(int descriptor, int level, int option,
                                    void* value, std::uint32_t* length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (level != 0xffff && level != 0 && level != 6 && level != 41) return Fail(22);
    if (!length) return Fail(22);
    if (!value) {
        const GuestArena::HostWrite lengthOutput(length, sizeof(*length));
        if (!lengthOutput.Open()) return Fail(14);
        *length = 0;
    }
    std::lock_guard lock(socket->modeMutex);
    const auto native = static_cast<std::uintptr_t>(socket->value);
    const int error = level == 0    ? GuestSockets::GetIpLevelOption(socket->options, socket->family == 2, socket->type == 2, option, value, length)
                      : level == 6  ? GuestSockets::GetTcpLevelOption(native, socket->options, socket->family != GuestUnix && socket->type == 1, option, value, length)
                      : level == 41 ? GuestSockets::GetIpv6LevelOption(native, socket->family == 28, option, value, length)
                                    : GuestSockets::GetSocketLevelOption(native, socket->options, socket->family, socket->type, option, value, length);
    return error == 0 ? 0 : Fail(error);
}
int APS5_VABI socket_nid_postfix(int family, int type, int protocol) {
    const bool nonblocking = (type & GuestNonBlocking) != 0;
    const int plainType = type & ~(GuestCloseOnExec | GuestNonBlocking);
    if (family != 2 && family != 28) return Fail(47);
    const int nativeType = plainType == 1 ? SOCK_STREAM : plainType == 2 ? SOCK_DGRAM : -1;
    const int expectedProtocol = plainType == 1 ? 6 : 17;
    const bool inetKnowsType = plainType == 3;
    if (nativeType == -1) return Fail(protocol == 0 && plainType != 0 && !inetKnowsType ? 41 : 43);
    if (protocol != 0 && protocol != expectedProtocol) return Fail(43);
#ifdef _WIN32
    static const int startup = [] { WSADATA data{}; return WSAStartup(MAKEWORD(2, 2), &data); }();
    if (startup) return Fail(5);
#endif
    const auto native = ::socket(family == 2 ? AF_INET : AF_INET6, nativeType, protocol);
    if (native == Invalid) return Fail(NativeError());
    Socket guard(native, family, plainType);
    const int ipv6Only = 1;
    if (family == 28 && ::setsockopt(native, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&ipv6Only), sizeof(ipv6Only)))
        return Fail(NativeError());
#ifdef _WIN32
    BOOL reportUnreachable = FALSE;
    DWORD returned = 0;
    if (nativeType == SOCK_DGRAM && WSAIoctl(native, SIO_UDP_CONNRESET, &reportUnreachable, sizeof(reportUnreachable), nullptr, 0,
            &returned, nullptr, nullptr) != 0)
        return Fail(NativeError());
#endif
    try {
        auto socket = std::make_shared<Socket>(native, family, plainType);
        guard.value = Invalid;
        if (family == 28) socket->options.tcpMaximumSegment = 1220;
        if (nonblocking && SetNonBlocking(*socket)) return -1;
        const int number = Install(std::move(socket));
        if (number >= 0 && (type & GuestCloseOnExec) != 0) {
            std::lock_guard registry(socketsMutex);
            closeOnExecDescriptors.insert(number);
        }
        return number;
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
}
int APS5_VABI __sys_socketex_nid_postfix(const char* name, int family, int type, int protocol) {
    static_cast<void>(name);
    return socket_nid_postfix(family, type, protocol);
}
int APS5_VABI socketpair_nid_postfix(int family, int type, int protocol, int* descriptors) {
    const bool nonblocking = (type & GuestNonBlocking) != 0;
    const int plainType = type & ~(GuestCloseOnExec | GuestNonBlocking);
    if (family != GuestUnix && family != 2 && family != 28) return Fail(47);
    if (plainType == 0) return Fail(43);
    if (family != GuestUnix) {
        if (plainType != 1 && plainType != 2) return Fail(41);
        return Fail(protocol == 0 || protocol == (plainType == 1 ? 6 : 17) ? 45 : 43);
    }
    if (protocol != 0) return Fail(43);
    if (plainType != 1 && plainType != 2 && plainType != 5) return Fail(41);
    if (!descriptors) return Fail(14);
    const GuestArena::HostWrite destination(descriptors, 2 * sizeof(int));
    if (!destination.Open()) return Fail(14);
    NativeSocket native[2];
    if (const int error = NativePair(plainType, native)) return Fail(error);
    Socket firstGuard(native[0], GuestUnix, plainType);
    Socket secondGuard(native[1], GuestUnix, plainType);
    try {
        auto first = std::make_shared<Socket>(native[0], GuestUnix, plainType);
        firstGuard.value = Invalid;
        auto second = std::make_shared<Socket>(native[1], GuestUnix, plainType);
        secondGuard.value = Invalid;
        first->peer = second;
        second->peer = first;
        if (nonblocking && (SetNonBlocking(*first) || SetNonBlocking(*second))) return -1;
        const int firstDescriptor = Install(std::move(first));
        if (firstDescriptor < 0) return -1;
        const int secondDescriptor = Install(std::move(second));
        if (secondDescriptor < 0) {
            const int error = *__error_nid_postfix();
            GuestSockets::Close(firstDescriptor);
            return Fail(error);
        }
        if ((type & GuestCloseOnExec) != 0) {
            std::lock_guard registry(socketsMutex);
            closeOnExecDescriptors.insert(firstDescriptor);
            closeOnExecDescriptors.insert(secondDescriptor);
        }
        descriptors[0] = firstDescriptor;
        descriptors[1] = secondDescriptor;
        return 0;
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
}
int APS5_VABI connect_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    if (const int error = AddressLengthError(address, length)) return Fail(error);
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (socket->family != GuestUnix && socket->type == 2 && address && length >= 2 && static_cast<const unsigned char*>(address)[1] == 0
        && Disconnect(*socket) != 0)
        return -1;
    if (socket->type == 1 && Listening(*socket)) return Fail(45);
    const int mapped = LongMappedDualStack(*socket, address, length);
    if (mapped < 0) return -1;
    sockaddr_storage native{};
    socklen_t size;
    if (!Address(socket->family, address, mapped ? 28 : length, native, size)) return -1;
#ifdef _WIN32
    if (socket->type == 1) RefuseClosedLoopbackAtOnce_nid_no_patch(socket->value, reinterpret_cast<const sockaddr*>(&native));
#endif
    if (ZeroPortConnect(*socket, address, length)) return -1;
    {
        std::lock_guard lock(socket->modeMutex);
        socket->options.reportedError = 0;
    }
    return ::connect(socket->value, reinterpret_cast<sockaddr*>(&native), size) ? Fail(ConnectError()) : 0;
}
int APS5_VABI listen_nid_postfix(int descriptor, int backlog) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (socket->type != 1) return Fail(45);
    if (::listen(socket->value, backlog) == 0) return 0;
    const int error = NativeError();
    return Fail(error == 56 ? 22 : error);
}
int APS5_VABI accept_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    const auto listener = Lookup(descriptor);
    if (!listener) return -1;
    if (listener->type != 1 || (address && !length)) return Fail(22);
    sockaddr_storage peer{};
    socklen_t size = sizeof(peer);
    const auto native = ::accept(listener->value, address ? reinterpret_cast<sockaddr*>(&peer) : nullptr,
        address ? &size : nullptr);
    if (native == Invalid) return Fail(NativeError());
    Socket guard(native, listener->family, listener->type);
    try {
        auto accepted = std::make_shared<Socket>(native, listener->family, listener->type);
        guard.value = Invalid;
        if (listener->family == 28) accepted->options.tcpMaximumSegment = 1220;
        bool inheritNonblocking;
        {
            std::lock_guard modeLock(listener->modeMutex);
            inheritNonblocking = listener->options.nonblocking;
            accepted->options.noSignal = listener->options.noSignal;
            accepted->options.keptFlags = listener->options.keptFlags;
            accepted->options.sendTimeout = listener->options.sendTimeout;
            accepted->options.receiveTimeout = listener->options.receiveTimeout;
            accepted->options.sendBuffer = listener->options.sendBuffer;
            accepted->options.receiveBuffer = listener->options.receiveBuffer;
            accepted->options.sendLowWater = listener->options.sendLowWater;
            accepted->options.receiveLowWater = listener->options.receiveLowWater;
            accepted->options.ipTimeToLive = listener->options.ipTimeToLive;
            accepted->options.ipTypeOfService = listener->options.ipTypeOfService;
            accepted->options.tcpKeepIdle = listener->options.tcpKeepIdle;
            accepted->options.tcpKeepInterval = listener->options.tcpKeepInterval;
            accepted->options.tcpKeepCount = listener->options.tcpKeepCount;
            accepted->options.tcpFlags = listener->options.tcpFlags & 0x1000;
        }
        if (inheritNonblocking && SetNonBlocking(*accepted)) return -1;
        const int acceptedDescriptor = Install(std::move(accepted));
        if (acceptedDescriptor < 0) return -1;
        if (address) GuestAddress(peer, address, length);
        return acceptedDescriptor;
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
}
std::int64_t APS5_VABI send_nid_postfix(int descriptor, const void* buffer, std::uint64_t length, int flags) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestSendFlags) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    if (const int waited = WaitForTransfer(*socket, true, flags)) return Fail(waited);
    const auto result = ::send(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), SocketSendFlags(*socket, flags));
    return result < 0 ? Fail(SendError(socket->type)) : result;
}
std::int64_t APS5_VABI recv_nid_postfix(int descriptor, void* buffer, std::uint64_t length, int flags) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestReceiveFlags) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    const GuestArena::HostWrite destination(length == 0 ? nullptr : buffer, static_cast<std::size_t>(length));
    if (!destination.Open()) return Fail(14);
    if (const int answered = ReceiveAnsweredHere(*socket, length); answered >= 0) return answered == 0 ? 0 : Fail(answered);
    if (const int waited = WaitForTransfer(*socket, false, flags)) return Fail(waited);
    const auto result = ::recv(socket->value, static_cast<char*>(buffer), static_cast<int>(length),
        NativeReceiveFlags(flags, socket->type));
#ifdef _WIN32
    if (result < 0 && socket->type == 2 && WSAGetLastError() == WSAEMSGSIZE) return static_cast<std::int64_t>(length);
#endif
    return result < 0 ? Fail(NativeError()) : result;
}
int APS5_VABI bind_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    if (const int error = AddressLengthError(address, length)) return Fail(error);
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    sockaddr_storage native{};
    socklen_t size;
    const int mapped = LongMappedDualStack(*socket, address, length);
    if (mapped < 0) return -1;
    if (!Address(socket->family, address, mapped ? 28 : length, native, size)) return -1;
    return ::bind(socket->value, reinterpret_cast<sockaddr*>(&native), size) ? Fail(NativeError()) : 0;
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
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (how < 0 || how > 2) return Fail(22);
    sockaddr_storage peer{};
    socklen_t size = sizeof(peer);
    const bool unconnected = socket->type == 2 && ::getpeername(socket->value, reinterpret_cast<sockaddr*>(&peer), &size) != 0;
    if (::shutdown(socket->value, how) && !unconnected && !AlreadyShut(socket->value, how)) return Fail(NativeError());
    {
        std::lock_guard lock(socket->modeMutex);
        if (how != 1) socket->receiveShut = true;
        if (how != 0) socket->sendShut = true;
    }
    if (how != 1)
        for (const int alias : DescriptorsOf(socket)) EqueueDescriptorReadShut(alias);
    return unconnected ? Fail(57) : 0;
}
int APS5_VABI getsockname_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (!address || !length) return Fail(14);
    if (socket->family == GuestUnix) return UnnamedAddress(address, length);
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    if (::getsockname(socket->value, reinterpret_cast<sockaddr*>(&native), &size)) {
#ifdef _WIN32
        if (WSAGetLastError() != WSAEINVAL) return Fail(NativeError());
        native = {};
        native.ss_family = socket->family == 2 ? AF_INET : AF_INET6;
#else
        return Fail(NativeError());
#endif
    }
    GuestAddress(native, address, length);
    return 0;
}
int APS5_VABI getpeername_nid_postfix(int descriptor, void* address, std::uint32_t* length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (socket->family == GuestUnix && socket->peer.expired()) return Fail(57);
    if (!address || !length) return Fail(14);
    if (socket->family == GuestUnix) return UnnamedAddress(address, length);
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    if (::getpeername(socket->value, reinterpret_cast<sockaddr*>(&native), &size)) return Fail(NativeError());
    GuestAddress(native, address, length);
    return 0;
}
int APS5_VABI ioctl_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    if (!GuestSockets::IsOpen(descriptor)) return File::Ioctl(descriptor, request, argument);
    if (File::IoctlRequestMalformed(request)) return Fail(25);
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (!argument) return Fail(14);
    if (request != 0x8004667e && request != 0x4004667f) return Fail(25);
    std::lock_guard lock(socket->modeMutex);
    if (request == 0x4004667f && socket->receiveShut) {
        *static_cast<int*>(argument) = 0;
        return 0;
    }
    unsigned long value = 0;
    if (request == 0x8004667e) value = *static_cast<int*>(argument) != 0;
#ifdef _WIN32
    const auto result = ioctlsocket(socket->value, request == 0x8004667e ? FIONBIO : FIONREAD, &value);
#else
    int nativeValue = static_cast<int>(value);
    const auto result = request == 0x8004667e ? ::ioctl(socket->value, FIONBIO, &nativeValue) : QueuedBytes(*socket, nativeValue);
    value = static_cast<unsigned long>(nativeValue);
#endif
    if (result) return Fail(NativeError());
    if (request == 0x8004667e) socket->options.nonblocking = value != 0;
    if (request == 0x4004667f) *static_cast<int*>(argument) = static_cast<int>(value);
    return 0;
}
int APS5_VABI _ioctl_nid_postfix(int descriptor, std::uint64_t request, void* argument) {
    return ioctl_nid_postfix(descriptor, request, argument);
}
std::int64_t APS5_VABI sendto_nid_postfix(int descriptor, const void* buffer, std::uint64_t length,
    int flags, const void* address, std::uint32_t addressLength) {
    if (address) {
        if (const int error = AddressLengthError(address, addressLength)) return Fail(error);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestSendFlags) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    if (const int waited = WaitForTransfer(*socket, true, flags)) return Fail(waited);
    int result;
    if (!address || (socket->type == 1 && PeerConnected(*socket))) {
        if (!address && addressLength != 0) return Fail(22);
        result = ::send(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), SocketSendFlags(*socket, flags));
    } else {
        sockaddr_storage native{};
        socklen_t size;
        if (!Address(socket->family, address, addressLength, native, size)) return -1;
        if (const auto answered = ZeroPortSend(*socket, native, length); answered != NotAnswered) return answered;
        result = static_cast<int>(::sendto(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), SocketSendFlags(*socket, flags),
            reinterpret_cast<sockaddr*>(&native), size));
    }
    return result < 0 ? Fail(SendError(socket->type)) : result;
}
std::int64_t APS5_VABI recvfrom_nid_postfix(int descriptor, void* buffer, std::uint64_t length,
    int flags, void* address, std::uint32_t* addressLength) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestReceiveFlags) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if ((!buffer && length) || (address && !addressLength)) return Fail(14);
    const GuestArena::HostWrite destination(length == 0 ? nullptr : buffer, static_cast<std::size_t>(length));
    if (!destination.Open()) return Fail(14);
    if (const int answered = ReceiveAnsweredHere(*socket, length); answered >= 0) {
        if (answered > 0) return Fail(answered);
        if (address) *addressLength = 0;
        return 0;
    }
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    if (const int waited = WaitForTransfer(*socket, false, flags)) return Fail(waited);
    auto result = ::recvfrom(socket->value, static_cast<char*>(buffer), static_cast<int>(length),
        NativeReceiveFlags(flags, socket->type), reinterpret_cast<sockaddr*>(&native), &size);
#ifdef _WIN32
    if (result < 0 && socket->type == 2 && WSAGetLastError() == WSAEMSGSIZE) result = static_cast<int>(length);
#endif
    if (result < 0) return Fail(NativeError());
    if (address && socket->family == GuestUnix) {
        if (socket->type == 1) *addressLength = 0; else UnnamedAddress(address, addressLength);
        return result;
    }
    if (address) {
        if (native.ss_family == AF_INET || native.ss_family == AF_INET6) GuestAddress(native, address, addressLength);
        else *addressLength = 0;
    }
    return result;
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
std::int64_t APS5_VABI sendmsg_nid_postfix(int descriptor, const GuestMsghdr* message, int flags) {
    const auto total = MessageLength(message);
    if (total < 0) return -1;
    if (message->control && message->controlLength) throw std::runtime_error("sendmsg: control data is not supported");
    std::vector<char> buffer;
    try {
        buffer.resize(static_cast<std::size_t>(total));
    } catch (const std::bad_alloc&) {
        return Fail(55);
    }
    std::size_t offset = 0;
    for (int i = 0; i < message->iovLength; ++i) {
        if (message->iov[i].length) std::memcpy(buffer.data() + offset, message->iov[i].base, message->iov[i].length);
        offset += message->iov[i].length;
    }
    return sendto_nid_postfix(descriptor, buffer.data(), buffer.size(), flags, message->name, message->name ? message->nameLength : 0u);
}
#ifdef _WIN32
static bool GuestPagesAllow(std::uintptr_t start, std::size_t bytes, DWORD allowed) {
    const auto end = start + bytes;
    for (auto address = start; address < end;) {
        MEMORY_BASIC_INFORMATION memory{};
        if (VirtualQuery(reinterpret_cast<LPCVOID>(address), &memory, sizeof(memory)) != sizeof(memory)) return false;
        if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 || (memory.Protect & allowed) == 0) return false;
        address = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    }
    return true;
}
#elif defined(__linux__)
static bool Transferred(ssize_t copied, std::size_t bytes, const char* call) {
    if (copied >= 0) return static_cast<std::size_t>(copied) == bytes;
    if (errno == EFAULT) return false;
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
    return Transferred(::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0), bytes, "process_vm_readv");
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
    return Transferred(::process_vm_writev(::getpid(), &local, 1, &remote, 1, 0), bytes, "process_vm_writev");
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
    if ((flags & ~GuestReceiveFlags) != 0) return Fail(45);
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
    const bool shut = ReceiveShut(*socket);
    std::vector<unsigned char> stamps;
    if (!shut && (total != 0 || socket->type == 1 || ReadableNow(*socket))) {
        size = sizeof(native);
        if (const int waited = WaitForTransfer(*socket, false, flags)) return Fail(waited);
        received = ::recvfrom(socket->value, buffer.data(), static_cast<int>(buffer.size()),
            NativeReceiveFlags(flags, socket->type), reinterpret_cast<sockaddr*>(&native), &size);
        if (received < 0) {
#ifdef _WIN32
            if (!records || WSAGetLastError() != WSAEMSGSIZE) return Fail(NativeError());
            received = static_cast<std::int64_t>(buffer.size());
#else
            return Fail(NativeError());
#endif
        }
        if (header.control) {
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
    const std::size_t controlCapacity = header.controlLength;
    const std::size_t copied = std::min(stamps.size(), controlCapacity);
    if (copied != 0 && !WriteGuest(header.control, stamps.data(), copied)) return Fail(14);
    if (copied < stamps.size()) header.flags |= 0x20;
    header.controlLength = static_cast<std::uint32_t>(copied);
    constexpr auto results = offsetof(GuestMsghdr, controlLength);
    if (!WriteGuest(reinterpret_cast<char*>(message) + results, reinterpret_cast<const char*>(&header) + results, sizeof(header) - results)) return Fail(14);
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
    if (headers && (!SendfileIovecs(headers->headers, headers->headerCount) || !SendfileIovecs(headers->trailers, headers->trailerCount)))
        return Fail(22);
    std::int64_t sent = 0;
    const auto finish = [&](int error) {
        if (sbytes) *sbytes = sent;
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
    if (headers)
        if (const int error = SendIovecs(*socket, headers->headers, headers->headerCount, sent)) return finish(error);
    while (received > 0) {
        if (const int error = SendAll(*socket, chunk.data(), static_cast<std::uint64_t>(received), sent)) return finish(error);
        offset += received;
        remaining -= static_cast<std::uint64_t>(received);
        if (remaining == 0) break;
        received = read(offset, remaining);
        if (received < 0) return finish(static_cast<int>(-received));
    }
    if (headers)
        if (const int error = SendIovecs(*socket, headers->trailers, headers->trailerCount, sent)) return finish(error);
    return finish(0);
}
int APS5_VABI poll_nid_postfix(GuestPollDescriptor* descriptors, std::uint32_t count, int timeout) {
    if (timeout < -1) return Fail(22);
    if (count && !descriptors) return Fail(14);
    std::vector<std::shared_ptr<void>> held;
    std::vector<NativePollDescriptor> native;
    std::vector<std::uint32_t> owners;
    std::vector<std::uint32_t> pipes;
    std::vector<std::uint32_t> kqueues;
    int ready = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        auto& entry = descriptors[i];
        entry.revents = 0;
        if (entry.descriptor < 0) continue;
        if ((entry.events & ~GuestPollAccepted) != 0)
            throw std::runtime_error("poll: unsupported event flags " + std::to_string(entry.events));
        std::shared_ptr<void> owner;
        std::uintptr_t value = 0;
        if (auto socket = Find(entry.descriptor)) {
            value = static_cast<std::uintptr_t>(socket->value);
            owner = std::move(socket);
        } else if (!KernelFindSocketNative_nid_no_patch(entry.descriptor, &owner, &value)) {
            if (!DescriptorIsOpen_nid_no_patch(entry.descriptor)) {
                entry.revents = GuestPollInvalid;
                ++ready;
                continue;
            }
            if (IsKqueueDescriptor(entry.descriptor)) {
                kqueues.push_back(i);
                continue;
            }
            if (File::IsRandomDevice(entry.descriptor)) {
                const short readable = static_cast<short>(entry.events & GuestPollReadable);
                entry.revents = readable != 0 ? readable : entry.events;
                if (entry.revents) ++ready;
                continue;
            }
            if (File::IsVnode(entry.descriptor)) {
                entry.revents = static_cast<short>(entry.events & GuestPollVnodeReady);
                if (entry.revents) ++ready;
                continue;
            }
            if (entry.descriptor <= 2) {
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
                throw std::runtime_error("poll: descriptor " + std::to_string(entry.descriptor) + " is neither a socket, a pipe, a file, a directory nor a kqueue");
            pipes.push_back(i);
            continue;
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
    const auto collect = [&](std::size_t i, bool pipe) {
        auto& entry = descriptors[owners[i]];
#ifdef _WIN32
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
        if (pipe && (entry.revents & GuestPollError)) entry.revents = static_cast<short>((entry.revents & ~GuestPollError) | GuestPollHangUp);
        if (pipe && (entry.revents & GuestPollHangUp))
            entry.revents = static_cast<short>((entry.revents & ~GuestPollWritable) | (entry.events & GuestPollReadable));
#ifndef _WIN32
        if (entry.revents & GuestPollHangUp) entry.revents = static_cast<short>(entry.revents & ~GuestPollWritable);
#endif
        if (entry.revents) ++ready;
    };
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout < 0 ? 0 : timeout);
#ifdef __APPLE__
    bool waitsForUrgent = false;
    for (std::size_t i = 0; i < socketCount && !waitsForUrgent; ++i) {
        waitsForUrgent = (descriptors[owners[i]].events & GuestPollPriority) != 0;
    }
#else
    constexpr bool waitsForUrgent = false;
#endif
#ifdef _WIN32
    const bool sliced = !pipes.empty() || !kqueues.empty();
#else
    const bool sliced = !kqueues.empty() || waitsForUrgent;
#endif
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
        if (!ready && !checkedReady && !urgent) {
            if (!sliced) slice = timeout;
            else if (timeout < 0) slice = 1;
            else {
                const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
                slice = static_cast<int>(std::clamp<long long>(left, 0, 1));
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
        int socketReady = 0;
        for (const auto& request : native)
            if (request.revents) ++socketReady;
        const bool expired = timeout >= 0 && std::chrono::steady_clock::now() >= deadline;
        if (ready || checkedReady || urgent || socketReady || expired || !sliced) {
            ready += checkedReady;
#ifdef _WIN32
            for (std::size_t i = 0; i < native.size(); ++i) collect(i, false);
#else
            for (std::size_t i = 0; i < native.size(); ++i) collect(i, i >= socketCount);
#endif
            return ready;
        }
    }
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
        if (amount != 0) std::memcpy(iov[i].base, buffer.data() + copied, amount);
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
        if (iov[i].length != 0) std::memcpy(buffer.data() + copied, iov[i].base, iov[i].length);
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

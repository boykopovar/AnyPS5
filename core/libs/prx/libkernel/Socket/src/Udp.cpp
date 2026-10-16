#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <fcntl.h>
#include <io.h>
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>
#endif
#include "SceTypes.hpp"
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
#include <thread>
#include <vector>

extern "C" int* APS5_VABI __error_nid_postfix();
namespace {
#ifdef _WIN32
using NativeSocket = SOCKET;
constexpr auto Invalid = INVALID_SOCKET;
#else
using NativeSocket = int;
constexpr auto Invalid = -1;
#endif
int Fail(int error) { *__error_nid_postfix() = error; return -1; }
constexpr int GuestPeek = 0x2;
constexpr int GuestDontWait = 0x80;
constexpr int GuestNoSignal = 0x20000;
#ifdef _WIN32
constexpr int GuestSendFlags = GuestNoSignal;
constexpr int GuestReceiveFlags = GuestPeek;
#else
constexpr int GuestSendFlags = GuestNoSignal | GuestDontWait;
constexpr int GuestReceiveFlags = GuestPeek | GuestDontWait;
#endif
int NativeSendFlags(int flags) {
#ifdef _WIN32
    static_cast<void>(flags);
    return 0;
#else
    return (flags & GuestNoSignal ? MSG_NOSIGNAL : 0) | (flags & GuestDontWait ? MSG_DONTWAIT : 0);
#endif
}
int NativeReceiveFlags(int flags) {
#ifdef _WIN32
    return flags & GuestPeek ? MSG_PEEK : 0;
#else
    return (flags & GuestPeek ? MSG_PEEK : 0) | (flags & GuestDontWait ? MSG_DONTWAIT : 0);
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
    if (type == 2 && WSAGetLastError() == WSAENOTCONN) return 39;
#else
    static_cast<void>(type);
#endif
    return NativeError();
}
struct Socket {
    NativeSocket value;
    int family;
    int type;
    std::mutex modeMutex;
    GuestSockets::SocketOptionState options;
    bool receiveShut = false;
    bool sendShut = false;
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
    return number;
}
int CopyNumber(int number) {
    const int copy = ::_dup(number);
    return copy < 0 ? Fail(errno == EMFILE ? 24 : 9) : copy;
}
int CopyNumberTo(int number, int target) {
    return ::_dup2(number, target) != 0 ? Fail(errno == EMFILE ? 24 : 9) : 0;
}
void ReleaseNumber(int number) {
    ::_close(number);
}
#else
int ReserveNumber(NativeSocket native) {
    const int number = ::fcntl(native, F_DUPFD_CLOEXEC, 0);
    return number < 0 ? Fail(NativeError()) : number;
}
int CopyNumber(int number) {
    const int copy = ::fcntl(number, F_DUPFD_CLOEXEC, 0);
    return copy < 0 ? Fail(NativeError()) : copy;
}
int CopyNumberTo(int number, int target) {
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
    Fail(9);
    return {};
}
bool Address(const void* input, std::uint32_t length, sockaddr_storage& native, socklen_t& size) {
    if (!input || length < 2) { Fail(14); return false; }
    const auto* bytes = static_cast<const unsigned char*>(input);
    if (bytes[1] == 2 && length >= 16) {
        auto& v4 = reinterpret_cast<sockaddr_in&>(native);
        v4.sin_family = AF_INET;
        std::memcpy(&v4.sin_port, bytes + 2, 2);
        std::memcpy(&v4.sin_addr, bytes + 4, 4);
        size = sizeof(v4);
        return true;
    }
    if (bytes[1] == 28 && length >= 28) {
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
std::uint16_t& PortOf(sockaddr_storage& address) {
    return address.ss_family == AF_INET ? reinterpret_cast<sockaddr_in&>(address).sin_port : reinterpret_cast<sockaddr_in6&>(address).sin6_port;
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
#else
    std::vector<pollfd> entries;
    constexpr short ReadMask = POLLIN;
    constexpr short WriteMask = POLLOUT;
#endif
    {
        std::lock_guard lock(socketsMutex);
        for (const auto& interest : interests) {
            const auto found = sockets.find(interest.descriptor);
            if (found == sockets.end()) continue;
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
    if (WSAPoll(entries.data(), static_cast<ULONG>(entries.size()), timeout) == SOCKET_ERROR)
        throw std::runtime_error("equeue wait: WSAPoll failed, guest errno " + std::to_string(NativeError()));
#else
    timespec timeout{static_cast<time_t>(remaining / 1000000000), static_cast<long>(remaining % 1000000000)};
    if (::ppoll(entries.data(), entries.size(), deadlineNanos != 0 ? &timeout : nullptr, nullptr) < 0 && errno != EINTR)
        throw std::runtime_error("equeue wait: ppoll failed, guest errno " + std::to_string(NativeError()));
#endif
    char drained[16];
    while (recv(waker, drained, sizeof(drained), 0) > 0) {}
    return true;
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
struct GuestPollDescriptor {
    int descriptor;
    short events;
    short revents;
};
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
std::shared_ptr<Socket> Find(int descriptor) {
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    return found != sockets.end() ? found->second : nullptr;
}
constexpr short GuestPollReadable = 0x1 | 0x40;
constexpr short GuestPollWritable = 0x4;
constexpr short GuestPollHangUp = 0x10;
constexpr short GuestPollError = 0x8;
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
bool PipeReadiness(int descriptor, short events, short& revents) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    if (handle == INVALID_HANDLE_VALUE || GetFileType(handle) != FILE_TYPE_PIPE) return false;
    static const auto query = reinterpret_cast<QueryInformationFile>(
        reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationFile")));
    if (!query) throw std::runtime_error("poll: NtQueryInformationFile is unavailable");
    PipeIoStatus status{};
    ACCESS_MASK access = 0;
    PipeLocalInformation pipe{};
    if (query(handle, &status, &access, sizeof(access), FileAccessInformationClass) < 0 ||
        query(handle, &status, &pipe, sizeof(pipe), FilePipeLocalInformationClass) < 0)
        throw std::runtime_error("poll: querying pipe descriptor " + std::to_string(descriptor) + " failed");
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

int GuestSockets::Duplicate(int descriptor) {
    std::lock_guard lock(socketsMutex);
    const auto found = sockets.find(descriptor);
    if (found == sockets.end()) return Fail(9);
    const int duplicate = CopyNumber(descriptor);
    if (duplicate < 0) return -1;
    try {
        sockets.insert_or_assign(duplicate, found->second);
        closeOnExecDescriptors.erase(duplicate);
    } catch (const std::bad_alloc&) {
        ReleaseNumber(duplicate);
        return Fail(12);
    }
    return duplicate;
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
int SocketSendFlags(Socket& socket, int flags) {
    std::lock_guard lock(socket.modeMutex);
    return NativeSendFlags(socket.options.noSignal ? flags | GuestNoSignal : flags);
}
int WaitForTransfer(Socket& socket, bool send, int flags) {
    if ((flags & GuestDontWait) != 0) return 0;
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
    const int nativeType = type == 1 ? SOCK_STREAM : type == 2 ? SOCK_DGRAM : SOCK_SEQPACKET;
    int native[2];
    if (::socketpair(AF_UNIX, nativeType, 0, native)) return NativeError();
    pair[0] = native[0];
    pair[1] = native[1];
    return 0;
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
    {
        std::lock_guard lock(socketsMutex);
        const auto found = sockets.find(descriptor);
        if (found == sockets.end()) return 9;
        socket = found->second;
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
    if (ioctlsocket(socket->value, FIONREAD, &count) == 0) available = static_cast<std::int64_t>(count);
#else
    int count = 0;
    if (::ioctl(socket->value, FIONREAD, &count) == 0) available = count;
#endif
    int sendBuffer = 0;
    socklen_t length = sizeof(sendBuffer);
    if (getsockopt(socket->value, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&sendBuffer), &length) != 0) sendBuffer = 0;
    *status = FileStat{};
    status->st_mode = 0140000;
    if (!receiveShut || available > 0) status->st_mode |= 0444;
    if (!sendShut) status->st_mode |= 0222;
    status->st_size = available;
    status->st_blksize = sendBuffer > 0 ? static_cast<std::uint32_t>(sendBuffer) : 0;
    return 0;
}

bool GuestSockets::Ready(int descriptor, bool write, std::int64_t* data, bool* eof) {
    std::shared_ptr<Socket> socket;
    {
        std::lock_guard lock(socketsMutex);
        const auto found = sockets.find(descriptor);
        if (found == sockets.end()) return false;
        socket = found->second;
    }
#ifdef _WIN32
    WSAPOLLFD entry{socket->value, static_cast<SHORT>(write ? POLLWRNORM : POLLRDNORM), 0};
    if (WSAPoll(&entry, 1, 0) <= 0) return false;
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
    } else {
#ifdef _WIN32
        unsigned long available = 0;
        if (ioctlsocket(socket->value, FIONREAD, &available) == 0) *data = static_cast<std::int64_t>(available);
#else
        int available = 0;
        if (::ioctl(socket->value, FIONREAD, &available) == 0) *data = available;
#endif
    }
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

extern "C" {
int APS5_VABI fcntl_nid_postfix(int descriptor, int command, ...) {
    if (!GuestSockets::IsOpen(descriptor)) {
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
        int result = 0;
        int netError = 0;
        if (KernelControlSocketStatus_nid_no_patch(descriptor, command, static_cast<int>(argument), &result, &netError)) {
            return netError == 0 ? result : Fail(netError);
        }
        return File::Fcntl(descriptor, command, argument);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    std::lock_guard lock(socket->modeMutex);
    if (command == 1) {
        std::lock_guard registry(socketsMutex);
        return closeOnExecDescriptors.count(descriptor) != 0 ? 1 : 0;
    }
    if (command == 2) {
#ifdef _WIN32
        __builtin_sysv_va_list arguments;
        __builtin_sysv_va_start(arguments, command);
        const int flags = __builtin_va_arg(arguments, int);
        __builtin_sysv_va_end(arguments);
#else
        std::va_list arguments;
        va_start(arguments, command);
        const int flags = va_arg(arguments, int);
        va_end(arguments);
#endif
        std::lock_guard registry(socketsMutex);
        if ((flags & 1) != 0) closeOnExecDescriptors.insert(descriptor);
        else closeOnExecDescriptors.erase(descriptor);
        return 0;
    }
    if (command == 3) return 2 | (socket->options.nonblocking ? 4 : 0); // F_GETFL, O_RDWR
    if (command != 4) return Fail(22);
#ifdef _WIN32
    __builtin_sysv_va_list arguments;
    __builtin_sysv_va_start(arguments, command);
    const int flags = __builtin_va_arg(arguments, int);
    __builtin_sysv_va_end(arguments);
#else
    std::va_list arguments;
    va_start(arguments, command);
    const int flags = va_arg(arguments, int);
    va_end(arguments);
#endif
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
int APS5_VABI setsockopt_nid_postfix(int descriptor, int level, int option,
                                    const void* value, std::uint32_t length) {
    int netError = 0;
    if (!GuestSockets::IsOpen(descriptor) && KernelSetSocketOption_nid_no_patch(descriptor, level, option, value, length, &netError)) {
        return netError == 0 ? 0 : Fail(netError);
    }
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (level != 0xffff && level != 6) return Fail(42);
    std::lock_guard lock(socket->modeMutex);
    const auto native = static_cast<std::uintptr_t>(socket->value);
    const int error = level == 6 ? GuestSockets::SetTcpLevelOption(native, socket->family != GuestUnix && socket->type == 1, option, value, length)
                                 : GuestSockets::SetSocketLevelOption(native, socket->options, option, value, length);
    return error == 0 ? 0 : Fail(error);
}
int APS5_VABI getsockopt_nid_postfix(int descriptor, int level, int option,
                                    void* value, std::uint32_t* length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (level != 0xffff && level != 6) return Fail(42);
    if (!length) return Fail(22);
    std::lock_guard lock(socket->modeMutex);
    const auto native = static_cast<std::uintptr_t>(socket->value);
    const int error = level == 6 ? GuestSockets::GetTcpLevelOption(native, socket->family != GuestUnix && socket->type == 1, option, value, length)
                                 : GuestSockets::GetSocketLevelOption(native, socket->options, socket->type, option, value, length);
    return error == 0 ? 0 : Fail(error);
}
int APS5_VABI socket_nid_postfix(int family, int type, int protocol) {
    const bool nonblocking = (type & GuestNonBlocking) != 0;
    const int plainType = type & ~(GuestCloseOnExec | GuestNonBlocking);
    if (family != 2 && family != 28) return Fail(47);
    const int nativeType = plainType == 1 ? SOCK_STREAM : plainType == 2 ? SOCK_DGRAM : -1;
    const int expectedProtocol = plainType == 1 ? 6 : 17;
    if (nativeType == -1 || (protocol != 0 && protocol != expectedProtocol)) return Fail(43);
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
    try {
        auto socket = std::make_shared<Socket>(native, family, plainType);
        guard.value = Invalid;
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
        if (nonblocking && (SetNonBlocking(*first) || SetNonBlocking(*second))) return -1;
        const int firstDescriptor = Install(std::move(first));
        if (firstDescriptor < 0) return -1;
        const int secondDescriptor = Install(std::move(second));
        if (secondDescriptor < 0) {
            const int error = *__error_nid_postfix();
            GuestSockets::Close(firstDescriptor);
            return Fail(error);
        }
        descriptors[0] = firstDescriptor;
        descriptors[1] = secondDescriptor;
        return 0;
    } catch (const std::bad_alloc&) {
        return Fail(12);
    }
}
int APS5_VABI connect_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (socket->family != GuestUnix && socket->type == 2 && address && length >= 2 && static_cast<const unsigned char*>(address)[1] == 0
        && Disconnect(*socket) != 0)
        return -1;
    sockaddr_storage native{};
    socklen_t size;
    if (!Address(address, length, native, size)) return -1;
#ifdef _WIN32
    if (socket->type == 1) RefuseClosedLoopbackAtOnce_nid_no_patch(socket->value, reinterpret_cast<const sockaddr*>(&native));
#endif
    if (native.ss_family != (socket->family == 2 ? AF_INET : AF_INET6)) return Fail(47);
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
    return ::listen(socket->value, backlog) ? Fail(NativeError()) : 0;
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
        bool inheritNonblocking;
        {
            std::lock_guard modeLock(listener->modeMutex);
            inheritNonblocking = listener->options.nonblocking;
            accepted->options.noSignal = listener->options.noSignal;
            accepted->options.sendTimeout = listener->options.sendTimeout;
            accepted->options.receiveTimeout = listener->options.receiveTimeout;
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
    if (const int waited = WaitForTransfer(*socket, false, flags)) return Fail(waited);
    const auto result = ::recv(socket->value, static_cast<char*>(buffer), static_cast<int>(length),
        NativeReceiveFlags(flags));
#ifdef _WIN32
    if (result < 0 && socket->type == 2 && WSAGetLastError() == WSAEMSGSIZE) return static_cast<std::int64_t>(length);
#endif
    return result < 0 ? Fail(NativeError()) : result;
}
int APS5_VABI bind_nid_postfix(int descriptor, const void* address, std::uint32_t length) {
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    sockaddr_storage native{};
    socklen_t size;
    if (!Address(address, length, native, size)) return -1;
    return ::bind(socket->value, reinterpret_cast<sockaddr*>(&native), size) ? Fail(NativeError()) : 0;
}
int APS5_VABI shutdown_nid_postfix(int descriptor, int how) {
    const auto socket = Lookup(descriptor);
    if (!socket) return Fail(9);
    if (how < 0 || how > 2) return Fail(22);
    if (::shutdown(socket->value, how)) return Fail(NativeError());
    std::lock_guard lock(socket->modeMutex);
    if (how != 1) socket->receiveShut = true;
    if (how != 0) socket->sendShut = true;
    return 0;
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
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if (!argument) return Fail(14);
    if (request != 0x8004667e && request != 0x4004667f) return Fail(25);
    std::lock_guard lock(socket->modeMutex);
    unsigned long value = 0;
    if (request == 0x8004667e) value = *static_cast<int*>(argument) != 0;
#ifdef _WIN32
    const auto result = ioctlsocket(socket->value, request == 0x8004667e ? FIONBIO : FIONREAD, &value);
#else
    int nativeValue = static_cast<int>(value);
    const auto result = ::ioctl(socket->value, request == 0x8004667e ? FIONBIO : FIONREAD, &nativeValue);
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
    const auto socket = Lookup(descriptor);
    if (!socket) return -1;
    if ((flags & ~GuestSendFlags) != 0) return Fail(45);
    if (length > INT_MAX) return Fail(40);
    if (!buffer && length) return Fail(14);
    if (const int waited = WaitForTransfer(*socket, true, flags)) return Fail(waited);
    int result;
    if (!address) {
        if (addressLength != 0) return Fail(22);
        result = ::send(socket->value, static_cast<const char*>(buffer), static_cast<int>(length), SocketSendFlags(*socket, flags));
    } else {
        sockaddr_storage native{};
        socklen_t size;
        if (!Address(address, addressLength, native, size)) return -1;
        if (native.ss_family != (socket->family == 2 ? AF_INET : AF_INET6)) return Fail(47);
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
    sockaddr_storage native{};
    socklen_t size = sizeof(native);
    if (const int waited = WaitForTransfer(*socket, false, flags)) return Fail(waited);
    auto result = ::recvfrom(socket->value, static_cast<char*>(buffer), static_cast<int>(length),
        NativeReceiveFlags(flags), reinterpret_cast<sockaddr*>(&native), &size);
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
struct GuestIovec {
    void* base;
    std::uint64_t length;
};
struct GuestMsghdr {
    void* name;
    std::uint32_t nameLength;
    GuestIovec* iov;
    int iovLength;
    void* control;
    std::uint32_t controlLength;
    int flags;
};
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
std::int64_t APS5_VABI recvmsg_nid_postfix(int descriptor, GuestMsghdr* message, int flags) {
    const auto total = MessageLength(message);
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
    socklen_t size = sizeof(native);
    if (const int waited = WaitForTransfer(*socket, false, flags)) return Fail(waited);
    std::int64_t received = ::recvfrom(socket->value, buffer.data(), static_cast<int>(buffer.size()),
        NativeReceiveFlags(flags), reinterpret_cast<sockaddr*>(&native), &size);
    if (received < 0) {
#ifdef _WIN32
        if (!records || WSAGetLastError() != WSAEMSGSIZE) return Fail(NativeError());
        received = static_cast<std::int64_t>(buffer.size());
#else
        return Fail(NativeError());
#endif
    }
    message->flags = 0;
    if (received > total) {
        received = total;
        message->flags = 0x10;
    }
    std::size_t offset = 0;
    for (int i = 0; i < message->iovLength && offset < static_cast<std::size_t>(received); ++i) {
        const auto count = std::min<std::size_t>(message->iov[i].length, static_cast<std::size_t>(received) - offset);
        std::memcpy(message->iov[i].base, buffer.data() + offset, count);
        offset += count;
    }
    if (message->name && socket->family == GuestUnix) {
        if (socket->type == 1) message->nameLength = 0; else UnnamedAddress(message->name, &message->nameLength);
    } else if (message->name) {
        if (size > 0 && (native.ss_family == AF_INET || native.ss_family == AF_INET6)) {
            GuestAddress(native, message->name, &message->nameLength);
        } else {
            message->nameLength = 0;
        }
    }
    message->controlLength = 0;
    return received;
}
struct GuestSendfileHeaders {
    GuestIovec* headers;
    int headerCount;
    GuestIovec* trailers;
    int trailerCount;
};
static_assert(sizeof(GuestSendfileHeaders) == 32 && offsetof(GuestSendfileHeaders, trailers) == 16);
std::int64_t APS5_VABI sceKernelPread(int descriptor, void* buffer, std::size_t length, std::int64_t offset);
static bool SendfileIovecs(const GuestIovec* iov, int count) {
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
static int SendIovecs(const Socket& socket, const GuestIovec* iov, int count, std::int64_t& sent) {
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
    std::int64_t sent = 0;
    const auto finish = [&](int error) {
        if (sbytes) *sbytes = sent;
        return error ? Fail(error) : 0;
    };
    if (received < 0) return finish(received == -21 || received == -29 ? 22 : static_cast<int>(-received));
    const auto socket = Lookup(descriptor);
    if (!socket) return finish(9);
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
#ifdef _WIN32
            short revents = 0;
            if (!PipeReadiness(entry.descriptor, entry.events, revents))
#else
            if (!IsPipe(entry.descriptor, entry.events))
#endif
                throw std::runtime_error("poll: descriptor " + std::to_string(entry.descriptor) + " is neither a socket nor a pipe");
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
    if (native.empty() && pipes.empty()) {
        if (ready || timeout == 0) return ready;
        if (timeout < 0) throw std::runtime_error("poll: an infinite wait without sockets or pipes never returns");
        PreciseSleepUs(static_cast<unsigned long long>(timeout) * 1000);
        return 0;
    }
    const auto collect = [&](std::size_t i, bool pipe) {
        auto& entry = descriptors[owners[i]];
        for (const auto& flag : RequestFlags)
            if ((entry.events & flag.guest) && (native[i].revents & flag.native))
                entry.revents = static_cast<short>(entry.revents | flag.guest);
        for (const auto& flag : StatusFlags)
            if (native[i].revents & flag.native) entry.revents = static_cast<short>(entry.revents | flag.guest);
        if (pipe && (entry.revents & GuestPollError)) entry.revents = static_cast<short>((entry.revents & ~GuestPollError) | GuestPollHangUp);
        if (pipe && (entry.revents & GuestPollHangUp))
            entry.revents = static_cast<short>((entry.revents & ~GuestPollWritable) | (entry.events & GuestPollReadable));
        if (entry.revents) ++ready;
    };
#ifdef _WIN32
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout < 0 ? 0 : timeout);
    for (;;) {
        int pipeReady = 0;
        for (const auto index : pipes) {
            auto& entry = descriptors[index];
            PipeReadiness(entry.descriptor, entry.events, entry.revents);
            if (entry.revents) ++pipeReady;
        }
        int slice = 0;
        if (!ready && !pipeReady) {
            if (pipes.empty()) slice = timeout;
            else if (timeout < 0) slice = 1;
            else {
                const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
                slice = static_cast<int>(std::clamp<long long>(left, 0, 1));
            }
        }
        if (!native.empty()) {
            for (auto& request : native) request.revents = 0;
            if (WSAPoll(native.data(), static_cast<ULONG>(native.size()), slice) < 0) return Fail(NativeError());
        } else if (slice > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
        }
        int socketReady = 0;
        for (const auto& request : native)
            if (request.revents) ++socketReady;
        const bool expired = timeout >= 0 && std::chrono::steady_clock::now() >= deadline;
        if (ready || pipeReady || socketReady || expired || pipes.empty()) {
            ready += pipeReady;
            for (std::size_t i = 0; i < native.size(); ++i) collect(i, false);
            return ready;
        }
    }
#else
    const int wait = ready ? 0 : timeout;
    const int result = ::poll(native.data(), static_cast<nfds_t>(native.size()), wait);
    if (result < 0) return Fail(NativeError());
    for (std::size_t i = 0; i < native.size(); ++i) collect(i, i >= socketCount);
    return ready;
#endif
}
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif
#include "prx/libkernel/Socket/include/SocketError.hpp"
#include "prx/libkernel/Socket/include/SocketOptions.hpp"
#include <algorithm>
#include <cstring>

namespace GuestSockets {
namespace {

constexpr int GuestAcceptConnection = 0x2;
constexpr int GuestReuseAddress = 0x4;
constexpr int GuestKeepAlive = 0x8;
constexpr int GuestDontRoute = 0x10;
constexpr int GuestBroadcast = 0x20;
constexpr int GuestLinger = 0x80;
constexpr int GuestOobInline = 0x100;
constexpr int GuestNoSigpipe = 0x800;
constexpr int GuestSendBuffer = 0x1001;
constexpr int GuestReceiveBuffer = 0x1002;
constexpr int GuestSendTimeout = 0x1005;
constexpr int GuestReceiveTimeout = 0x1006;
constexpr int GuestError = 0x1007;
constexpr int GuestType = 0x1008;
constexpr int GuestNonBlocking = 0x1200;
constexpr int GuestTcpNoDelay = 1;
constexpr int GuestTcpNoDelayFlag = 4;
constexpr int GuestIpv6Only = 27;

constexpr int ErrorInvalid = 22;
constexpr int ErrorDomain = 33;
constexpr int ErrorNoProtocolOption = 42;
constexpr int MaximumLinger = 65535;
constexpr std::int64_t LongestTimeout = 0x7fffffffffffffffLL;

struct GuestLingerValue {
    std::int32_t enabled;
    std::int32_t seconds;
};

struct GuestTimeval {
    std::int64_t seconds;
    std::int64_t microseconds;
};

std::int64_t TimeoutOf(const GuestTimeval& time) {
    if (time.seconds > INT32_MAX) return LongestTimeout;
    return (time.seconds << 32) + static_cast<std::int64_t>((static_cast<std::uint64_t>(time.microseconds) * ((std::uint64_t{1} << 63) / 500000)) >> 32);
}

GuestTimeval TimevalOf(std::int64_t timeout) {
    return {timeout >> 32, (1000000 * static_cast<std::int64_t>(static_cast<std::uint32_t>(timeout))) >> 32};
}

#ifdef _WIN32
using NativeHandle = SOCKET;
using NativeOptionValue = const char*;
using NativeOptionOutput = char*;
int HostError() { return GuestErrorFromHost(WSAGetLastError()); }
#else
using NativeHandle = int;
using NativeOptionValue = const void*;
using NativeOptionOutput = void*;
int HostError() { return GuestErrorFromHost(errno); }
#endif

int NativeBoolean(int option) {
    switch (option) {
        case GuestReuseAddress: return SO_REUSEADDR;
        case GuestKeepAlive: return SO_KEEPALIVE;
        case GuestDontRoute: return SO_DONTROUTE;
        case GuestBroadcast: return SO_BROADCAST;
        case GuestOobInline: return SO_OOBINLINE;
        default: return -1;
    }
}

int Copy(void* output, std::uint32_t* length, const void* value, std::uint32_t size) {
    const std::uint32_t count = std::min(*length, size);
    if (count != 0 && !output) return ErrorInvalid;
    if (count != 0) std::memcpy(output, value, count);
    *length = count;
    return 0;
}

}

int SetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int option, const void* value, std::uint32_t length) {
    const auto handle = static_cast<NativeHandle>(native);
    if (option == GuestLinger) {
        if (length < sizeof(GuestLingerValue)) return ErrorInvalid;
        if (!value) return ErrorInvalid;
        GuestLingerValue guest;
        std::memcpy(&guest, value, sizeof(guest));
        if (guest.seconds < 0 || guest.seconds > MaximumLinger) return ErrorDomain;
#ifdef _WIN32
        linger request{};
        request.l_onoff = guest.enabled != 0 ? 1 : 0;
        request.l_linger = static_cast<u_short>(guest.seconds);
#else
        linger request{};
        request.l_onoff = guest.enabled != 0 ? 1 : 0;
        request.l_linger = guest.seconds;
#endif
        if (::setsockopt(handle, SOL_SOCKET, SO_LINGER, reinterpret_cast<NativeOptionValue>(&request), sizeof(request))) return HostError();
        return 0;
    }
    if (option == GuestSendTimeout || option == GuestReceiveTimeout) {
        if (length < sizeof(GuestTimeval)) return ErrorInvalid;
        if (!value) return ErrorInvalid;
        GuestTimeval time;
        std::memcpy(&time, value, sizeof(time));
        if (time.seconds < 0 || time.microseconds < 0 || time.microseconds >= 1000000) return ErrorDomain;
        (option == GuestSendTimeout ? state.sendTimeout : state.receiveTimeout) = TimeoutOf(time);
        return 0;
    }
    if (option == GuestNoSigpipe || option == GuestNonBlocking) {
        if (length < sizeof(int)) return ErrorInvalid;
        if (!value) return ErrorInvalid;
        int number;
        std::memcpy(&number, value, sizeof(number));
        if (option == GuestNoSigpipe) {
            state.noSignal = number != 0;
            return 0;
        }
#ifdef _WIN32
        unsigned long enabled = number != 0;
        if (ioctlsocket(handle, FIONBIO, &enabled)) return HostError();
#else
        int enabled = number != 0;
        if (::ioctl(handle, FIONBIO, &enabled)) return HostError();
#endif
        state.nonblocking = enabled != 0;
        return 0;
    }
    const bool native_boolean = NativeBoolean(option) != -1;
    if (native_boolean || option == GuestSendBuffer || option == GuestReceiveBuffer) {
        if (length < sizeof(int)) return ErrorInvalid;
        if (!value) return ErrorInvalid;
        int number;
        std::memcpy(&number, value, sizeof(number));
        if (option == GuestSendBuffer || option == GuestReceiveBuffer) {
            if (number < 1) return ErrorInvalid;
            const int nativeOption = option == GuestSendBuffer ? SO_SNDBUF : SO_RCVBUF;
            if (::setsockopt(handle, SOL_SOCKET, nativeOption, reinterpret_cast<NativeOptionValue>(&number), sizeof(number))) return HostError();
            return 0;
        }
        const int enabled = number != 0 ? 1 : 0;
        if (::setsockopt(handle, SOL_SOCKET, NativeBoolean(option), reinterpret_cast<NativeOptionValue>(&enabled), sizeof(enabled))) return HostError();
        return 0;
    }
    return ErrorNoProtocolOption;
}

int GetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int type, int option, void* value, std::uint32_t* length) {
    const auto handle = static_cast<NativeHandle>(native);
    if (option == GuestLinger) {
        linger current{};
        socklen_t size = sizeof(current);
        if (::getsockopt(handle, SOL_SOCKET, SO_LINGER, reinterpret_cast<NativeOptionOutput>(&current), &size)) return HostError();
        const GuestLingerValue guest{current.l_onoff != 0 ? GuestLinger : 0, static_cast<std::int32_t>(current.l_linger)};
        return Copy(value, length, &guest, sizeof(guest));
    }
    if (option == GuestSendTimeout || option == GuestReceiveTimeout) {
        const GuestTimeval time = TimevalOf(option == GuestSendTimeout ? state.sendTimeout : state.receiveTimeout);
        return Copy(value, length, &time, sizeof(time));
    }
    int number = 0;
    if (const int nativeOption = NativeBoolean(option); nativeOption != -1) {
        int enabled = 0;
        socklen_t size = sizeof(enabled);
        if (::getsockopt(handle, SOL_SOCKET, nativeOption, reinterpret_cast<NativeOptionOutput>(&enabled), &size)) return HostError();
        number = enabled != 0 ? option : 0;
    } else if (option == GuestAcceptConnection) {
        int listening = 0;
        socklen_t size = sizeof(listening);
        if (::getsockopt(handle, SOL_SOCKET, SO_ACCEPTCONN, reinterpret_cast<NativeOptionOutput>(&listening), &size)) return HostError();
        number = listening != 0 ? GuestAcceptConnection : 0;
    } else if (option == GuestType) {
        number = type;
    } else if (option == GuestNoSigpipe) {
        number = state.noSignal ? GuestNoSigpipe : 0;
    } else if (option == GuestNonBlocking) {
        number = state.nonblocking ? 1 : 0;
    } else if (option == GuestError) {
        int pending = 0;
        socklen_t size = sizeof(pending);
        if (::getsockopt(handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<NativeOptionOutput>(&pending), &size)) return HostError();
#ifdef _WIN32
        if (pending != 0 && pending == state.reportedError) pending = 0;
        else if (pending != 0) state.reportedError = pending;
#endif
        number = pending == 0 ? 0 : GuestErrorFromHost(pending);
    } else if (option == GuestSendBuffer || option == GuestReceiveBuffer) {
        socklen_t size = sizeof(number);
        if (::getsockopt(handle, SOL_SOCKET, option == GuestSendBuffer ? SO_SNDBUF : SO_RCVBUF, reinterpret_cast<NativeOptionOutput>(&number), &size)) return HostError();
    } else {
        return ErrorNoProtocolOption;
    }
    return Copy(value, length, &number, sizeof(number));
}

int SetTcpLevelOption(std::uintptr_t native, bool tcp, int option, const void* value, std::uint32_t length) {
    if (!tcp) return ErrorInvalid;
    if (option != GuestTcpNoDelay) return ErrorNoProtocolOption;
    if (length < sizeof(int)) return ErrorInvalid;
    if (!value) return ErrorInvalid;
    int number;
    std::memcpy(&number, value, sizeof(number));
    const int enabled = number != 0 ? 1 : 0;
    if (::setsockopt(static_cast<NativeHandle>(native), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<NativeOptionValue>(&enabled), sizeof(enabled))) return HostError();
    return 0;
}

int GetTcpLevelOption(std::uintptr_t native, bool tcp, int option, void* value, std::uint32_t* length) {
    if (!tcp) return ErrorInvalid;
    if (option != GuestTcpNoDelay) return ErrorNoProtocolOption;
    int enabled = 0;
    socklen_t size = sizeof(enabled);
    if (::getsockopt(static_cast<NativeHandle>(native), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<NativeOptionOutput>(&enabled), &size)) return HostError();
    const int number = enabled != 0 ? GuestTcpNoDelayFlag : 0;
    return Copy(value, length, &number, sizeof(number));
}

int SetIpv6LevelOption(std::uintptr_t native, bool ipv6, int option, const void* value, std::uint32_t length) {
    if (!ipv6) return ErrorInvalid;
    if (option != GuestIpv6Only) return ErrorNoProtocolOption;
    if (length != sizeof(int)) return ErrorInvalid;
    if (!value) return ErrorInvalid;
    int number;
    std::memcpy(&number, value, sizeof(number));
    const int enabled = number != 0 ? 1 : 0;
    if (::setsockopt(static_cast<NativeHandle>(native), IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<NativeOptionValue>(&enabled), sizeof(enabled))) return HostError();
    return 0;
}

int GetIpv6LevelOption(std::uintptr_t native, bool ipv6, int option, void* value, std::uint32_t* length) {
    if (!ipv6) return ErrorInvalid;
    if (option != GuestIpv6Only) return ErrorNoProtocolOption;
    int enabled = 0;
    socklen_t size = sizeof(enabled);
    if (::getsockopt(static_cast<NativeHandle>(native), IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<NativeOptionOutput>(&enabled), &size)) return HostError();
    const int number = enabled != 0 ? 1 : 0;
    return Copy(value, length, &number, sizeof(number));
}

}

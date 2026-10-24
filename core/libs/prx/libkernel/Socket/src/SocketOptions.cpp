#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#ifdef __APPLE__
#include <libproc.h>
#include <unistd.h>
#endif
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
constexpr int GuestSendLowWater = 0x1003;
constexpr int GuestReceiveLowWater = 0x1004;
constexpr int GuestSendTimeout = 0x1005;
constexpr int GuestReceiveTimeout = 0x1006;
constexpr int GuestError = 0x1007;
constexpr int GuestType = 0x1008;
constexpr int GuestNonBlocking = 0x1200;
constexpr int GuestNonBlockingState = 0x100;
constexpr int GuestUnix = 1;
constexpr int GuestTcpNoDelay = 1;
constexpr int GuestTcpNoDelayFlag = 4;
constexpr int GuestIpv6Only = 27;
constexpr int GuestIpTypeOfService = 3;
constexpr int GuestIpTimeToLive = 4;
constexpr int GuestIpMulticastTimeToLive = 10;
constexpr int GuestIpMulticastLoop = 11;

constexpr int ErrorInvalid = 22;
constexpr int ErrorDomain = 33;
constexpr int ErrorNoProtocolOption = 42;
constexpr int ErrorNoBuffers = 55;
constexpr int MaximumReservation = 5592405;
constexpr int MaximumBuffer = 524288;
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

void EnsureBuffers(SocketOptionState& state, int family, int type) {
    if (state.sendBuffer != 0) return;
    if (family == GuestUnix) {
        state.sendBuffer = type == 2 ? 2048 : 8192;
        state.receiveBuffer = type == 2 ? 4096 : 8192;
    } else {
        state.sendBuffer = type == 1 ? 32768 : 9216;
        state.receiveBuffer = type == 1 ? 65536 : 42080;
    }
    state.sendLowWater = std::min(2048, state.sendBuffer);
    state.receiveLowWater = 1;
}

int Copy(void* output, std::uint32_t* length, const void* value, std::uint32_t size) {
    const std::uint32_t count = output ? std::min(*length, size) : 0;
    if (count != 0) std::memcpy(output, value, count);
    *length = count;
    return 0;
}

}

int SetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int family, int type, int option, const void* value, std::uint32_t length) {
    const auto handle = static_cast<NativeHandle>(native);
    if (option == GuestLinger) {
        if (length < sizeof(GuestLingerValue)) return ErrorInvalid;
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
        GuestTimeval time;
        std::memcpy(&time, value, sizeof(time));
        if (time.seconds < 0 || time.microseconds < 0 || time.microseconds >= 1000000) return ErrorDomain;
        (option == GuestSendTimeout ? state.sendTimeout : state.receiveTimeout) = TimeoutOf(time);
        return 0;
    }
    if (option == GuestNoSigpipe || option == GuestNonBlocking) {
        if (length < sizeof(int)) return ErrorInvalid;
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
    const bool buffer = option == GuestSendBuffer || option == GuestReceiveBuffer;
    const bool lowWater = option == GuestSendLowWater || option == GuestReceiveLowWater;
    if (native_boolean || buffer || lowWater) {
        if (length < sizeof(int)) return ErrorInvalid;
        int number;
        std::memcpy(&number, value, sizeof(number));
        if (buffer || lowWater) {
            if (number < 1) return ErrorInvalid;
            EnsureBuffers(state, family, type);
            const bool send = option == GuestSendBuffer || option == GuestSendLowWater;
            int& size = send ? state.sendBuffer : state.receiveBuffer;
            int& mark = send ? state.sendLowWater : state.receiveLowWater;
            if (lowWater) {
                mark = std::min(number, size);
                return 0;
            }
            if (number > MaximumReservation) return ErrorNoBuffers;
            const int nativeOption = send ? SO_SNDBUF : SO_RCVBUF;
            if (::setsockopt(handle, SOL_SOCKET, nativeOption, reinterpret_cast<NativeOptionValue>(&number), sizeof(number))) return HostError();
            size = std::min(number, MaximumBuffer);
            mark = std::min(mark, size);
            return 0;
        }
        const int enabled = number != 0 ? 1 : 0;
        if (::setsockopt(handle, SOL_SOCKET, NativeBoolean(option), reinterpret_cast<NativeOptionValue>(&enabled), sizeof(enabled))) return HostError();
        return 0;
    }
    return ErrorNoProtocolOption;
}

bool NativeListening(std::uintptr_t native, bool& listening) {
#ifdef __APPLE__
    socket_fdinfo info{};
    if (proc_pidfdinfo(getpid(), static_cast<int>(native), PROC_PIDFDSOCKETINFO, &info, sizeof(info)) != static_cast<int>(sizeof(info))) return false;
    listening = (info.psi.soi_options & SO_ACCEPTCONN) != 0;
#else
    int accepting = 0;
    socklen_t size = sizeof(accepting);
    if (::getsockopt(static_cast<NativeHandle>(native), SOL_SOCKET, SO_ACCEPTCONN, reinterpret_cast<NativeOptionOutput>(&accepting), &size)) return false;
    listening = accepting != 0;
#endif
    return true;
}

int GetSocketLevelOption(std::uintptr_t native, SocketOptionState& state, int family, int type, int option, void* value, std::uint32_t* length) {
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
        bool listening = false;
        if (!NativeListening(native, listening)) return HostError();
        number = listening ? GuestAcceptConnection : 0;
    } else if (option == GuestType) {
        number = type;
    } else if (option == GuestNoSigpipe) {
        number = state.noSignal ? GuestNoSigpipe : 0;
    } else if (option == GuestNonBlocking) {
        number = state.nonblocking ? GuestNonBlockingState : 0;
    } else if (option == GuestError) {
        int pending = 0;
        socklen_t size = sizeof(pending);
        if (::getsockopt(handle, SOL_SOCKET, SO_ERROR, reinterpret_cast<NativeOptionOutput>(&pending), &size)) return HostError();
#ifdef _WIN32
        if (pending != 0 && pending == state.reportedError) pending = 0;
        else if (pending != 0) state.reportedError = pending;
#endif
        number = pending == 0 ? 0 : GuestErrorFromHost(pending);
    } else if (option == GuestSendBuffer || option == GuestReceiveBuffer || option == GuestSendLowWater || option == GuestReceiveLowWater) {
        EnsureBuffers(state, family, type);
        number = option == GuestSendBuffer ? state.sendBuffer : option == GuestReceiveBuffer ? state.receiveBuffer
            : option == GuestSendLowWater ? state.sendLowWater : state.receiveLowWater;
    } else {
        return ErrorNoProtocolOption;
    }
    return Copy(value, length, &number, sizeof(number));
}

int SetTcpLevelOption(std::uintptr_t native, bool tcp, int option, const void* value, std::uint32_t length) {
    if (!tcp) return ErrorInvalid;
    if (option != GuestTcpNoDelay) return ErrorNoProtocolOption;
    if (length < sizeof(int)) return ErrorInvalid;
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

int SetIpLevelOption(std::uintptr_t native, SocketOptionState& state, bool ipv4, int option, const void* value, std::uint32_t length) {
    if (!ipv4) return ErrorInvalid;
    const auto handle = static_cast<NativeHandle>(native);
    if (option == GuestIpTimeToLive || option == GuestIpTypeOfService) {
        if (length < sizeof(int)) return ErrorInvalid;
        int number;
        std::memcpy(&number, value, sizeof(number));
        const int stored = number & 0xff;
        if (option == GuestIpTypeOfService) {
            state.ipTypeOfService = stored;
            return 0;
        }
        if (stored != 0 && ::setsockopt(handle, IPPROTO_IP, IP_TTL, reinterpret_cast<NativeOptionValue>(&stored), sizeof(stored))) return HostError();
        state.ipTimeToLive = stored;
        return 0;
    }
    if (option == GuestIpMulticastTimeToLive || option == GuestIpMulticastLoop) {
        unsigned int number;
        if (length == 1) {
            unsigned char byte;
            std::memcpy(&byte, value, 1);
            number = byte;
        } else {
            if (length < sizeof(number)) return ErrorInvalid;
            std::memcpy(&number, value, sizeof(number));
            if (option == GuestIpMulticastTimeToLive && number > 255) return ErrorInvalid;
        }
        const int host = option == GuestIpMulticastLoop ? ((number & 0xff) != 0 ? 1 : 0) : static_cast<int>(number);
        const int nativeOption = option == GuestIpMulticastLoop ? IP_MULTICAST_LOOP : IP_MULTICAST_TTL;
        if (::setsockopt(handle, IPPROTO_IP, nativeOption, reinterpret_cast<NativeOptionValue>(&host), sizeof(host))) return HostError();
        if (option == GuestIpMulticastLoop) state.multicastLoop = host != 0;
        else state.multicastTimeToLive = host;
        return 0;
    }
    return ErrorNoProtocolOption;
}

int GetIpLevelOption(SocketOptionState& state, bool ipv4, int option, void* value, std::uint32_t* length) {
    if (!ipv4) return ErrorInvalid;
    int number;
    if (option == GuestIpTimeToLive) number = state.ipTimeToLive;
    else if (option == GuestIpTypeOfService) number = state.ipTypeOfService;
    else if (option == GuestIpMulticastTimeToLive) number = state.multicastTimeToLive;
    else if (option == GuestIpMulticastLoop) number = state.multicastLoop ? 1 : 0;
    else return ErrorNoProtocolOption;
    if ((option == GuestIpMulticastTimeToLive || option == GuestIpMulticastLoop) && *length == 1) {
        const auto byte = static_cast<unsigned char>(number);
        return Copy(value, length, &byte, 1);
    }
    return Copy(value, length, &number, sizeof(number));
}

int SetIpv6LevelOption(std::uintptr_t native, bool ipv6, int option, const void* value, std::uint32_t length) {
    if (!ipv6) return ErrorInvalid;
    if (option != GuestIpv6Only) return ErrorNoProtocolOption;
    if (length != sizeof(int)) return ErrorInvalid;
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
